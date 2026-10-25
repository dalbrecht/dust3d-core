// SPDX-License-Identifier: MIT
#include "WorkerUv.h"
#include "WorkerWire.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string_view>
#include <unordered_set>

namespace dust3d {
namespace worker {
    namespace {
        static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559 && sizeof(size_t) <= 8, "UV wire requires binary64 and size_t no wider than uint64.");
        static_assert(maximumPayloadBytes <= std::numeric_limits<size_t>::max(), "The bounded wire span must fit size_t.");
        [[noreturn]] void reject(UvPayloadErrorCode code, const char* message) { throw UvPayloadError(code, message); }
        void charge(uint64_t& total, uint64_t count, uint64_t width, uint64_t maximum)
        {
            if (total > maximum || count > (maximum - total) / width)
                reject(UvPayloadErrorCode::limitExceeded, "Worker UV payload exceeds a configured quota.");
            total += count * width;
        }
        void validateLimits(const UvPayloadLimits& limits)
        {
            for (auto value : { limits.maximumWireBytes, limits.maximumDecodedBytes, limits.maximumTextBytes, limits.maximumKeyBytes })
                if (!value || value > maximumPayloadBytes)
                    reject(UvPayloadErrorCode::invalidLimits, "Worker UV byte limits are outside their supported range.");
            for (auto value : { limits.maximumTriangles, limits.maximumComponents, limits.maximumCharts, limits.maximumGradients })
                if (!value || value > maximumUvRecords)
                    reject(UvPayloadErrorCode::invalidLimits, "Worker UV count limits are outside their supported range.");
        }
        struct Layout {
            size_t bytes;
            uint64_t logicalBytes;
            size_t charts;
            size_t gradients;
            size_t keys;
        };
        Layout plan(uint64_t triangles, uint64_t components, uint64_t charts, uint64_t gradients, uint64_t text, uint64_t expected, const UvPayloadLimits& limits)
        {
            validateLimits(limits);
            if (!expected || expected > maximumUvRecords)
                reject(UvPayloadErrorCode::invalidInput, "Worker UV geometry count is unsupported.");
            if (triangles != expected)
                reject(UvPayloadErrorCode::invalidLayout, "Worker UV triangle count does not match its geometry.");
            if (triangles > limits.maximumTriangles || components > limits.maximumComponents || charts > limits.maximumCharts || gradients > limits.maximumGradients || text > limits.maximumTextBytes)
                reject(UvPayloadErrorCode::limitExceeded, "Worker UV counts or text exceed their configured limits.");
            uint64_t wire = uvPrefixBytes;
            charge(wire, triangles, uvTriangleBytes, limits.maximumWireBytes);
            const auto chartOffset = wire;
            charge(wire, charts, uvChartBytes, limits.maximumWireBytes);
            const auto gradientOffset = wire;
            charge(wire, gradients, uvGradientBytes, limits.maximumWireBytes);
            const auto keyOffset = wire;
            charge(wire, components, 4, limits.maximumWireBytes);
            charge(wire, text, 1, limits.maximumWireBytes);
            uint64_t logical = 0;
            charge(logical, triangles, sizeof(UvTriangle), limits.maximumDecodedBytes);
            charge(logical, charts, sizeof(UvChart), limits.maximumDecodedBytes);
            charge(logical, gradients, sizeof(UvGradient), limits.maximumDecodedBytes);
            charge(logical, components, sizeof(std::string), limits.maximumDecodedBytes);
            charge(logical, text, 1, limits.maximumDecodedBytes);
            return { static_cast<size_t>(wire), logical, static_cast<size_t>(chartOffset), static_cast<size_t>(gradientOffset), static_cast<size_t>(keyOffset) };
        }
        bool validText(std::string_view text)
        {
            for (size_t i = 0; i < text.size();) {
                const auto first = static_cast<uint8_t>(text[i]);
                if (first < 128) {
                    if ((first < 32 && first != 9 && first != 10 && first != 13) || first == 127)
                        return false;
                    ++i;
                    continue;
                }
                const size_t width = first >= 0xc2 && first <= 0xdf ? 2 : first >= 0xe0 && first <= 0xef ? 3
                    : first >= 0xf0 && first <= 0xf4                                                     ? 4
                                                                                                         : 0;
                if (!width || width > text.size() - i)
                    return false;
                uint32_t scalar = first & ((1U << (7 - width)) - 1);
                for (size_t j = 1; j < width; ++j) {
                    const auto next = static_cast<uint8_t>(text[i + j]);
                    if ((next & 0xc0) != 0x80)
                        return false;
                    scalar = (scalar << 6) | (next & 0x3f);
                }
                if (scalar < (width == 2 ? 0x80U : width == 3 ? 0x800U
                                                              : 0x10000U)
                    || scalar > 0x10ffff || (scalar >= 0xd800 && scalar <= 0xdfff))
                    return false;
                i += width;
            }
            return true;
        }
        bool imageIdentity(std::string_view text)
        {
            if (text.empty())
                return true;
            if (text.size() == 38 && text.front() == '{' && text.back() == '}')
                text = text.substr(1, 36);
            if (text.size() != 36)
                return false;
            bool nonzero = false;
            for (size_t i = 0; i < text.size(); ++i) {
                const auto c = text[i];
                if (i == 8 || i == 13 || i == 18 || i == 23) {
                    if (c != '-')
                        return false;
                } else {
                    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
                        return false;
                    nonzero = nonzero || c != '0';
                }
            }
            return nonzero;
        }
        bool unit(double value) { return std::isfinite(value) && value >= 0 && value <= 1; }
        void association(UvAssociation kind, uint64_t index, uint64_t components)
        {
            if (kind == UvAssociation::component) {
                if (index < components)
                    return;
            } else if ((kind == UvAssociation::unknown || kind == UvAssociation::ambiguous) && index == noUvComponent)
                return;
            reject(UvPayloadErrorCode::invalidMetadata, "Worker UV component association is invalid.");
        }
        void validateTriangle(const UvTriangle& value, uint64_t components, uint64_t charts)
        {
            association(value.association, value.componentIndex, components);
            if (value.known > 1 || (value.known && !charts))
                reject(UvPayloadErrorCode::invalidMetadata, "Worker UV mapping flag is invalid.");
            for (const auto& corner : value.uv)
                for (auto scalar : corner)
                    if (!unit(scalar) || (!value.known && scalar != 0))
                        reject(UvPayloadErrorCode::invalidMetadata, "Worker UV coordinates are invalid.");
        }
        void validateChart(const UvChart& value, uint64_t components)
        {
            association(value.association, value.componentIndex, components);
            if (!unit(value.left) || !unit(value.top) || !unit(value.width) || !unit(value.height) || value.left + value.width > 1 + 1e-9 || value.top + value.height > 1 + 1e-9 || value.flipped > 1)
                reject(UvPayloadErrorCode::invalidMetadata, "Worker UV chart bounds or rotation are invalid.");
            for (auto scalar : value.color)
                if (!unit(scalar))
                    reject(UvPayloadErrorCode::invalidMetadata, "Worker UV chart color is invalid.");
            if (!imageIdentity(value.imageID))
                reject(UvPayloadErrorCode::invalidText, "Worker UV chart image identity is invalid.");
        }
        void validateGradient(const UvGradient& value, uint64_t charts)
        {
            if (value.chartIndex >= charts || !value.resolution)
                reject(UvPayloadErrorCode::invalidMetadata, "Worker UV gradient reference or resolution is invalid.");
            for (const auto* colors : { &value.largeColor, &value.smallColor })
                for (auto scalar : *colors)
                    if (!unit(scalar))
                        reject(UvPayloadErrorCode::invalidMetadata, "Worker UV gradient color is invalid.");
        }
        void validateTexture(double value, uint64_t charts)
        {
            if (!std::isfinite(value) || value < 0 || (charts && value == 0))
                reject(UvPayloadErrorCode::invalidMetadata, "Worker UV packed texture size is invalid.");
        }
        void writeDouble(uint8_t* target, double value)
        {
            uint64_t bits;
            std::memcpy(&bits, &value, 8);
            detail::writeUnsigned(target, bits, 8);
        }
        double readDouble(const uint8_t* source)
        {
            const uint64_t bits = detail::readUnsigned(source, 8);
            if ((bits & 0x7ff0000000000000ULL) == 0x7ff0000000000000ULL)
                reject(UvPayloadErrorCode::invalidMetadata, "Worker UV scalars must be finite.");
            double value;
            std::memcpy(&value, &bits, 8);
            return value;
        }
        void zeros(const uint8_t* source, size_t count)
        {
            for (size_t i = 0; i < count; ++i)
                if (source[i])
                    reject(UvPayloadErrorCode::invalidLayout, "Worker UV reserved storage must be zero.");
        }
        UvTriangle readTriangle(const uint8_t* source)
        {
            UvTriangle value;
            for (auto& corner : value.uv)
                for (auto& scalar : corner) {
                    scalar = readDouble(source);
                    source += 8;
                }
            value.known = source[0];
            value.association = static_cast<UvAssociation>(source[1]);
            zeros(source + 2, 6);
            value.componentIndex = detail::readUnsigned(source + 8, 8);
            return value;
        }
        UvChart readChart(const uint8_t* source)
        {
            UvChart value;
            value.left = readDouble(source);
            value.top = readDouble(source + 8);
            value.width = readDouble(source + 16);
            value.height = readDouble(source + 24);
            for (size_t i = 0; i < 4; ++i)
                value.color[i] = readDouble(source + 32 + i * 8);
            value.flipped = source[64];
            value.association = static_cast<UvAssociation>(source[65]);
            zeros(source + 66, 6);
            value.componentIndex = detail::readUnsigned(source + 72, 8);
            const size_t length = source[80];
            if (length > 38)
                reject(UvPayloadErrorCode::invalidLayout, "Worker UV image text length is invalid.");
            zeros(source + 81 + length, 39 - length);
            value.imageID.assign(reinterpret_cast<const char*>(source + 81), length);
            return value;
        }
        UvGradient readGradient(const uint8_t* source)
        {
            UvGradient value;
            value.chartIndex = detail::readUnsigned(source, 8);
            for (size_t i = 0; i < 4; ++i) {
                value.largeColor[i] = readDouble(source + 8 + i * 8);
                value.smallColor[i] = readDouble(source + 40 + i * 8);
            }
            value.resolution = detail::readUnsigned(source + 72, 8);
            return value;
        }
        void checkKey(std::string_view key, const UvPayloadLimits& limits, std::unordered_set<std::string_view>& keys)
        {
            if (key.size() > limits.maximumKeyBytes)
                reject(UvPayloadErrorCode::limitExceeded, "Worker UV component key exceeds its quota.");
            if (!validText(key))
                reject(UvPayloadErrorCode::invalidText, "Worker UV component key is not valid text.");
            if (!keys.emplace(key).second)
                reject(UvPayloadErrorCode::invalidMetadata, "Worker UV component keys are duplicated.");
        }
    }
    UvPayloadError::UvPayloadError(UvPayloadErrorCode code, const char* message)
        : std::runtime_error(message)
        , m_code(code)
    {
    }
    void validateUvPayloadLimits(const UvPayloadLimits& limits)
    {
        validateLimits(limits);
    }
    std::vector<uint8_t> encodeUvPayload(const UvPayload& payload, uint64_t expected, const UvPayloadLimits& limits)
    {
        validateLimits(limits);
        // Reject impossible record counts before scanning any caller-owned text.
        (void)plan(payload.triangles.size(), payload.components.size(), payload.charts.size(), payload.gradients.size(), 0, expected, limits);
        uint64_t text = 0;
        for (const auto& key : payload.components)
            charge(text, key.size(), 1, limits.maximumTextBytes);
        const auto layout = plan(payload.triangles.size(), payload.components.size(), payload.charts.size(), payload.gradients.size(), text, expected, limits);
        validateTexture(payload.packedTextureSize, payload.charts.size());
        uint64_t logical = layout.logicalBytes;
        uint64_t allText = text;
        for (const auto& triangle : payload.triangles)
            validateTriangle(triangle, payload.components.size(), payload.charts.size());
        for (const auto& chart : payload.charts) {
            validateChart(chart, payload.components.size());
            charge(allText, chart.imageID.size(), 1, limits.maximumTextBytes);
            charge(logical, chart.imageID.size(), 1, limits.maximumDecodedBytes);
        }
        for (const auto& gradient : payload.gradients)
            validateGradient(gradient, payload.charts.size());
        std::unordered_set<std::string_view> keys;
        for (const auto& key : payload.components)
            checkKey(key, limits, keys);
        std::vector<uint8_t> bytes(layout.bytes, 0);
        detail::writeUnsigned(bytes.data(), 1, 2);
        detail::writeUnsigned(bytes.data() + 2, uvPrefixBytes, 2);
        for (auto item : { std::pair<size_t, uint64_t> { 8, payload.triangles.size() }, { 16, payload.components.size() }, { 24, payload.charts.size() }, { 32, payload.gradients.size() }, { 40, text } })
            detail::writeUnsigned(bytes.data() + item.first, item.second, 8);
        writeDouble(bytes.data() + 48, payload.packedTextureSize);
        size_t offset = uvPrefixBytes;
        for (const auto& triangle : payload.triangles) {
            auto* out = bytes.data() + offset;
            for (const auto& corner : triangle.uv)
                for (auto value : corner) {
                    writeDouble(out, value);
                    out += 8;
                }
            out[0] = triangle.known;
            out[1] = static_cast<uint8_t>(triangle.association);
            detail::writeUnsigned(out + 8, triangle.componentIndex, 8);
            offset += uvTriangleBytes;
        }
        for (const auto& chart : payload.charts) {
            auto* out = bytes.data() + offset;
            for (auto value : { chart.left, chart.top, chart.width, chart.height }) {
                writeDouble(out, value);
                out += 8;
            }
            for (auto value : chart.color) {
                writeDouble(out, value);
                out += 8;
            }
            out[0] = chart.flipped;
            out[1] = static_cast<uint8_t>(chart.association);
            detail::writeUnsigned(out + 8, chart.componentIndex, 8);
            out[16] = static_cast<uint8_t>(chart.imageID.size());
            std::memcpy(out + 17, chart.imageID.data(), chart.imageID.size());
            offset += uvChartBytes;
        }
        for (const auto& gradient : payload.gradients) {
            auto* out = bytes.data() + offset;
            detail::writeUnsigned(out, gradient.chartIndex, 8);
            out += 8;
            for (const auto* colors : { &gradient.largeColor, &gradient.smallColor })
                for (auto value : *colors) {
                    writeDouble(out, value);
                    out += 8;
                }
            detail::writeUnsigned(out, gradient.resolution, 8);
            offset += uvGradientBytes;
        }
        for (const auto& key : payload.components) {
            detail::writeUnsigned(bytes.data() + offset, key.size(), 4);
            offset += 4;
            std::memcpy(bytes.data() + offset, key.data(), key.size());
            offset += key.size();
        }
        return bytes;
    }
    namespace {
        struct ParsedUv {
            Layout layout;
            uint64_t triangles, components, charts, gradients;
            double texture;
            uint64_t logicalBytes;
        };
        ParsedUv describeUv(const uint8_t* bytes, size_t size, uint64_t expected, const UvPayloadLimits& limits)
        {
            if (!bytes)
                reject(UvPayloadErrorCode::invalidInput, "Worker UV input pointer is missing.");
            if (size < uvPrefixBytes)
                reject(UvPayloadErrorCode::truncated, "Worker UV prefix is incomplete.");
            if (detail::readUnsigned(bytes, 2) != 1)
                reject(UvPayloadErrorCode::unsupportedVersion, "Worker UV version is unsupported.");
            if (detail::readUnsigned(bytes + 2, 2) != uvPrefixBytes)
                reject(UvPayloadErrorCode::invalidLayout, "Worker UV prefix layout is invalid.");
            zeros(bytes + 4, 4);
            const auto triangles = detail::readUnsigned(bytes + 8, 8), components = detail::readUnsigned(bytes + 16, 8), charts = detail::readUnsigned(bytes + 24, 8), gradients = detail::readUnsigned(bytes + 32, 8), text = detail::readUnsigned(bytes + 40, 8);
            const auto layout = plan(triangles, components, charts, gradients, text, expected, limits);
            if (size < layout.bytes)
                reject(UvPayloadErrorCode::truncated, "Worker UV payload is incomplete.");
            if (size != layout.bytes)
                reject(UvPayloadErrorCode::trailingData, "Worker UV payload has trailing bytes.");
            const auto texture = readDouble(bytes + 48);
            validateTexture(texture, charts);
            for (uint64_t i = 0; i < triangles; ++i)
                validateTriangle(readTriangle(bytes + uvPrefixBytes + i * uvTriangleBytes), components, charts);
            uint64_t logical = layout.logicalBytes, allText = text;
            for (uint64_t i = 0; i < charts; ++i) {
                const auto chart = readChart(bytes + layout.charts + i * uvChartBytes);
                validateChart(chart, components);
                charge(allText, chart.imageID.size(), 1, limits.maximumTextBytes);
                charge(logical, chart.imageID.size(), 1, limits.maximumDecodedBytes);
            }
            for (uint64_t i = 0; i < gradients; ++i)
                validateGradient(readGradient(bytes + layout.gradients + i * uvGradientBytes), charts);
            size_t offset = layout.keys;
            uint64_t seenText = 0;
            std::unordered_set<std::string_view> keys;
            for (uint64_t i = 0; i < components; ++i) {
                // Layout reserves four bytes per remaining key plus all unconsumed text.
                const auto length = detail::readUnsigned(bytes + offset, 4);
                offset += 4;
                if (length > text - seenText)
                    reject(UvPayloadErrorCode::invalidLayout, "Worker UV key span exceeds declared text.");
                checkKey(std::string_view(reinterpret_cast<const char*>(bytes + offset), static_cast<size_t>(length)), limits, keys);
                seenText += length;
                offset += static_cast<size_t>(length);
            }
            if (seenText != text)
                reject(UvPayloadErrorCode::invalidLayout, "Worker UV text length is inconsistent.");
            return { layout, triangles, components, charts, gradients, texture, logical };
        }
    }
    uint64_t inspectUvPayload(const uint8_t* bytes, size_t size, uint64_t expected, const UvPayloadLimits& limits)
    {
        return describeUv(bytes, size, expected, limits).logicalBytes;
    }
    UvPayload decodeUvPayload(const uint8_t* bytes, size_t size, uint64_t expected, const UvPayloadLimits& limits)
    {
        const auto parsed = describeUv(bytes, size, expected, limits);
        const auto& layout = parsed.layout;
        const auto triangles = parsed.triangles, components = parsed.components, charts = parsed.charts, gradients = parsed.gradients;
        const auto texture = parsed.texture;
        UvPayload result;
        result.packedTextureSize = texture;
        result.triangles.reserve(static_cast<size_t>(triangles));
        result.charts.reserve(static_cast<size_t>(charts));
        result.gradients.reserve(static_cast<size_t>(gradients));
        result.components.reserve(static_cast<size_t>(components));
        for (uint64_t i = 0; i < triangles; ++i)
            result.triangles.push_back(readTriangle(bytes + uvPrefixBytes + i * uvTriangleBytes));
        for (uint64_t i = 0; i < charts; ++i)
            result.charts.push_back(readChart(bytes + layout.charts + i * uvChartBytes));
        for (uint64_t i = 0; i < gradients; ++i)
            result.gradients.push_back(readGradient(bytes + layout.gradients + i * uvGradientBytes));
        size_t offset = layout.keys;
        for (uint64_t i = 0; i < components; ++i) {
            const auto length = static_cast<size_t>(detail::readUnsigned(bytes + offset, 4));
            offset += 4;
            result.components.emplace_back(reinterpret_cast<const char*>(bytes + offset), length);
            offset += length;
        }
        return result;
    }
}
}
