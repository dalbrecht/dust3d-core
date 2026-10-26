// SPDX-License-Identifier: MIT
#include "WorkerIndexedCopyBudget.h"
#include <algorithm>
namespace dust3d {
namespace worker {
    uint64_t calculateIndexedCopyBudget(uint64_t indices, uint64_t warning, uint64_t nodes, uint64_t parts, const IndexedMeshPayloadLimits& limits)
    {
        validateIndexedMeshPayloadLimits(limits);
        if (!indices || indices % 3)
            throw IndexedMeshPayloadError(IndexedMeshPayloadErrorCode::invalidGeometry, "Native index export requires complete nonempty triangles.");
        if (indices > limits.maximumIndices || nodes > limits.maximumNodeCount || parts > limits.maximumPartCount || warning > limits.maximumWarningBytes)
            throw IndexedMeshPayloadError(IndexedMeshPayloadErrorCode::limitExceeded, "Native indexed metadata exceeds consumer counts.");
        if (warning >= limits.maximumDecodedBytes || indexedMeshPrefixBytes + warning >= limits.maximumWireBytes)
            throw IndexedMeshPayloadError(IndexedMeshPayloadErrorCode::limitExceeded, "Native indexed warning leaves no record allowance.");
        const auto records = std::min(limits.maximumDecodedBytes - warning, limits.maximumWireBytes - indexedMeshPrefixBytes - warning);
        // Supported count ceilings bound multiplication before any native indexing or allocation.
        const auto indexBytes = indices * sizeof(uint32_t);
        if (indexBytes >= records)
            throw IndexedMeshPayloadError(IndexedMeshPayloadErrorCode::limitExceeded, "Native indexed corners exhaust the record allowance.");
        const auto vertices = std::min(limits.maximumVertices, (records - indexBytes) / sizeof(MeshVertex));
        if (!vertices)
            throw IndexedMeshPayloadError(IndexedMeshPayloadErrorCode::limitExceeded, "Native indexed export requires at least one vertex record.");
        return indexBytes + vertices * sizeof(MeshVertex);
    }
}
}
