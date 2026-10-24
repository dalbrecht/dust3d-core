// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_FBX_ATLAS_GEOMETRY_H_
#define DUST3D_NATIVE_FBX_ATLAS_GEOMETRY_H_
#include "FbxGeometrySelection.h"
#include <dust3d/uv/uv_atlas_builder.h>
namespace dust3d {
namespace native {
    /// Copy original geometry using complete packed atlas UVs, never local/preview UVs.
    /// @param source Original Object used to construct the supplied atlas; remains unchanged.
    /// @param atlas Source-face/corner-aligned packed UVs with every known marker exactly1.
    /// @param maximumBytes Logical selection quota, zero64MiB, maximum256MiB.
    /// @return Independent geometry owner preserving all original positions, indices and faces.
    /// @throws DocumentError INVALID_DOCUMENT for incomplete/unknown UVs or geometry;
    /// LIMIT_EXCEEDED for selection quotas/coordinate bounds. Allocation failures propagate.
    /// Caller establishes common provenance; equal array counts alone cannot prove identity.
    /// Temporary source-index storage and caller atlas/source memory are outside the owner quota.
    /// Example: selectFbxAtlasGeometry(object, metadata.atlas()).object().
    FbxGeometrySelection selectFbxAtlasGeometry(const Object& source, const UvAtlas& atlas, size_t maximumBytes = 0);
}
}
#endif
