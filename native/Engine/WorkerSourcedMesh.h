// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_SOURCED_MESH_H_
#define DUST3D_NATIVE_WORKER_SOURCED_MESH_H_
#include "WorkerMesh.h"
#include "WorkerSourceHints.h"

namespace dust3d {
namespace worker {
    constexpr size_t sourcedMeshPrefixBytes = 24;
    /// One owned expanded geometry/source pair. Correspondence does not authenticate producer provenance.
    struct SourcedMeshPayload {
        MeshPayload geometry;
        SourceHintPayload sources;
    };
    struct SourcedMeshPayloadLimits {
        MeshPayloadLimits geometry;
        SourceHintPayloadLimits sources;
        uint64_t maximumWireBytes = maximumPayloadBytes;
        uint64_t maximumDecodedBytes = 128ULL * 1024 * 1024;
    };
    enum class SourcedMeshPayloadErrorCode { invalidLimits,
        invalidInput,
        truncated,
        trailingData,
        unsupportedVersion,
        invalidLayout,
        countMismatch,
        limitExceeded };
    class SourcedMeshPayloadError : public std::runtime_error {
    public:
        SourcedMeshPayloadError(SourcedMeshPayloadErrorCode code, const char* cause);
        SourcedMeshPayloadErrorCode code() const noexcept { return m_code; }

    private:
        SourcedMeshPayloadErrorCode m_code;
    };
    /// Validate independent inner policies and supported positive aggregate quotas.
    /// @param limits Wire/logical policies, not process memory limits.
    /// @throws Original inner errors or SourcedMeshPayloadError(invalidLimits).
    /// Example: validateSourcedMeshPayloadLimits({});
    void validateSourcedMeshPayloadLimits(const SourcedMeshPayloadLimits& limits);
    /// Encode one atomic pair after validating count correspondence and aggregate charge.
    /// @param value Borrowed geometry and source values; no generation is performed.
    /// @param limits Independent inner and aggregate quotas; temporary encoding workspace is excluded.
    /// @return Complete independent explicit-endian wire bytes.
    /// @throws Original inner errors, SourcedMeshPayloadError or allocation errors; no partial result.
    /// Example: auto bytes = encodeSourcedMeshPayload(value, {});
    std::vector<uint8_t> encodeSourcedMeshPayload(const SourcedMeshPayload& value, const SourcedMeshPayloadLimits& limits);
    /// Inspect both full subspans and aggregate quotas before retaining decoded output arrays/text.
    /// @param bytes Stable borrowed complete wire bytes for this call.
    /// @param size Exact readable size; truncation/trailing data rejects.
    /// @param limits Independent inner and aggregate logical/wire quotas.
    /// @return Independent complete owned pair; no geometry escapes after source rejection.
    /// @throws Original inner errors, SourcedMeshPayloadError or allocation errors.
    /// Example: auto pair = decodeSourcedMeshPayload(bytes.data(), bytes.size(), {});
    SourcedMeshPayload decodeSourcedMeshPayload(const uint8_t* bytes, size_t size, const SourcedMeshPayloadLimits& limits);
}
}
#endif
