// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_OPERATION_H_
#define DUST3D_NATIVE_WORKER_OPERATION_H_
#include "WorkerFailure.h"
#include "WorkerMesh.h"
#include <Dust3DBridge.h>

namespace dust3d {
namespace worker {
    /// Copy a reported native non-success status/cause into owned protocol data.
    /// @param error Native status and actual bounded diagnostic array, borrowed for the call.
    /// @return Independent mapped status/cause; unknown numeric statuses retain their code/cause.
    /// @throws ProtocolError if success was supplied as a failure; allocation errors propagate.
    FailurePayload copyNativeFailure(const Dust3DError& error);
    /// Copy expanded geometry from a stable borrowed native owner after complete geometry/text/copy preflight.
    /// @param owner Borrowed mesh; caller retains the owning mesh or UV owner until return.
    /// @param limits Validated supported consumer policy; does not bound native generation memory.
    /// @return Owned expanded geometry, warning and source counts; no native storage escapes.
    /// @throws MeshPayloadError or allocation errors; complete semantic preflight precedes retained allocation.
    /// Example: auto mesh = copyNativeMesh(dust3d_uv_mesh_geometry(owner), limits);
    MeshPayload copyNativeMesh(const Dust3DMesh* owner, const MeshPayloadLimits& limits);
    /// Generate one owned base-mesh/failure response through the serialized native C ABI.
    /// @param request Owned archive bytes and matching request identity/kind/length.
    /// @param limits Validated consumer output policy, checked before execution.
    /// @return Independent base-mesh or typed failure frame with matching identity.
    /// @throws ProtocolError/MeshPayloadError for invalid request/configuration; allocation errors may propagate.
    /// Invalid requests/configuration throw typed ProtocolError/MeshPayloadError.
    /// Operation failures preserve an owned typed reply when serialization is possible;
    /// transport/allocation failures can still propagate. Does not change global streams or save data.
    Frame generateMeshReply(const Frame& request, const MeshPayloadLimits& limits);
}
}
#endif
