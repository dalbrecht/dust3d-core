// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_MESH_COPY_INSPECTION_H_
#define DUST3D_NATIVE_WORKER_MESH_COPY_INSPECTION_H_
#include "WorkerMesh.h"
#include <Dust3DBridge.h>
namespace dust3d {
namespace worker {
    namespace detail {
        struct NativeMeshCopyInspection {
            uint64_t vertexCount, wire, logical;
            size_t warningBytes;
        };
        /// Validate stable expanded native geometry, warning text and independent copy/wire quotas.
        /// @param owner Live immutable borrowed mesh retained throughout this call.
        /// @param limits Supported positive geometry policy; excludes root/capacity/native workspace/RSS.
        /// @return Exact counts and charges without retained output arrays.
        /// @throws Original MeshPayloadError; no native generation or output allocation occurs.
        /// Example: auto shape = inspectNativeMeshCopy(owner, {});
        NativeMeshCopyInspection inspectNativeMeshCopy(const Dust3DMesh* owner, const MeshPayloadLimits& limits);
    }
}
}
#endif
