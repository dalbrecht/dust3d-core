// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_UV_MESH_H_
#define DUST3D_NATIVE_WORKER_UV_MESH_H_
#include "WorkerMesh.h"
#include "WorkerUv.h"

namespace dust3d {
namespace worker {
    constexpr size_t uvMeshPrefixBytes = 24;
    /// One atomic response value. Matching counts do not authenticate producer provenance.
    struct UvMeshPayload {
        MeshPayload geometry;
        UvPayload uv;
    };
    struct UvMeshPayloadLimits {
        MeshPayloadLimits geometry;
        UvPayloadLimits uv;
        uint64_t maximumWireBytes = maximumPayloadBytes;
        uint64_t maximumDecodedBytes = 128ULL * 1024 * 1024;
    };
    enum class UvMeshPayloadErrorCode { invalidLimits,
        invalidInput,
        unsupportedVersion,
        invalidLayout,
        truncated,
        trailingData,
        limitExceeded };
    class UvMeshPayloadError : public std::runtime_error {
    public:
        UvMeshPayloadError(UvMeshPayloadErrorCode code, const char* message);
        UvMeshPayloadErrorCode code() const noexcept { return m_code; }

    private:
        UvMeshPayloadErrorCode m_code;
    };
    /// Encode a complete explicit-endian paired response after independent/aggregate validation.
    /// @param value Geometry and UV data with matching nonzero triangle count.
    /// @param limits Separate inner policies and aggregate wire/logical quotas; not RSS limits.
    /// @return Owned complete wire bytes; intermediate encoded spans are temporary workspace.
    /// @throws Original MeshPayloadError/UvPayloadError, paired framing errors or allocation errors.
    /// Example: auto wire = encodeUvMeshPayload(pair, {});
    std::vector<uint8_t> encodeUvMeshPayload(const UvMeshPayload& value, const UvMeshPayloadLimits& limits);
    /// Validate both full spans and aggregate logical charge before constructing returned arrays.
    /// @param bytes Stable borrowed complete response storage, only for this call.
    /// @param size Exact byte span; trailing bytes reject.
    /// @param limits Independent inner/aggregate policies; validation workspace is excluded.
    /// @return Independent complete pair only; no geometry is returned after UV rejection.
    /// @throws Original inner errors, paired framing errors or allocation errors. No partial result.
    /// Example: auto pair = decodeUvMeshPayload(wire.data(), wire.size(), {});
    UvMeshPayload decodeUvMeshPayload(const uint8_t* bytes, size_t size, const UvMeshPayloadLimits& limits);
}
}
#endif
