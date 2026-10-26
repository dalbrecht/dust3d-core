// SPDX-License-Identifier: MIT
#include "WorkerLegacyBaked.h"
#include "WorkerTangentValidation.h"
#include "WorkerUvValidation.h"
#include "WorkerWire.h"
#include <algorithm>
#include <cstring>
namespace dust3d {
namespace worker {
    namespace {
        static_assert(maximumPayloadBytes <= std::numeric_limits<size_t>::max(), "Legacy baked spans must fit size_t.");
        static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559, "Tangent wire scalars require IEEE binary32.");
        [[noreturn]] void reject(LegacyBakedErrorCode code, const char* cause) { throw LegacyBakedError(code, cause); }
        void charge(uint64_t& total, uint64_t count, uint64_t unit, uint64_t maximum)
        {
            if (total > maximum || count > (maximum - total) / unit)
                reject(LegacyBakedErrorCode::limitExceeded, "Legacy baked output exceeds its aggregate quota.");
            total += count * unit;
        }
        struct Layout {
            size_t bytes, uvOffset, tangentOffset, textureOffset;
        };
        Layout planLayout(uint64_t geometry, uint64_t uv, uint64_t tangents, uint64_t textures, const LegacyBakedLimits& limits)
        {
            if (!tangents)
                reject(LegacyBakedErrorCode::countMismatch, "Legacy baked output requires nonempty per-triangle tangents.");
            if (tangents > limits.maximumTangents)
                reject(LegacyBakedErrorCode::limitExceeded, "Legacy tangent count exceeds configured limits.");
            if (geometry < meshPrefixBytes || uv < uvPrefixBytes || textures < bakedTexturesPrefixBytes)
                reject(LegacyBakedErrorCode::invalidLayout, "Legacy baked inner spans lack complete prefixes.");
            uint64_t size = legacyBakedPrefixBytes;
            charge(size, geometry, 1, limits.maximumWireBytes);
            const auto uvOffset = size;
            charge(size, uv, 1, limits.maximumWireBytes);
            const auto tangentOffset = size;
            charge(size, tangents, textureTangentBytes, limits.maximumWireBytes);
            const auto textureOffset = size;
            charge(size, textures, 1, limits.maximumWireBytes);
            return { static_cast<size_t>(size), static_cast<size_t>(uvOffset), static_cast<size_t>(tangentOffset), static_cast<size_t>(textureOffset) };
        }
        uint64_t calculateLogicalBytes(uint64_t geometry, uint64_t uv, uint64_t tangents, uint64_t textures, const LegacyBakedLimits& limits)
        {
            uint64_t total = 0;
            charge(total, geometry, 1, limits.maximumDecodedBytes);
            charge(total, uv, 1, limits.maximumDecodedBytes);
            charge(total, tangents, sizeof(TextureTangent), limits.maximumDecodedBytes);
            charge(total, textures, 1, limits.maximumDecodedBytes);
            return total;
        }
        void validateTangent(const TextureTangent& value, uint8_t uvKnown)
        {
            if (!detail::isValidTextureTangent(value.direction, value.handedness, value.known, uvKnown))
                reject(LegacyBakedErrorCode::invalidTangent, "Legacy tangent scalars, mapping or unit length are invalid.");
        }
        template <typename CountAt>
        void validateBaseOnly(CountAt countAt)
        {
            for (size_t role = 0; role < bakedTextureRoleCount; ++role)
                if (countAt(role) > 1)
                    reject(LegacyBakedErrorCode::invalidTextures, "Legacy baked output retains base maps only.");
        }
        float readFloat(const uint8_t* bytes)
        {
            const uint32_t bits = static_cast<uint32_t>(detail::readUnsigned(bytes, 4));
            float value;
            std::memcpy(&value, &bits, sizeof(value));
            return value;
        }
        void writeFloat(uint8_t* bytes, float value)
        {
            uint32_t bits;
            std::memcpy(&bits, &value, sizeof(bits));
            detail::writeUnsigned(bytes, bits, 4);
        }
        TextureTangent readTangent(const uint8_t* bytes)
        {
            for (size_t index = 17; index < textureTangentBytes; ++index)
                if (bytes[index])
                    reject(LegacyBakedErrorCode::invalidLayout, "Legacy tangent reserved bytes must be zero.");
            return { { readFloat(bytes), readFloat(bytes + 4), readFloat(bytes + 8) }, readFloat(bytes + 12), bytes[16] };
        }
        struct Description {
            Layout layout;
            size_t geometry, uv, textures;
            LegacyBakedInspection summary;
        };
        Description describeLegacyBaked(const uint8_t* bytes, size_t size, const LegacyBakedLimits& limits)
        {
            validateLegacyBakedLimits(limits);
            if (!bytes)
                reject(LegacyBakedErrorCode::invalidInput, "Legacy baked bytes are missing.");
            if (size < legacyBakedPrefixBytes)
                reject(LegacyBakedErrorCode::truncated, "Legacy baked prefix is incomplete.");
            if (detail::readUnsigned(bytes, 2) != 1)
                reject(LegacyBakedErrorCode::unsupportedVersion, "Legacy baked version is unsupported.");
            if (detail::readUnsigned(bytes + 2, 2) != legacyBakedPrefixBytes || detail::readUnsigned(bytes + 4, 4)
                || detail::readUnsigned(bytes + 40, 4) != textureTangentBytes || detail::readUnsigned(bytes + 44, 4))
                reject(LegacyBakedErrorCode::invalidLayout, "Legacy baked prefix/stride/reserved fields are invalid.");
            const auto geometry = detail::readUnsigned(bytes + 8, 8), uv = detail::readUnsigned(bytes + 16, 8), tangents = detail::readUnsigned(bytes + 24, 8), textures = detail::readUnsigned(bytes + 32, 8);
            const auto layout = planLayout(geometry, uv, tangents, textures, limits);
            if (size < layout.bytes)
                reject(LegacyBakedErrorCode::truncated, "Legacy baked inner spans are incomplete.");
            if (size > layout.bytes)
                reject(LegacyBakedErrorCode::trailingData, "Legacy baked output has trailing bytes.");
            const auto mesh = inspectMeshPayload(bytes + legacyBakedPrefixBytes, static_cast<size_t>(geometry), limits.geometry);
            if (mesh.vertexCount / 3 != tangents)
                reject(LegacyBakedErrorCode::countMismatch, "Legacy tangent count does not match geometry triangles.");
            const auto* uvBytes = bytes + layout.uvOffset;
            const auto uvLogical = inspectUvPayload(uvBytes, static_cast<size_t>(uv), tangents, limits.uv);
            for (uint64_t index = 0; index < tangents; ++index)
                validateTangent(readTangent(bytes + layout.tangentOffset + index * textureTangentBytes), detail::readValidatedUvTriangle(uvBytes + uvPrefixBytes + index * uvTriangleBytes).known);
            const auto* textureBytes = bytes + layout.textureOffset;
            const auto maps = inspectBakedTexturesPayload(textureBytes, static_cast<size_t>(textures), limits.textures);
            validateBaseOnly([&](size_t role) { return detail::readUnsigned(textureBytes + 32 + role * 4, 4); });
            return { layout, static_cast<size_t>(geometry), static_cast<size_t>(uv), static_cast<size_t>(textures),
                { mesh.vertexCount, tangents, calculateLogicalBytes(mesh.logicalBytes, uvLogical, tangents, maps.logicalBytes, limits) } };
        }
    }
    LegacyBakedError::LegacyBakedError(LegacyBakedErrorCode code, const char* cause)
        : std::runtime_error(cause)
        , m_code(code)
    {
    }
    void validateLegacyBakedLimits(const LegacyBakedLimits& limits)
    {
        if (!limits.maximumWireBytes || limits.maximumWireBytes > maximumPayloadBytes || !limits.maximumDecodedBytes || limits.maximumDecodedBytes > maximumPayloadBytes
            || !limits.maximumTangents || limits.maximumTangents > maximumUvRecords)
            reject(LegacyBakedErrorCode::invalidLimits, "Legacy baked quotas are outside supported positive bounds.");
        validateMeshPayloadLimits(limits.geometry);
        validateUvPayloadLimits(limits.uv);
        validateBakedTexturesLimits(limits.textures);
    }
    std::vector<uint8_t> encodeLegacyBakedPayload(const LegacyBakedPayload& value, const LegacyBakedLimits& limits)
    {
        validateLegacyBakedLimits(limits);
        if (value.tangents.size() != value.geometry.vertices.size() / 3)
            reject(LegacyBakedErrorCode::countMismatch, "Legacy tangent count does not match geometry triangles.");
        const auto geometry = encodeMeshPayload(value.geometry, limits.geometry);
        const auto uv = encodeUvPayload(value.uv, value.tangents.size(), limits.uv);
        if (value.tangents.size() > limits.maximumTangents)
            reject(LegacyBakedErrorCode::limitExceeded, "Legacy tangent count exceeds configured limits.");
        for (size_t index = 0; index < value.tangents.size(); ++index)
            validateTangent(value.tangents[index], value.uv.triangles[index].known);
        const auto textures = encodeBakedTexturesPayload(value.textures, limits.textures);
        validateBaseOnly([&](size_t role) { return value.textures.maps[role].size(); });
        const auto layout = planLayout(geometry.size(), uv.size(), value.tangents.size(), textures.size(), limits);
        const auto mesh = inspectMeshPayload(geometry.data(), geometry.size(), limits.geometry);
        const auto uvLogical = inspectUvPayload(uv.data(), uv.size(), value.tangents.size(), limits.uv);
        const auto maps = inspectBakedTexturesPayload(textures.data(), textures.size(), limits.textures);
        (void)calculateLogicalBytes(mesh.logicalBytes, uvLogical, value.tangents.size(), maps.logicalBytes, limits);
        std::vector<uint8_t> bytes(layout.bytes, 0);
        detail::writeUnsigned(bytes.data(), 1, 2);
        detail::writeUnsigned(bytes.data() + 2, legacyBakedPrefixBytes, 2);
        detail::writeUnsigned(bytes.data() + 8, geometry.size(), 8);
        detail::writeUnsigned(bytes.data() + 16, uv.size(), 8);
        detail::writeUnsigned(bytes.data() + 24, value.tangents.size(), 8);
        detail::writeUnsigned(bytes.data() + 32, textures.size(), 8);
        detail::writeUnsigned(bytes.data() + 40, textureTangentBytes, 4);
        std::copy(geometry.begin(), geometry.end(), bytes.begin() + legacyBakedPrefixBytes);
        std::copy(uv.begin(), uv.end(), bytes.begin() + layout.uvOffset);
        std::copy(textures.begin(), textures.end(), bytes.begin() + layout.textureOffset);
        for (size_t index = 0; index < value.tangents.size(); ++index) {
            const auto& tangent = value.tangents[index];
            auto* output = bytes.data() + layout.tangentOffset + index * textureTangentBytes;
            for (size_t axis = 0; axis < 3; ++axis)
                writeFloat(output + axis * 4, tangent.direction[axis]);
            writeFloat(output + 12, tangent.handedness);
            output[16] = tangent.known;
        }
        return bytes;
    }
    LegacyBakedInspection inspectLegacyBakedPayload(const uint8_t* bytes, size_t size, const LegacyBakedLimits& limits) { return describeLegacyBaked(bytes, size, limits).summary; }
    LegacyBakedPayload decodeLegacyBakedPayload(const uint8_t* bytes, size_t size, const LegacyBakedLimits& limits)
    {
        const auto shape = describeLegacyBaked(bytes, size, limits);
        LegacyBakedPayload value;
        value.geometry = decodeMeshPayload(bytes + legacyBakedPrefixBytes, shape.geometry, limits.geometry);
        value.uv = decodeUvPayload(bytes + shape.layout.uvOffset, shape.uv, shape.summary.triangleCount, limits.uv);
        value.tangents.reserve(static_cast<size_t>(shape.summary.triangleCount));
        for (uint64_t index = 0; index < shape.summary.triangleCount; ++index)
            value.tangents.push_back(readTangent(bytes + shape.layout.tangentOffset + index * textureTangentBytes));
        value.textures = decodeBakedTexturesPayload(bytes + shape.layout.textureOffset, shape.textures, limits.textures);
        return value;
    }
}
}
