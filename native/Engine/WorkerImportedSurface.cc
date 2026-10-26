// SPDX-License-Identifier: MIT
#include "WorkerImportedSurface.h"
#include "WorkerImportedCatalogViews.h"
#include "WorkerImportedSurfaceValidation.h"
#include "WorkerUtf8.h"
#include "WorkerWire.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string_view>
#include <unordered_set>
namespace dust3d {
namespace worker {
    namespace {
        static_assert(maximumPayloadBytes <= std::numeric_limits<size_t>::max(), "Imported spans must fit size_t.");
        static_assert(sizeof(double) == 8 && sizeof(float) == 4 && std::numeric_limits<double>::is_iec559 && std::numeric_limits<float>::is_iec559, "Imported metadata requires IEEE scalars.");
        [[noreturn]] void reject(ImportedSurfaceErrorCode code, const char* cause) { throw ImportedSurfaceError(code, cause); }
        void charge(uint64_t& total, uint64_t count, uint64_t unit, uint64_t maximum)
        {
            if (total > maximum || count > (maximum - total) / unit)
                reject(ImportedSurfaceErrorCode::limitExceeded, "Imported worker output exceeds an aggregate quota.");
            total += count * unit;
        }
        void validateSurfaceLimits(const ImportedMetadataLimits& limits)
        {
            if (!limits.maximumWireBytes || limits.maximumWireBytes > maximumPayloadBytes
                || !limits.maximumDecodedBytes || limits.maximumDecodedBytes > maximumPayloadBytes
                || !limits.maximumTriangles || limits.maximumTriangles > 1000000
                || !limits.maximumModels || limits.maximumModels > 100000
                || !limits.maximumComponents || limits.maximumComponents > 100000
                || !limits.maximumTextBytes || limits.maximumTextBytes > maximumPayloadBytes)
                reject(ImportedSurfaceErrorCode::invalidLimits, "Imported surface policy is outside supported positive bounds.");
        }
        struct SurfacePlan {
            size_t wire, keys;
            uint64_t logical;
        };
        SurfacePlan planSurface(uint64_t triangles, uint64_t models, uint64_t components, uint64_t text, uint64_t expected, const ImportedMetadataLimits& limits)
        {
            validateSurfaceLimits(limits);
            if (!triangles || triangles != expected)
                reject(ImportedSurfaceErrorCode::countMismatch, "Imported records do not match geometry triangle order/count.");
            if (triangles > limits.maximumTriangles || models > limits.maximumModels || components > limits.maximumComponents || text > limits.maximumTextBytes)
                reject(ImportedSurfaceErrorCode::limitExceeded, "Imported catalog/triangle/text counts exceed configured limits.");
            uint64_t wire = importedMetadataPrefixBytes, logical = 0;
            charge(wire, triangles, importedMetadataTriangleBytes, limits.maximumWireBytes);
            const auto keys = wire;
            charge(wire, models, 4, limits.maximumWireBytes);
            charge(wire, components, 8, limits.maximumWireBytes);
            charge(wire, text, 1, limits.maximumWireBytes);
            charge(logical, triangles, sizeof(ImportedSurfaceTriangle), limits.maximumDecodedBytes);
            charge(logical, models, sizeof(std::string) + 1, limits.maximumDecodedBytes);
            charge(logical, components, sizeof(ImportedSurfaceComponent) + 1, limits.maximumDecodedBytes);
            charge(logical, text, 1, limits.maximumDecodedBytes);
            return { static_cast<size_t>(wire), static_cast<size_t>(keys), logical };
        }
        void validateTriangle(const ImportedSurfaceTriangle& value, uint64_t models, uint64_t components)
        {
            if (value.uvKnown > 1 || value.colorEncoding > 1
                || (value.modelIndex != noImportedIdentity && value.modelIndex >= models)
                || (value.componentIndex != noImportedIdentity && value.componentIndex >= components)
                || (value.uvKnown && value.modelIndex == noImportedIdentity))
                reject(ImportedSurfaceErrorCode::invalidMetadata, "Imported surface flags or catalog references are invalid.");
            for (const auto& uv : value.uv)
                for (double coordinate : uv)
                    if (!std::isfinite(coordinate) || (!value.uvKnown && coordinate != 0))
                        reject(ImportedSurfaceErrorCode::invalidMetadata, "Imported authored UVs are invalid or unknown mapping is nonzero.");
            for (const auto& color : value.colors)
                for (double channel : color)
                    if (!std::isfinite(channel) || channel < 0 || channel > 1)
                        reject(ImportedSurfaceErrorCode::invalidMetadata, "Imported straight vertex color is outside finite unit range.");
        }
        void validateKey(std::string_view key, std::unordered_set<std::string_view>& keys)
        {
            if (key.empty() || key.size() > 38 || !detail::isIdentityUtf8(key) || !keys.insert(key).second)
                reject(ImportedSurfaceErrorCode::invalidIdentity, "Imported catalog key is empty, oversized, duplicated or not non-NUL UTF8.");
        }
        void validateKind(uint32_t kind)
        {
            if (kind != 1 && kind != 2)
                reject(ImportedSurfaceErrorCode::invalidMetadata, "Imported component membership is unsupported.");
        }
        template <typename Scalar, typename Bits>
        Scalar readScalar(const uint8_t* bytes)
        {
            const Bits bits = static_cast<Bits>(detail::readUnsigned(bytes, sizeof(Bits)));
            Scalar value;
            std::memcpy(&value, &bits, sizeof(value));
            return value;
        }
        template <typename Bits, typename Scalar>
        void writeScalar(uint8_t* bytes, Scalar value)
        {
            Bits bits;
            std::memcpy(&bits, &value, sizeof(bits));
            detail::writeUnsigned(bytes, bits, sizeof(bits));
        }
        ImportedSurfaceTriangle readTriangle(const uint8_t* bytes)
        {
            ImportedSurfaceTriangle value;
            for (size_t corner = 0; corner < 3; ++corner)
                for (size_t axis = 0; axis < 2; ++axis)
                    value.uv[corner][axis] = readScalar<double, uint64_t>(bytes + corner * 16 + axis * 8);
            for (size_t corner = 0; corner < 3; ++corner)
                for (size_t channel = 0; channel < 4; ++channel)
                    value.colors[corner][channel] = readScalar<double, uint64_t>(bytes + 48 + corner * 32 + channel * 8);
            value.modelIndex = detail::readUnsigned(bytes + 144, 8);
            value.componentIndex = detail::readUnsigned(bytes + 152, 8);
            value.uvKnown = bytes[160];
            value.colorEncoding = static_cast<uint32_t>(detail::readUnsigned(bytes + 164, 4));
            return value;
        }
        void writeTriangle(uint8_t* bytes, const ImportedSurfaceTriangle& value)
        {
            for (size_t corner = 0; corner < 3; ++corner)
                for (size_t axis = 0; axis < 2; ++axis)
                    writeScalar<uint64_t>(bytes + corner * 16 + axis * 8, value.uv[corner][axis]);
            for (size_t corner = 0; corner < 3; ++corner)
                for (size_t channel = 0; channel < 4; ++channel)
                    writeScalar<uint64_t>(bytes + 48 + corner * 32 + channel * 8, value.colors[corner][channel]);
            detail::writeUnsigned(bytes + 144, value.modelIndex, 8);
            detail::writeUnsigned(bytes + 152, value.componentIndex, 8);
            bytes[160] = value.uvKnown;
            detail::writeUnsigned(bytes + 164, value.colorEncoding, 4);
        }
        std::vector<uint8_t> encodeSurface(const ImportedSurfacePayload& value, uint64_t expected, const ImportedMetadataLimits& limits)
        {
            (void)planSurface(value.triangles.size(), value.models.size(), value.components.size(), 0, expected, limits);
            uint64_t text = 0;
            std::unordered_set<std::string_view> keys;
            for (const auto& key : value.models) {
                validateKey(key, keys);
                charge(text, key.size(), 1, limits.maximumTextBytes);
            }
            keys.clear();
            for (const auto& component : value.components) {
                validateKind(component.kind);
                validateKey(component.id, keys);
                charge(text, component.id.size(), 1, limits.maximumTextBytes);
            }
            const auto plan = planSurface(value.triangles.size(), value.models.size(), value.components.size(), text, expected, limits);
            for (const auto& triangle : value.triangles)
                validateTriangle(triangle, value.models.size(), value.components.size());
            std::vector<uint8_t> bytes(plan.wire, 0);
            detail::writeUnsigned(bytes.data(), 1, 2);
            detail::writeUnsigned(bytes.data() + 2, importedMetadataPrefixBytes, 2);
            detail::writeUnsigned(bytes.data() + 8, value.triangles.size(), 8);
            detail::writeUnsigned(bytes.data() + 16, value.models.size(), 8);
            detail::writeUnsigned(bytes.data() + 24, value.components.size(), 8);
            detail::writeUnsigned(bytes.data() + 32, text, 8);
            size_t offset = importedMetadataPrefixBytes;
            for (const auto& triangle : value.triangles) {
                writeTriangle(bytes.data() + offset, triangle);
                offset += importedMetadataTriangleBytes;
            }
            auto key = [&](const std::string& value) { detail::writeUnsigned(bytes.data() + offset, value.size(), 4); offset += 4; std::copy(value.begin(), value.end(), bytes.begin() + offset); offset += value.size(); };
            for (const auto& model : value.models)
                key(model);
            for (const auto& component : value.components) {
                detail::writeUnsigned(bytes.data() + offset, component.kind, 4);
                offset += 4;
                key(component.id);
            }
            return bytes;
        }
        struct SurfaceDescription {
            SurfacePlan plan;
            uint64_t triangles, models, components;
        };
        SurfaceDescription inspectSurface(const uint8_t* bytes, size_t size, uint64_t expected, const ImportedMetadataLimits& limits)
        {
            if (detail::readUnsigned(bytes, 2) != 1)
                reject(ImportedSurfaceErrorCode::unsupportedVersion, "Imported surface version is unsupported.");
            if (detail::readUnsigned(bytes + 2, 2) != importedMetadataPrefixBytes || detail::readUnsigned(bytes + 4, 4))
                reject(ImportedSurfaceErrorCode::invalidLayout, "Imported surface prefix/reserved fields are invalid.");
            const auto triangles = detail::readUnsigned(bytes + 8, 8), models = detail::readUnsigned(bytes + 16, 8), components = detail::readUnsigned(bytes + 24, 8), text = detail::readUnsigned(bytes + 32, 8);
            const auto plan = planSurface(triangles, models, components, text, expected, limits);
            if (size < plan.wire)
                reject(ImportedSurfaceErrorCode::truncated, "Imported surface records/text are incomplete.");
            if (size > plan.wire)
                reject(ImportedSurfaceErrorCode::trailingData, "Imported surface has trailing bytes.");
            for (uint64_t index = 0; index < triangles; ++index) {
                const auto* record = bytes + importedMetadataPrefixBytes + index * importedMetadataTriangleBytes;
                if (detail::readUnsigned(record + 161, 3))
                    reject(ImportedSurfaceErrorCode::invalidLayout, "Imported surface triangle reserved bytes are nonzero.");
                validateTriangle(readTriangle(record), models, components);
            }
            size_t offset = plan.keys;
            uint64_t observedText = 0;
            std::unordered_set<std::string_view> keys;
            auto key = [&] {
                if (size - offset < 4)
                    reject(ImportedSurfaceErrorCode::truncated, "Imported catalog key prefix is incomplete.");
                const auto count = detail::readUnsigned(bytes + offset, 4);
                offset += 4;
                if (count > size - offset)
                    reject(ImportedSurfaceErrorCode::truncated, "Imported catalog key span is incomplete.");
                validateKey({ reinterpret_cast<const char*>(bytes + offset), static_cast<size_t>(count) }, keys);
                charge(observedText, count, 1, limits.maximumTextBytes);
                offset += static_cast<size_t>(count);
            };
            for (uint64_t index = 0; index < models; ++index)
                key();
            keys.clear();
            for (uint64_t index = 0; index < components; ++index) {
                if (size - offset < 4)
                    reject(ImportedSurfaceErrorCode::truncated, "Imported component kind is incomplete.");
                validateKind(static_cast<uint32_t>(detail::readUnsigned(bytes + offset, 4)));
                offset += 4;
                key();
            }
            if (offset != size)
                reject(ImportedSurfaceErrorCode::invalidLayout, "Imported catalog text framing is inconsistent.");
            return { plan, triangles, models, components };
        }
        ImportedSurfacePayload decodeSurface(const uint8_t* bytes, const SurfaceDescription& shape)
        {
            ImportedSurfacePayload value;
            value.triangles.reserve(static_cast<size_t>(shape.triangles));
            value.models.reserve(static_cast<size_t>(shape.models));
            value.components.reserve(static_cast<size_t>(shape.components));
            for (uint64_t index = 0; index < shape.triangles; ++index)
                value.triangles.push_back(readTriangle(bytes + importedMetadataPrefixBytes + index * importedMetadataTriangleBytes));
            size_t offset = shape.plan.keys;
            auto key = [&] { const auto count = static_cast<size_t>(detail::readUnsigned(bytes + offset, 4)); offset += 4; std::string result(reinterpret_cast<const char*>(bytes + offset), count); offset += count; return result; };
            for (uint64_t index = 0; index < shape.models; ++index)
                value.models.push_back(key());
            for (uint64_t index = 0; index < shape.components; ++index) {
                const auto kind = static_cast<uint32_t>(detail::readUnsigned(bytes + offset, 4));
                offset += 4;
                value.components.push_back({ key(), kind });
            }
            return value;
        }
        struct Description {
            size_t geometry;
            SurfaceDescription metadata;
            ImportedSurfaceInspection summary;
        };
        uint64_t totalCharge(uint64_t first, uint64_t second, uint64_t maximum, uint64_t prefix = 0)
        {
            charge(prefix, first, 1, maximum);
            charge(prefix, second, 1, maximum);
            return prefix;
        }
        Description describe(const uint8_t* bytes, size_t size, const ImportedSurfaceLimits& limits)
        {
            validateImportedSurfaceLimits(limits);
            if (!bytes)
                reject(ImportedSurfaceErrorCode::invalidInput, "Imported output bytes are missing.");
            if (size < importedSurfacePrefixBytes)
                reject(ImportedSurfaceErrorCode::truncated, "Imported output prefix is incomplete.");
            if (detail::readUnsigned(bytes, 2) != 1)
                reject(ImportedSurfaceErrorCode::unsupportedVersion, "Imported output version is unsupported.");
            if (detail::readUnsigned(bytes + 2, 2) != importedSurfacePrefixBytes || detail::readUnsigned(bytes + 4, 4))
                reject(ImportedSurfaceErrorCode::invalidLayout, "Imported output prefix/reserved fields are invalid.");
            const auto geometry = detail::readUnsigned(bytes + 8, 8), surface = detail::readUnsigned(bytes + 16, 8);
            const auto total = totalCharge(geometry, surface, limits.maximumWireBytes, importedSurfacePrefixBytes);
            if (geometry < meshPrefixBytes || surface < importedMetadataPrefixBytes)
                reject(ImportedSurfaceErrorCode::invalidLayout, "Imported inner spans lack complete prefixes.");
            if (size < total)
                reject(ImportedSurfaceErrorCode::truncated, "Imported inner spans are incomplete.");
            if (size > total)
                reject(ImportedSurfaceErrorCode::trailingData, "Imported output has trailing bytes.");
            const auto mesh = inspectMeshPayload(bytes + importedSurfacePrefixBytes, static_cast<size_t>(geometry), limits.geometry);
            const auto metadata = inspectSurface(bytes + importedSurfacePrefixBytes + geometry, static_cast<size_t>(surface), mesh.vertexCount / 3, limits.surface);
            return { static_cast<size_t>(geometry), metadata,
                { mesh.vertexCount, mesh.vertexCount / 3, totalCharge(mesh.logicalBytes, metadata.plan.logical, limits.maximumDecodedBytes) } };
        }
    }
    namespace detail {
        void visitImportedSurfaceModels(const uint8_t* bytes, size_t size, const ImportedSurfaceLimits& limits, const std::function<void(uint64_t, std::string_view)>& visitor)
        {
            const auto shape = describe(bytes, size, limits);
            const auto* surface = bytes + importedSurfacePrefixBytes + shape.geometry;
            size_t offset = shape.metadata.plan.keys;
            for (uint64_t index = 0; index < shape.metadata.models; ++index) {
                const auto count = static_cast<size_t>(readUnsigned(surface + offset, 4));
                offset += 4;
                visitor(index, { reinterpret_cast<const char*>(surface + offset), count });
                offset += count;
            }
        }
        ImportedSurfaceMeasurement measureImportedSurface(uint64_t triangles, uint64_t models, uint64_t components, uint64_t text, uint64_t expected, const ImportedMetadataLimits& limits)
        {
            const auto shape = planSurface(triangles, models, components, text, expected, limits);
            return { shape.wire, shape.logical };
        }
        void validateImportedTriangle(const ImportedSurfaceTriangle& value, uint64_t models, uint64_t components) { validateTriangle(value, models, components); }
        void validateImportedKey(std::string_view key, std::unordered_set<std::string_view>& keys) { validateKey(key, keys); }
        void validateImportedKind(uint32_t kind) { validateKind(kind); }
    }
    ImportedSurfaceError::ImportedSurfaceError(ImportedSurfaceErrorCode code, const char* cause)
        : std::runtime_error(cause)
        , m_code(code)
    {
    }
    void validateImportedSurfaceLimits(const ImportedSurfaceLimits& limits)
    {
        if (!limits.maximumWireBytes || limits.maximumWireBytes > maximumPayloadBytes || !limits.maximumDecodedBytes || limits.maximumDecodedBytes > maximumPayloadBytes)
            reject(ImportedSurfaceErrorCode::invalidLimits, "Imported output policy is outside supported positive bounds.");
        validateMeshPayloadLimits(limits.geometry);
        validateSurfaceLimits(limits.surface);
    }
    std::vector<uint8_t> encodeImportedSurfacePayload(const ImportedSurfacePayload& value, const ImportedSurfaceLimits& limits)
    {
        validateImportedSurfaceLimits(limits);
        const auto mesh = encodeMeshPayload(value.geometry, limits.geometry);
        const auto surface = encodeSurface(value, value.geometry.vertices.size() / 3, limits.surface);
        const auto size = totalCharge(mesh.size(), surface.size(), limits.maximumWireBytes, importedSurfacePrefixBytes);
        const auto geometryShape = inspectMeshPayload(mesh.data(), mesh.size(), limits.geometry);
        const auto metadataShape = inspectSurface(surface.data(), surface.size(), geometryShape.vertexCount / 3, limits.surface);
        (void)totalCharge(geometryShape.logicalBytes, metadataShape.plan.logical, limits.maximumDecodedBytes);
        std::vector<uint8_t> bytes(static_cast<size_t>(size), 0);
        detail::writeUnsigned(bytes.data(), 1, 2);
        detail::writeUnsigned(bytes.data() + 2, importedSurfacePrefixBytes, 2);
        detail::writeUnsigned(bytes.data() + 8, mesh.size(), 8);
        detail::writeUnsigned(bytes.data() + 16, surface.size(), 8);
        auto output = std::copy(mesh.begin(), mesh.end(), bytes.begin() + importedSurfacePrefixBytes);
        std::copy(surface.begin(), surface.end(), output);
        return bytes;
    }
    ImportedSurfaceInspection inspectImportedSurfacePayload(const uint8_t* bytes, size_t size, const ImportedSurfaceLimits& limits) { return describe(bytes, size, limits).summary; }
    ImportedSurfacePayload decodeImportedSurfacePayload(const uint8_t* bytes, size_t size, const ImportedSurfaceLimits& limits)
    {
        const auto shape = describe(bytes, size, limits);
        auto value = decodeSurface(bytes + importedSurfacePrefixBytes + shape.geometry, shape.metadata);
        value.geometry = decodeMeshPayload(bytes + importedSurfacePrefixBytes, shape.geometry, limits.geometry);
        return value;
    }
}
}
