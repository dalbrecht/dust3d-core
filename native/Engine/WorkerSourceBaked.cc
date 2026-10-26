// SPDX-License-Identifier: MIT
#include "WorkerSourceBaked.h"
#include "WorkerSourceBakedValidation.h"
#include "WorkerTangentValidation.h"
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
        static_assert(maximumPayloadBytes <= std::numeric_limits<size_t>::max(), "Baked spans must fit size_t.");
        static_assert(sizeof(double) == 8 && sizeof(float) == 4 && std::numeric_limits<double>::is_iec559 && std::numeric_limits<float>::is_iec559, "Baked metadata requires IEEE scalars.");
        [[noreturn]] void reject(SourceBakedErrorCode code, const char* cause) { throw SourceBakedError(code, cause); }
        void charge(uint64_t& total, uint64_t count, uint64_t unit, uint64_t maximum)
        {
            if (total > maximum || count > (maximum - total) / unit)
                reject(SourceBakedErrorCode::limitExceeded, "Source-baked worker output exceeds an aggregate quota.");
            total += count * unit;
        }
        void validateSurfaceLimits(const BakedSurfaceLimits& limits)
        {
            if (!limits.maximumWireBytes || limits.maximumWireBytes > maximumPayloadBytes
                || !limits.maximumDecodedBytes || limits.maximumDecodedBytes > maximumPayloadBytes
                || !limits.maximumTriangles || limits.maximumTriangles > 1000000
                || !limits.maximumModels || limits.maximumModels > 100000
                || !limits.maximumComponents || limits.maximumComponents > 100000
                || !limits.maximumTextBytes || limits.maximumTextBytes > maximumPayloadBytes)
                reject(SourceBakedErrorCode::invalidLimits, "Source-baked surface policy is outside supported positive bounds.");
        }
        struct SurfacePlan {
            size_t wire, keys;
            uint64_t logical;
        };
        SurfacePlan planSurface(uint64_t triangles, uint64_t models, uint64_t components, uint64_t text, uint64_t expected, const BakedSurfaceLimits& limits)
        {
            validateSurfaceLimits(limits);
            if (!triangles || triangles != expected)
                reject(SourceBakedErrorCode::countMismatch, "Source-baked records do not match geometry triangle order/count.");
            if (triangles > limits.maximumTriangles || models > limits.maximumModels || components > limits.maximumComponents || text > limits.maximumTextBytes)
                reject(SourceBakedErrorCode::limitExceeded, "Source-baked catalog/triangle/text counts exceed configured limits.");
            uint64_t wire = bakedSurfacePrefixBytes, logical = 0;
            charge(wire, triangles, bakedSurfaceTriangleBytes, limits.maximumWireBytes);
            const auto keys = wire;
            charge(wire, models, 4, limits.maximumWireBytes);
            charge(wire, components, 8, limits.maximumWireBytes);
            charge(wire, text, 1, limits.maximumWireBytes);
            charge(logical, triangles, sizeof(BakedSurfaceTriangle), limits.maximumDecodedBytes);
            charge(logical, models, sizeof(std::string) + 1, limits.maximumDecodedBytes);
            charge(logical, components, sizeof(BakedSurfaceComponent) + 1, limits.maximumDecodedBytes);
            charge(logical, text, 1, limits.maximumDecodedBytes);
            return { static_cast<size_t>(wire), static_cast<size_t>(keys), logical };
        }
        void validateTriangle(const BakedSurfaceTriangle& value, uint64_t models, uint64_t components)
        {
            if (value.uvKnown > 1 || value.policyKnown > 1 || value.isDoubleSided > 1 || value.tangentKnown > 1
                || (value.modelIndex != noBakedIdentity && value.modelIndex >= models)
                || (value.componentIndex != noBakedIdentity && value.componentIndex >= components))
                reject(SourceBakedErrorCode::invalidMetadata, "Source-baked flags or catalog references are invalid.");
            for (const auto& uv : value.uv)
                for (const auto coordinate : uv)
                    if (!std::isfinite(coordinate) || coordinate < 0 || coordinate > 1 || (!value.uvKnown && coordinate != 0))
                        reject(SourceBakedErrorCode::invalidMetadata, "Source-baked packed UVs are invalid or unknown mapping is nonzero.");
            if (!detail::isValidTextureTangent(value.tangent, value.handedness, value.tangentKnown, value.uvKnown))
                reject(SourceBakedErrorCode::invalidMetadata, "Source-baked tangent direction, mapping, unit length or handedness is invalid.");
            if (value.policyKnown && (value.alphaMode > 2 || !std::isfinite(value.alphaCutoff) || value.alphaCutoff < 0))
                reject(SourceBakedErrorCode::invalidMetadata, "Source-baked known alpha policy is invalid.");
        }
        void validateKey(std::string_view key, std::unordered_set<std::string_view>& keys)
        {
            if (key.empty() || key.size() > 38 || !detail::isIdentityUtf8(key) || !keys.insert(key).second)
                reject(SourceBakedErrorCode::invalidIdentity, "Source-baked catalog key is empty, oversized, duplicated or not non-NUL UTF8.");
        }
        void validateKind(uint32_t kind)
        {
            if (kind != 1 && kind != 2)
                reject(SourceBakedErrorCode::invalidMetadata, "Source-baked component membership is unsupported.");
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
        BakedSurfaceTriangle readTriangle(const uint8_t* bytes)
        {
            BakedSurfaceTriangle value;
            for (size_t corner = 0; corner < 3; ++corner)
                for (size_t axis = 0; axis < 2; ++axis)
                    value.uv[corner][axis] = readScalar<double, uint64_t>(bytes + corner * 16 + axis * 8);
            for (size_t axis = 0; axis < 3; ++axis)
                value.tangent[axis] = readScalar<float, uint32_t>(bytes + 48 + axis * 4);
            value.handedness = readScalar<float, uint32_t>(bytes + 60);
            value.alphaCutoff = readScalar<double, uint64_t>(bytes + 64);
            value.modelIndex = detail::readUnsigned(bytes + 72, 8);
            value.componentIndex = detail::readUnsigned(bytes + 80, 8);
            value.uvKnown = bytes[88];
            value.policyKnown = bytes[89];
            value.isDoubleSided = bytes[90];
            value.tangentKnown = bytes[91];
            value.alphaMode = static_cast<uint32_t>(detail::readUnsigned(bytes + 92, 4));
            return value;
        }
        void writeTriangle(uint8_t* bytes, const BakedSurfaceTriangle& value)
        {
            for (size_t corner = 0; corner < 3; ++corner)
                for (size_t axis = 0; axis < 2; ++axis)
                    writeScalar<uint64_t>(bytes + corner * 16 + axis * 8, value.uv[corner][axis]);
            for (size_t axis = 0; axis < 3; ++axis)
                writeScalar<uint32_t>(bytes + 48 + axis * 4, value.tangent[axis]);
            writeScalar<uint32_t>(bytes + 60, value.handedness);
            writeScalar<uint64_t>(bytes + 64, value.alphaCutoff);
            detail::writeUnsigned(bytes + 72, value.modelIndex, 8);
            detail::writeUnsigned(bytes + 80, value.componentIndex, 8);
            bytes[88] = value.uvKnown;
            bytes[89] = value.policyKnown;
            bytes[90] = value.isDoubleSided;
            bytes[91] = value.tangentKnown;
            detail::writeUnsigned(bytes + 92, value.alphaMode, 4);
        }
        std::vector<uint8_t> encodeSurface(const BakedSurfacePayload& value, uint64_t expected, const BakedSurfaceLimits& limits)
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
            detail::writeUnsigned(bytes.data() + 2, bakedSurfacePrefixBytes, 2);
            detail::writeUnsigned(bytes.data() + 8, value.triangles.size(), 8);
            detail::writeUnsigned(bytes.data() + 16, value.models.size(), 8);
            detail::writeUnsigned(bytes.data() + 24, value.components.size(), 8);
            detail::writeUnsigned(bytes.data() + 32, text, 8);
            size_t offset = bakedSurfacePrefixBytes;
            for (const auto& triangle : value.triangles) {
                writeTriangle(bytes.data() + offset, triangle);
                offset += bakedSurfaceTriangleBytes;
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
        SurfaceDescription inspectSurface(const uint8_t* bytes, size_t size, uint64_t expected, const BakedSurfaceLimits& limits)
        {
            if (detail::readUnsigned(bytes, 2) != 1)
                reject(SourceBakedErrorCode::unsupportedVersion, "Source-baked surface version is unsupported.");
            if (detail::readUnsigned(bytes + 2, 2) != bakedSurfacePrefixBytes || detail::readUnsigned(bytes + 4, 4))
                reject(SourceBakedErrorCode::invalidLayout, "Source-baked surface prefix/reserved fields are invalid.");
            const auto triangles = detail::readUnsigned(bytes + 8, 8), models = detail::readUnsigned(bytes + 16, 8), components = detail::readUnsigned(bytes + 24, 8), text = detail::readUnsigned(bytes + 32, 8);
            const auto plan = planSurface(triangles, models, components, text, expected, limits);
            if (size < plan.wire)
                reject(SourceBakedErrorCode::truncated, "Source-baked surface records/text are incomplete.");
            if (size > plan.wire)
                reject(SourceBakedErrorCode::trailingData, "Source-baked surface has trailing bytes.");
            for (uint64_t index = 0; index < triangles; ++index)
                validateTriangle(readTriangle(bytes + bakedSurfacePrefixBytes + index * bakedSurfaceTriangleBytes), models, components);
            size_t offset = plan.keys;
            uint64_t observedText = 0;
            std::unordered_set<std::string_view> keys;
            auto key = [&] {
                if (size - offset < 4)
                    reject(SourceBakedErrorCode::truncated, "Source-baked catalog key prefix is incomplete.");
                const auto count = detail::readUnsigned(bytes + offset, 4);
                offset += 4;
                if (count > size - offset)
                    reject(SourceBakedErrorCode::truncated, "Source-baked catalog key span is incomplete.");
                validateKey({ reinterpret_cast<const char*>(bytes + offset), static_cast<size_t>(count) }, keys);
                charge(observedText, count, 1, limits.maximumTextBytes);
                offset += static_cast<size_t>(count);
            };
            for (uint64_t index = 0; index < models; ++index)
                key();
            keys.clear();
            for (uint64_t index = 0; index < components; ++index) {
                if (size - offset < 4)
                    reject(SourceBakedErrorCode::truncated, "Source-baked component kind is incomplete.");
                validateKind(static_cast<uint32_t>(detail::readUnsigned(bytes + offset, 4)));
                offset += 4;
                key();
            }
            if (offset != size)
                reject(SourceBakedErrorCode::invalidLayout, "Source-baked catalog text framing is inconsistent.");
            return { plan, triangles, models, components };
        }
        BakedSurfacePayload decodeSurface(const uint8_t* bytes, const SurfaceDescription& shape)
        {
            BakedSurfacePayload value;
            value.triangles.reserve(static_cast<size_t>(shape.triangles));
            value.models.reserve(static_cast<size_t>(shape.models));
            value.components.reserve(static_cast<size_t>(shape.components));
            for (uint64_t index = 0; index < shape.triangles; ++index)
                value.triangles.push_back(readTriangle(bytes + bakedSurfacePrefixBytes + index * bakedSurfaceTriangleBytes));
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
        size_t outerWire(uint64_t geometry, uint64_t surface, uint64_t textures, const SourceBakedLimits& limits)
        {
            uint64_t wire = sourceBakedPrefixBytes;
            charge(wire, geometry, 1, limits.maximumWireBytes);
            charge(wire, surface, 1, limits.maximumWireBytes);
            charge(wire, textures, 1, limits.maximumWireBytes);
            if (geometry < meshPrefixBytes || surface < bakedSurfacePrefixBytes || textures < bakedTexturesPrefixBytes)
                reject(SourceBakedErrorCode::invalidLayout, "Source-baked inner spans lack complete prefixes.");
            return static_cast<size_t>(wire);
        }
        uint64_t logicalBytes(uint64_t geometry, uint64_t surface, uint64_t textures, const SourceBakedLimits& limits)
        {
            uint64_t total = 0;
            charge(total, geometry, 1, limits.maximumDecodedBytes);
            charge(total, surface, 1, limits.maximumDecodedBytes);
            charge(total, textures, 1, limits.maximumDecodedBytes);
            return total;
        }
        struct Description {
            size_t geometry, surface, textures;
            SurfaceDescription metadata;
            SourceBakedInspection summary;
        };
        Description describe(const uint8_t* bytes, size_t size, const SourceBakedLimits& limits)
        {
            validateSourceBakedLimits(limits);
            if (!bytes)
                reject(SourceBakedErrorCode::invalidInput, "Source-baked output bytes are missing.");
            if (size < sourceBakedPrefixBytes)
                reject(SourceBakedErrorCode::truncated, "Source-baked output prefix is incomplete.");
            if (detail::readUnsigned(bytes, 2) != 1)
                reject(SourceBakedErrorCode::unsupportedVersion, "Source-baked output version is unsupported.");
            if (detail::readUnsigned(bytes + 2, 2) != sourceBakedPrefixBytes || detail::readUnsigned(bytes + 4, 4))
                reject(SourceBakedErrorCode::invalidLayout, "Source-baked output prefix/reserved fields are invalid.");
            const auto geometry = detail::readUnsigned(bytes + 8, 8), surface = detail::readUnsigned(bytes + 16, 8), textures = detail::readUnsigned(bytes + 24, 8);
            const auto total = outerWire(geometry, surface, textures, limits);
            if (size < total)
                reject(SourceBakedErrorCode::truncated, "Source-baked output inner spans are incomplete.");
            if (size > total)
                reject(SourceBakedErrorCode::trailingData, "Source-baked output has trailing bytes.");
            const auto mesh = inspectMeshPayload(bytes + sourceBakedPrefixBytes, static_cast<size_t>(geometry), limits.geometry);
            const auto metadata = inspectSurface(bytes + sourceBakedPrefixBytes + geometry, static_cast<size_t>(surface), mesh.vertexCount / 3, limits.surface);
            const auto maps = inspectBakedTexturesPayload(bytes + sourceBakedPrefixBytes + geometry + surface, static_cast<size_t>(textures), limits.textures);
            return { static_cast<size_t>(geometry), static_cast<size_t>(surface), static_cast<size_t>(textures), metadata,
                { mesh.vertexCount, mesh.vertexCount / 3, maps.pixelBytes, logicalBytes(mesh.logicalBytes, metadata.plan.logical, maps.logicalBytes, limits) } };
        }
    }
    namespace detail {
        BakedSurfaceMeasurement measureBakedSurfaceCounts(uint64_t triangles, uint64_t models, uint64_t components, uint64_t text, uint64_t expected, const BakedSurfaceLimits& limits)
        {
            const auto plan = planSurface(triangles, models, components, text, expected, limits);
            return { plan.wire, plan.logical };
        }
        void validateBakedSurfaceTriangle(const BakedSurfaceTriangle& value, uint64_t models, uint64_t components) { validateTriangle(value, models, components); }
        void validateBakedCatalogKey(std::string_view key, std::unordered_set<std::string_view>& keys) { validateKey(key, keys); }
        void validateBakedComponentKind(uint32_t kind) { validateKind(kind); }
    }

    SourceBakedError::SourceBakedError(SourceBakedErrorCode code, const char* cause)
        : std::runtime_error(cause)
        , m_code(code)
    {
    }
    void validateSourceBakedLimits(const SourceBakedLimits& limits)
    {
        if (!limits.maximumWireBytes || limits.maximumWireBytes > maximumPayloadBytes || !limits.maximumDecodedBytes || limits.maximumDecodedBytes > maximumPayloadBytes)
            reject(SourceBakedErrorCode::invalidLimits, "Source-baked output policy is outside supported positive bounds.");
        validateMeshPayloadLimits(limits.geometry);
        validateSurfaceLimits(limits.surface);
        validateBakedTexturesLimits(limits.textures);
    }
    std::vector<uint8_t> encodeSourceBakedPayload(const SourceBakedPayload& value, const SourceBakedLimits& limits)
    {
        validateSourceBakedLimits(limits);
        const auto mesh = encodeMeshPayload(value.geometry, limits.geometry);
        const auto surface = encodeSurface(value.surface, value.geometry.vertices.size() / 3, limits.surface);
        const auto textures = encodeBakedTexturesPayload(value.textures, limits.textures);
        const auto size = outerWire(mesh.size(), surface.size(), textures.size(), limits);
        const auto geometryShape = inspectMeshPayload(mesh.data(), mesh.size(), limits.geometry);
        const auto metadataShape = inspectSurface(surface.data(), surface.size(), geometryShape.vertexCount / 3, limits.surface);
        const auto textureShape = inspectBakedTexturesPayload(textures.data(), textures.size(), limits.textures);
        (void)logicalBytes(geometryShape.logicalBytes, metadataShape.plan.logical, textureShape.logicalBytes, limits);
        std::vector<uint8_t> bytes(size, 0);
        detail::writeUnsigned(bytes.data(), 1, 2);
        detail::writeUnsigned(bytes.data() + 2, sourceBakedPrefixBytes, 2);
        detail::writeUnsigned(bytes.data() + 8, mesh.size(), 8);
        detail::writeUnsigned(bytes.data() + 16, surface.size(), 8);
        detail::writeUnsigned(bytes.data() + 24, textures.size(), 8);
        auto output = std::copy(mesh.begin(), mesh.end(), bytes.begin() + sourceBakedPrefixBytes);
        output = std::copy(surface.begin(), surface.end(), output);
        std::copy(textures.begin(), textures.end(), output);
        return bytes;
    }
    SourceBakedInspection inspectSourceBakedPayload(const uint8_t* bytes, size_t size, const SourceBakedLimits& limits) { return describe(bytes, size, limits).summary; }
    SourceBakedPayload decodeSourceBakedPayload(const uint8_t* bytes, size_t size, const SourceBakedLimits& limits)
    {
        const auto shape = describe(bytes, size, limits);
        return { decodeMeshPayload(bytes + sourceBakedPrefixBytes, shape.geometry, limits.geometry),
            decodeSurface(bytes + sourceBakedPrefixBytes + shape.geometry, shape.metadata),
            decodeBakedTexturesPayload(bytes + sourceBakedPrefixBytes + shape.geometry + shape.surface, shape.textures, limits.textures) };
    }
}
}
