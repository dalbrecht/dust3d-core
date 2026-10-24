// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_FBX_MESH_SCENE_H_
#define DUST3D_NATIVE_FBX_MESH_SCENE_H_
#include "FbxDocument.h"
#include <dust3d/base/object.h>
namespace dust3d {
namespace native {
    /// Encode a static geometry scene with original indexed positions, triangle
    /// order, optional complete corner normals and UVs (reference V flip), and
    /// reference Y-up/unit metadata. Source stays unchanged/alive during the call;
    /// returned bytes are independent. Geometry only: no material, skin, animation
    /// or image export; callers must not present this as complete document export.
    /// Bounds: 1M vertices, 5M triangles, finite positions within +/-4M; finite
    /// normals/UVs within +/-1M. Triangles must have three valid vertex indices.
    /// Invalid inputs throw DocumentError INVALID_DOCUMENT, bounds/quotas throw
    /// LIMIT_EXCEEDED; allocation failures propagate. Document limits apply to
    /// output; staging arrays are separately bounded to256MiB, not total RSS.
    /// No globals, files or native engine calls. Example: encodeFbxMeshScene(mesh).
    OwnedFbxDocument encodeFbxMeshScene(const Object& source, FbxDocumentLimits limits = {});
}
}
#endif
