// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_MESH_H_
#define DUST3D_NATIVE_WORKER_MESH_H_
#include "WorkerProtocol.h"
#include <string>

namespace dust3d {
namespace worker {
    constexpr size_t meshPrefixBytes = 40;
    constexpr size_t meshVertexBytes = 40;
    constexpr uint64_t maximumMeshVertices = maximumPayloadBytes / meshVertexBytes;
    constexpr uint64_t maximumSourceCount = 1000000;

    /// Owned expanded geometry value, independent of C ABI layout and native handles.
    struct MeshVertex {
        std::array<float, 3> position;
        std::array<float, 3> normal;
        std::array<float, 4> color;
    };
    struct MeshPayload {
        std::vector<MeshVertex> vertices;
        uint64_t nodeCount = 0;
        uint64_t partCount = 0;
        std::string warning;
    };
    /// Decoded quota counts logical vertex records plus warning bytes, excluding roots/capacities/RSS.
    struct MeshPayloadLimits {
        uint64_t maximumWireBytes = maximumPayloadBytes;
        uint64_t maximumDecodedBytes = 128ULL * 1024 * 1024;
        uint64_t maximumVertices = 3000000;
        uint64_t maximumNodeCount = maximumSourceCount;
        uint64_t maximumPartCount = maximumSourceCount;
        uint64_t maximumWarningBytes = 4096;
    };
    enum class MeshPayloadErrorCode { invalidLimits,
        invalidInput,
        unsupportedVersion,
        invalidLayout,
        truncated,
        trailingData,
        limitExceeded,
        invalidGeometry,
        invalidText };
    /// Typed semantic failure; diagnostics do not interpolate worker/document contents.
    class MeshPayloadError : public std::runtime_error {
    public:
        MeshPayloadError(MeshPayloadErrorCode code, const char* message);
        MeshPayloadErrorCode code() const noexcept { return m_code; }

    private:
        MeshPayloadErrorCode m_code;
    };
    /// Encode base triangle geometry with an explicit version-one, big-endian IEEE-754 schema.
    /// Validates quotas, finite/unit/range/area invariants and UTF-8 before output allocation.
    /// Throws MeshPayloadError or an ordinary allocation exception. No native object dump is used.
    std::vector<uint8_t> encodeMeshPayload(const MeshPayload& mesh, const MeshPayloadLimits& limits);
    /// Decode one exact complete payload, validating every triangle before returning owned storage.
    /// NULL/truncation/trailing data, invalid schema, quotas, text or geometry throw MeshPayloadError.
    /// Empty geometry is not success. This schema has no UV/material/rig/animation fields.
    MeshPayload decodeMeshPayload(const uint8_t* bytes, size_t size, const MeshPayloadLimits& limits);
}
}
#endif
