// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_INDEXED_MESH_H_
#define DUST3D_NATIVE_WORKER_INDEXED_MESH_H_
#include "WorkerMesh.h"
namespace dust3d {
namespace worker {
    constexpr size_t indexedMeshPrefixBytes = 48;
    constexpr uint64_t maximumMeshIndices = maximumPayloadBytes / 4;
    /// Owned vertex table and exact triangle-corner index order; no deduplication or normalization.
    struct IndexedMeshPayload {
        std::vector<MeshVertex> vertices;
        std::vector<uint32_t> indices;
        uint64_t nodeCount = 0, partCount = 0;
        std::string warning;
    };
    /// Logical retained charge includes vertex/index records and exact warning bytes.
    /// Root objects, capacity, workspace and RSS are excluded.
    struct IndexedMeshPayloadLimits {
        uint64_t maximumWireBytes = maximumPayloadBytes;
        uint64_t maximumDecodedBytes = 128ULL * 1024 * 1024;
        uint64_t maximumVertices = 3000000;
        uint64_t maximumIndices = 9000000;
        uint64_t maximumNodeCount = maximumSourceCount, maximumPartCount = maximumSourceCount;
        uint64_t maximumWarningBytes = 4096;
    };
    struct IndexedMeshPayloadInspection {
        uint64_t vertexCount, indexCount, nodeCount, partCount, logicalBytes;
    };
    enum class IndexedMeshPayloadErrorCode { invalidInput,
        invalidLimits,
        invalidGeometry,
        invalidIndex,
        unsupportedVersion,
        invalidLayout,
        truncated,
        trailingData,
        limitExceeded };
    class IndexedMeshPayloadError : public std::runtime_error {
    public:
        IndexedMeshPayloadError(IndexedMeshPayloadErrorCode code, const char* cause);
        IndexedMeshPayloadErrorCode code() const noexcept { return m_code; }

    private:
        IndexedMeshPayloadErrorCode m_code;
    };
    /// Validate supported positive count/wire/logical quotas without generation or output allocation.
    /// Throws IndexedMeshPayloadError(invalidLimits). Example: validateIndexedMeshPayloadLimits({});
    void validateIndexedMeshPayloadLimits(const IndexedMeshPayloadLimits& limits);
    /// Validate a complete owned indexed value and encode explicit-endian version-one bytes.
    /// @param value Borrowed vertex table, corner indices, counts and warning; ordering/bits preserved.
    /// @param limits Supported byte/count policy, excluding workspace and RSS.
    /// @return Complete independent wire bytes after all indices/vertices/triangles/text validate.
    /// @throws IndexedMeshPayloadError, original MeshPayloadError or allocation errors; no partial result.
    /// Example: auto bytes = encodeIndexedMeshPayload(value, {});
    std::vector<uint8_t> encodeIndexedMeshPayload(const IndexedMeshPayload& value, const IndexedMeshPayloadLimits& limits);
    /// Validate exact framing and every vertex/index/triangle before owned output arrays/text.
    /// @param bytes Stable borrowed exact readable span for this call.
    /// @param size Actual byte count; incomplete/trailing spans reject.
    /// @param limits Supported byte/count/geometry policy.
    /// @return Validated table/corner/source counts and logical retained charge.
    /// @throws IndexedMeshPayloadError or original MeshPayloadError; no decoded arrays are constructed.
    /// Example: auto summary = inspectIndexedMeshPayload(bytes.data(), bytes.size(), {});
    IndexedMeshPayloadInspection inspectIndexedMeshPayload(const uint8_t* bytes, size_t size, const IndexedMeshPayloadLimits& limits);
    /// Return independent values only after complete inspectIndexedMeshPayload validation.
    /// @param bytes Stable borrowed complete wire span; released/mutable after return.
    /// @param size Exact readable count.
    /// @param limits Supported output policy; no native generation is performed.
    /// @return Owned vertex/index/warning storage with source counts.
    /// @throws IndexedMeshPayloadError, original MeshPayloadError or allocation errors; no partial result.
    /// Example: auto value = decodeIndexedMeshPayload(bytes.data(), bytes.size(), {});
    IndexedMeshPayload decodeIndexedMeshPayload(const uint8_t* bytes, size_t size, const IndexedMeshPayloadLimits& limits);
}
}
#endif
