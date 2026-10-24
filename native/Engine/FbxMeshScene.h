// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_FBX_MESH_SCENE_H_
#define DUST3D_NATIVE_FBX_MESH_SCENE_H_
#include "FbxDocument.h"
#include <array>
#include <dust3d/base/object.h>
namespace dust3d {
namespace native {
    /// One composed atlas material; PNG channels are color, normal, metallic, roughness, emissive and AO.
    /// Nonempty maps must be validated encoded PNGs from the caller's image boundary.
    /// This value owns its bytes; export signature checks do not decode or establish image safety.
    struct FbxSceneMaterial {
        std::array<double, 4> baseColor { 1, 1, 1, 1 };
        double metallic = 0;
        double roughness = 1;
        std::array<std::vector<uint8_t>, 6> pngMaps;
    };
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
    /// Encode one atlas material and embedded PNG maps with the static geometry scene.
    /// Scalars must be finite0...1; map bytes total at most64MiB and present maps require complete UVs.
    /// Uses deterministic internal names, never caller paths. Inputs stay alive/unchanged during the call;
    /// returned bytes are independent. Throws DocumentError INVALID_DOCUMENT for invalid scalar/signature/UV
    /// input or LIMIT_EXCEEDED for quotas; allocation errors propagate. No image decoding, files, rig,
    /// animation or source baking. Example: encodeFbxTexturedMeshScene(mesh, material).
    OwnedFbxDocument encodeFbxTexturedMeshScene(const Object& source, const FbxSceneMaterial& material, FbxDocumentLimits limits = {});
}
}
#endif
