// SPDX-License-Identifier: MIT
#include "WorkerBakedTextures.h"
#include "WorkerBakedTextureViews.h"
#include "WorkerWire.h"
#include <algorithm>
#include <cstring>
#include <limits>
namespace dust3d {
namespace worker {
    namespace {
        static_assert(maximumPayloadBytes <= std::numeric_limits<size_t>::max(), "Texture spans must fit size_t.");
        static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559, "Texture caps require IEEE binary64.");
        constexpr size_t levelPrefixBytes = 16;
        [[noreturn]] void reject(BakedTexturesErrorCode code, const char* cause)
        {
            throw BakedTexturesError(code, cause);
        }
        void charge(uint64_t& total, uint64_t amount, uint64_t maximum)
        {
            if (total > maximum || amount > maximum - total)
                reject(BakedTexturesErrorCode::limitExceeded, "Baked worker textures exceed an aggregate quota.");
            total += amount;
        }
        struct LevelSpan {
            uint32_t width, height;
            size_t size;
            const uint8_t* pixels;
        };
        struct Plan {
            std::array<uint32_t, bakedTextureRoleCount> counts {};
            std::array<LevelSpan, bakedTextureRoleCount * maximumTextureLevels> levels {};
            size_t count = 0;
            uint64_t wireBytes = bakedTexturesPrefixBytes, pixelBytes = 0, logicalBytes = 0;
            uint64_t lodBits = 0;
        };
        void requireColorMap(const std::array<uint32_t, bakedTextureRoleCount>& counts)
        {
            if (!counts[0])
                reject(BakedTexturesErrorCode::invalidLayout, "Baked worker textures require a color map.");
        }
        double samplingLod(uint64_t bits, const std::array<uint32_t, bakedTextureRoleCount>& counts)
        {
            // Reject nonfinite bit patterns before evaluating even a signaling NaN.
            if ((bits & 0x7ff0000000000000ULL) == 0x7ff0000000000000ULL)
                reject(BakedTexturesErrorCode::invalidSamplingLod, "Baked worker texture sampling cap must be finite.");
            double value;
            std::memcpy(&value, &bits, sizeof(value));
            if (value < 0)
                reject(BakedTexturesErrorCode::invalidSamplingLod, "Baked worker texture sampling cap must be nonnegative.");
            for (const auto count : counts)
                if (count && value > count - 1)
                    reject(BakedTexturesErrorCode::invalidSamplingLod, "Baked worker texture sampling cap references an unavailable level.");
            return value;
        }
        uint64_t pixelSize(uint64_t width, uint64_t height)
        {
            if (!width || width > 8192 || !height || height > 8192)
                reject(BakedTexturesErrorCode::invalidImage, "Baked worker texture dimensions are outside 1..8192.");
            return width * height * 4;
        }
        void append(Plan& plan, uint32_t width, uint32_t height, uint64_t size, const uint8_t* pixels, size_t level, const BakedTexturesLimits& limits)
        {
            if (!pixels || size != pixelSize(width, height))
                reject(BakedTexturesErrorCode::invalidImage, "Baked worker texture requires exact tight RGBA8 bytes.");
            if (level) {
                const auto& previous = plan.levels[plan.count - 1];
                if ((previous.width == 1 && previous.height == 1)
                    || width != std::max(uint32_t(1), previous.width / 2) || height != std::max(uint32_t(1), previous.height / 2))
                    reject(BakedTexturesErrorCode::invalidChain, "Baked worker texture levels must halve and stop at one pixel.");
            }
            charge(plan.pixelBytes, size, limits.maximumPixelBytes);
            charge(plan.logicalBytes, sizeof(TextureLevel), limits.maximumDecodedBytes);
            charge(plan.logicalBytes, size, limits.maximumDecodedBytes);
            charge(plan.wireBytes, levelPrefixBytes, limits.maximumWireBytes);
            charge(plan.wireBytes, size, limits.maximumWireBytes);
            plan.levels[plan.count++] = { width, height, static_cast<size_t>(size), pixels };
        }
        Plan describeValue(const BakedTexturesPayload& value, const BakedTexturesLimits& limits)
        {
            validateBakedTexturesLimits(limits);
            Plan plan;
            for (size_t role = 0; role < value.maps.size(); ++role) {
                if (value.maps[role].size() > maximumTextureLevels)
                    reject(BakedTexturesErrorCode::invalidLayout, "Baked worker texture chain exceeds fourteen levels.");
                plan.counts[role] = static_cast<uint32_t>(value.maps[role].size());
            }
            requireColorMap(plan.counts);
            std::memcpy(&plan.lodBits, &value.maximumSamplingLod, sizeof(plan.lodBits));
            samplingLod(plan.lodBits, plan.counts);
            for (const auto& chain : value.maps)
                for (size_t level = 0; level < chain.size(); ++level) {
                    const auto& image = chain[level];
                    append(plan, image.width, image.height, image.pixels.size(), image.pixels.data(), level, limits);
                }
            return plan;
        }
        Plan describeWire(const uint8_t* bytes, size_t size, const BakedTexturesLimits& limits)
        {
            validateBakedTexturesLimits(limits);
            if (!bytes)
                reject(BakedTexturesErrorCode::invalidInput, "Baked worker texture bytes are missing.");
            if (size < bakedTexturesPrefixBytes)
                reject(BakedTexturesErrorCode::truncated, "Baked worker texture prefix is incomplete.");
            if (detail::readUnsigned(bytes, 2) != 1)
                reject(BakedTexturesErrorCode::unsupportedVersion, "Baked worker texture version is unsupported.");
            if (detail::readUnsigned(bytes + 2, 2) != bakedTexturesPrefixBytes || detail::readUnsigned(bytes + 4, 4)
                || detail::readUnsigned(bytes + 24, 4) != bakedTextureRoleCount || detail::readUnsigned(bytes + 52, 4) || detail::readUnsigned(bytes + 56, 8))
                reject(BakedTexturesErrorCode::invalidLayout, "Baked worker texture prefix, role count or reserved fields are invalid.");
            Plan plan;
            size_t count = 0;
            for (size_t role = 0; role < bakedTextureRoleCount; ++role) {
                plan.counts[role] = static_cast<uint32_t>(detail::readUnsigned(bytes + 32 + role * 4, 4));
                // Bound every count before accumulating or using it as an index.
                if (plan.counts[role] > maximumTextureLevels)
                    reject(BakedTexturesErrorCode::invalidLayout, "Baked worker texture chain exceeds fourteen levels.");
                count += plan.counts[role];
            }
            requireColorMap(plan.counts);
            if (count != detail::readUnsigned(bytes + 28, 4))
                reject(BakedTexturesErrorCode::invalidLayout, "Baked worker texture aggregate level count is inconsistent.");
            const auto expectedPixels = detail::readUnsigned(bytes + 8, 8);
            if (expectedPixels > limits.maximumPixelBytes)
                reject(BakedTexturesErrorCode::limitExceeded, "Baked worker texture declared pixel bytes exceed the configured quota.");
            plan.lodBits = detail::readUnsigned(bytes + 16, 8);
            samplingLod(plan.lodBits, plan.counts);
            size_t offset = bakedTexturesPrefixBytes;
            for (const auto levels : plan.counts)
                for (size_t level = 0; level < levels; ++level) {
                    if (size - offset < levelPrefixBytes)
                        reject(BakedTexturesErrorCode::truncated, "Baked worker texture level prefix is incomplete.");
                    const auto width = static_cast<uint32_t>(detail::readUnsigned(bytes + offset, 4));
                    const auto height = static_cast<uint32_t>(detail::readUnsigned(bytes + offset + 4, 4));
                    const auto pixels = detail::readUnsigned(bytes + offset + 8, 8);
                    offset += levelPrefixBytes;
                    if (pixels != pixelSize(width, height))
                        reject(BakedTexturesErrorCode::invalidImage, "Baked worker texture requires exact tight RGBA8 bytes.");
                    if (pixels > size - offset)
                        reject(BakedTexturesErrorCode::truncated, "Baked worker texture pixel span is incomplete.");
                    append(plan, width, height, pixels, bytes + offset, level, limits);
                    offset += static_cast<size_t>(pixels);
                }
            if (offset != size)
                reject(BakedTexturesErrorCode::trailingData, "Baked worker textures contain trailing bytes.");
            if (plan.pixelBytes != expectedPixels)
                reject(BakedTexturesErrorCode::invalidLayout, "Baked worker texture aggregate pixel count is inconsistent.");
            return plan;
        }
        Plan describeViews(const detail::BakedTextureViews& views, const BakedTexturesLimits& limits)
        {
            validateBakedTexturesLimits(limits);
            Plan plan;
            plan.counts = views.counts;
            requireColorMap(plan.counts);
            for (const auto count : plan.counts)
                if (count > maximumTextureLevels)
                    reject(BakedTexturesErrorCode::invalidLayout, "Baked worker texture chain exceeds fourteen levels.");
            std::memcpy(&plan.lodBits, &views.maximumSamplingLod, sizeof(plan.lodBits));
            samplingLod(plan.lodBits, plan.counts);
            for (size_t role = 0; role < bakedTextureRoleCount; ++role)
                for (size_t level = 0; level < plan.counts[role]; ++level) {
                    const auto& view = views.maps[role][level];
                    append(plan, view.width, view.height, view.byteCount, view.pixels, level, limits);
                }
            return plan;
        }
        BakedTexturesPayload copyPlan(const Plan& plan)
        {
            BakedTexturesPayload value;
            value.maximumSamplingLod = samplingLod(plan.lodBits, plan.counts);
            size_t index = 0;
            for (size_t role = 0; role < bakedTextureRoleCount; ++role) {
                auto& chain = value.maps[role];
                chain.reserve(plan.counts[role]);
                for (size_t level = 0; level < plan.counts[role]; ++level) {
                    const auto& span = plan.levels[index++];
                    chain.push_back({ span.width, span.height, { span.pixels, span.pixels + span.size } });
                }
            }
            return value;
        }

    }
    BakedTexturesError::BakedTexturesError(BakedTexturesErrorCode code, const char* cause)
        : std::runtime_error(cause)
        , m_code(code)
    {
    }
    void validateBakedTexturesLimits(const BakedTexturesLimits& limits)
    {
        if (!limits.maximumWireBytes || limits.maximumWireBytes > maximumPayloadBytes
            || !limits.maximumDecodedBytes || limits.maximumDecodedBytes > maximumPayloadBytes
            || !limits.maximumPixelBytes || limits.maximumPixelBytes > maximumPayloadBytes)
            reject(BakedTexturesErrorCode::invalidLimits, "Baked worker texture policy is outside supported positive bounds.");
    }
    std::vector<uint8_t> encodeBakedTexturesPayload(const BakedTexturesPayload& value, const BakedTexturesLimits& limits)
    {
        const auto plan = describeValue(value, limits);
        std::vector<uint8_t> bytes(static_cast<size_t>(plan.wireBytes), 0);
        detail::writeUnsigned(bytes.data(), 1, 2);
        detail::writeUnsigned(bytes.data() + 2, bakedTexturesPrefixBytes, 2);
        detail::writeUnsigned(bytes.data() + 8, plan.pixelBytes, 8);
        detail::writeUnsigned(bytes.data() + 16, plan.lodBits, 8);
        detail::writeUnsigned(bytes.data() + 24, bakedTextureRoleCount, 4);
        detail::writeUnsigned(bytes.data() + 28, plan.count, 4);
        for (size_t role = 0; role < bakedTextureRoleCount; ++role)
            detail::writeUnsigned(bytes.data() + 32 + role * 4, plan.counts[role], 4);
        size_t offset = bakedTexturesPrefixBytes;
        for (size_t index = 0; index < plan.count; ++index) {
            const auto& level = plan.levels[index];
            detail::writeUnsigned(bytes.data() + offset, level.width, 4);
            detail::writeUnsigned(bytes.data() + offset + 4, level.height, 4);
            detail::writeUnsigned(bytes.data() + offset + 8, level.size, 8);
            offset += levelPrefixBytes;
            std::copy_n(level.pixels, level.size, bytes.data() + offset);
            offset += level.size;
        }
        return bytes;
    }
    BakedTexturesInspection inspectBakedTexturesPayload(const uint8_t* bytes, size_t size, const BakedTexturesLimits& limits)
    {
        const auto plan = describeWire(bytes, size, limits);
        return { plan.count, plan.pixelBytes, plan.logicalBytes };
    }
    BakedTexturesPayload decodeBakedTexturesPayload(const uint8_t* bytes, size_t size, const BakedTexturesLimits& limits)
    {
        return copyPlan(describeWire(bytes, size, limits));
    }
    namespace detail {
        BakedTextureViewsInspection inspectBakedTextureViews(const BakedTextureViews& views, const BakedTexturesLimits& limits)
        {
            const auto plan = describeViews(views, limits);
            return { plan.wireBytes, plan.logicalBytes };
        }
        BakedTexturesPayload copyBakedTextureViews(const BakedTextureViews& views, const BakedTexturesLimits& limits)
        {
            return copyPlan(describeViews(views, limits));
        }
    }
}
}
