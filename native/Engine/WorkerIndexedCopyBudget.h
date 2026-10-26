// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_INDEXED_COPY_BUDGET_H_
#define DUST3D_NATIVE_WORKER_INDEXED_COPY_BUDGET_H_
#include "WorkerIndexedMesh.h"
namespace dust3d {
namespace worker {
    /// Derive a positive native vertex/index record budget before indexing, without native work.
    /// @param indices Nonzero complete triangle-corner count from stable source metadata.
    /// @param warning Exact bounded warning byte count, charged separately from native records.
    /// @param nodes Original source node count.
    /// @param parts Original source part count.
    /// @param limits Supported consumer geometry policy, already tightened by paired source charge.
    /// @return Positive record bytes, bounding allowed indices/vertices and both copy/wire quotas.
    /// @throws IndexedMeshPayloadError for invalid policy/counts or insufficient record allowance.
    /// Excludes native lookup workspace, roots, capacity and RSS; allocates no output buffers.
    /// Example: auto budget = calculateIndexedCopyBudget(6, 10, 2, 1, limits);
    uint64_t calculateIndexedCopyBudget(uint64_t indices, uint64_t warning, uint64_t nodes, uint64_t parts, const IndexedMeshPayloadLimits& limits);
}
}
#endif
