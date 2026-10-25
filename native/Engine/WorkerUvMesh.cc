// SPDX-License-Identifier: MIT
#include "WorkerUvMesh.h"
#include "WorkerWire.h"
#include <algorithm>

namespace dust3d {
namespace worker {
    namespace {
        static_assert(maximumPayloadBytes <= std::numeric_limits<size_t>::max(), "Paired worker spans must fit size_t.");
        [[noreturn]] void reject(UvMeshPayloadErrorCode code, const char* message) { throw UvMeshPayloadError(code, message); }
        void validateLimits(const UvMeshPayloadLimits& limits)
        {
            if (!limits.maximumWireBytes || limits.maximumWireBytes > maximumPayloadBytes || !limits.maximumDecodedBytes || limits.maximumDecodedBytes > maximumPayloadBytes)
                reject(UvMeshPayloadErrorCode::invalidLimits, "Paired worker limits are unsupported.");
            validateMeshPayloadLimits(limits.geometry);
            validateUvPayloadLimits(limits.uv);
        }
        size_t wireSize(uint64_t meshBytes, uint64_t uvBytes, const UvMeshPayloadLimits& limits)
        {
            if (meshBytes < meshPrefixBytes || uvBytes < uvPrefixBytes)
                reject(UvMeshPayloadErrorCode::invalidLayout, "Paired worker subspans lack their complete prefixes.");
            if (uvMeshPrefixBytes > limits.maximumWireBytes || meshBytes > limits.maximumWireBytes - uvMeshPrefixBytes || uvBytes > limits.maximumWireBytes - uvMeshPrefixBytes - meshBytes)
                reject(UvMeshPayloadErrorCode::limitExceeded, "Paired worker response exceeds its aggregate wire quota.");
            return static_cast<size_t>(uvMeshPrefixBytes + meshBytes + uvBytes);
        }
        void decodedSize(uint64_t meshBytes, uint64_t uvBytes, const UvMeshPayloadLimits& limits)
        {
            if (meshBytes > limits.maximumDecodedBytes || uvBytes > limits.maximumDecodedBytes - meshBytes)
                reject(UvMeshPayloadErrorCode::limitExceeded, "Paired worker response exceeds its aggregate logical quota.");
        }
    }
    void validateUvMeshPayloadLimits(const UvMeshPayloadLimits& limits)
    {
        validateLimits(limits);
    }
    UvMeshPayloadError::UvMeshPayloadError(UvMeshPayloadErrorCode code, const char* message)
        : std::runtime_error(message)
        , m_code(code)
    {
    }
    std::vector<uint8_t> encodeUvMeshPayload(const UvMeshPayload& value, const UvMeshPayloadLimits& limits)
    {
        validateLimits(limits);
        // Reject profiles too small for even the fixed spans before encoding temporary segments.
        (void)wireSize(meshPrefixBytes, uvPrefixBytes, limits);
        const auto mesh = encodeMeshPayload(value.geometry, limits.geometry);
        const auto uv = encodeUvPayload(value.uv, value.geometry.vertices.size() / 3, limits.uv);
        const auto size = wireSize(mesh.size(), uv.size(), limits);
        const auto inspectedMesh = inspectMeshPayload(mesh.data(), mesh.size(), limits.geometry);
        const auto logicalUv = inspectUvPayload(uv.data(), uv.size(), inspectedMesh.vertexCount / 3, limits.uv);
        decodedSize(inspectedMesh.logicalBytes, logicalUv, limits);
        std::vector<uint8_t> result(size, 0);
        detail::writeUnsigned(result.data(), 1, 2);
        detail::writeUnsigned(result.data() + 2, uvMeshPrefixBytes, 2);
        detail::writeUnsigned(result.data() + 8, mesh.size(), 8);
        detail::writeUnsigned(result.data() + 16, uv.size(), 8);
        std::copy(mesh.begin(), mesh.end(), result.begin() + uvMeshPrefixBytes);
        std::copy(uv.begin(), uv.end(), result.begin() + uvMeshPrefixBytes + mesh.size());
        return result;
    }
    UvMeshPayload decodeUvMeshPayload(const uint8_t* bytes, size_t size, const UvMeshPayloadLimits& limits)
    {
        validateLimits(limits);
        if (!bytes)
            reject(UvMeshPayloadErrorCode::invalidInput, "Paired worker response bytes are missing.");
        if (size < uvMeshPrefixBytes)
            reject(UvMeshPayloadErrorCode::truncated, "Paired worker response prefix is incomplete.");
        if (detail::readUnsigned(bytes, 2) != 1)
            reject(UvMeshPayloadErrorCode::unsupportedVersion, "Paired worker response version is unsupported.");
        if (detail::readUnsigned(bytes + 2, 2) != uvMeshPrefixBytes || detail::readUnsigned(bytes + 4, 4))
            reject(UvMeshPayloadErrorCode::invalidLayout, "Paired worker response prefix or flags are invalid.");
        const auto meshBytes = detail::readUnsigned(bytes + 8, 8), uvBytes = detail::readUnsigned(bytes + 16, 8);
        const auto total = wireSize(meshBytes, uvBytes, limits);
        if (size < total)
            reject(UvMeshPayloadErrorCode::truncated, "Paired worker response subspans are incomplete.");
        if (size != total)
            reject(UvMeshPayloadErrorCode::trailingData, "Paired worker response has trailing bytes.");
        const auto* mesh = bytes + uvMeshPrefixBytes;
        const auto* uv = mesh + static_cast<size_t>(meshBytes);
        const auto inspectedMesh = inspectMeshPayload(mesh, static_cast<size_t>(meshBytes), limits.geometry);
        const auto logicalUv = inspectUvPayload(uv, static_cast<size_t>(uvBytes), inspectedMesh.vertexCount / 3, limits.uv);
        decodedSize(inspectedMesh.logicalBytes, logicalUv, limits);
        return { decodeMeshPayload(mesh, static_cast<size_t>(meshBytes), limits.geometry),
            decodeUvPayload(uv, static_cast<size_t>(uvBytes), inspectedMesh.vertexCount / 3, limits.uv) };
    }
}
}
