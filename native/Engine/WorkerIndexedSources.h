// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_INDEXED_SOURCES_H_
#define DUST3D_NATIVE_WORKER_INDEXED_SOURCES_H_
#include "WorkerIndexedMesh.h"
#include "WorkerSourceHints.h"

namespace dust3d {
namespace worker {
    constexpr size_t indexedSourcesPrefixBytes = 24;
    /// One owned indexed geometry/source pair. Correspondence does not authenticate producer provenance.
    struct IndexedSourcesPayload {
        IndexedMeshPayload geometry;
        SourceHintPayload sources;
    };
    struct IndexedSourcesPayloadLimits {
        IndexedMeshPayloadLimits geometry;
        SourceHintPayloadLimits sources;
        uint64_t maximumWireBytes = maximumPayloadBytes;
        uint64_t maximumDecodedBytes = 128ULL * 1024 * 1024;
    };
    enum class IndexedSourcesPayloadErrorCode { invalidLimits,
        invalidInput,
        truncated,
        trailingData,
        unsupportedVersion,
        invalidLayout,
        countMismatch,
        limitExceeded };
    class IndexedSourcesPayloadError : public std::runtime_error {
    public:
        IndexedSourcesPayloadError(IndexedSourcesPayloadErrorCode code, const char* cause);
        IndexedSourcesPayloadErrorCode code() const noexcept { return m_code; }

    private:
        IndexedSourcesPayloadErrorCode m_code;
    };
    /// Validate independent inner policies and supported positive aggregate quotas.
    /// @param limits Wire/logical policies, not process memory limits.
    /// @throws Original inner errors or IndexedSourcesPayloadError(invalidLimits).
    /// Example: validateIndexedSourcesPayloadLimits({});
    void validateIndexedSourcesPayloadLimits(const IndexedSourcesPayloadLimits& limits);
    /// Encode one atomic pair after validating index/corner and node/identity correspondence and aggregate charge.
    /// @param value Borrowed geometry and source values; no generation is performed.
    /// @param limits Independent inner and aggregate quotas; temporary encoding workspace is excluded.
    /// @return Complete independent explicit-endian wire bytes.
    /// @throws Original inner errors, IndexedSourcesPayloadError or allocation errors; no partial result.
    /// Example: auto bytes = encodeIndexedSourcesPayload(value, {});
    std::vector<uint8_t> encodeIndexedSourcesPayload(const IndexedSourcesPayload& value, const IndexedSourcesPayloadLimits& limits);
    /// Inspect both full subspans and aggregate quotas before retaining decoded output arrays/text.
    /// @param bytes Stable borrowed complete wire bytes for this call.
    /// @param size Exact readable size; truncation/trailing data rejects.
    /// @param limits Independent inner and aggregate logical/wire quotas.
    /// @return Independent complete owned pair; no geometry escapes after source rejection.
    /// @throws Original inner errors, IndexedSourcesPayloadError or allocation errors.
    /// Example: auto pair = decodeIndexedSourcesPayload(bytes.data(), bytes.size(), {});
    IndexedSourcesPayload decodeIndexedSourcesPayload(const uint8_t* bytes, size_t size, const IndexedSourcesPayloadLimits& limits);
}
}
#endif
