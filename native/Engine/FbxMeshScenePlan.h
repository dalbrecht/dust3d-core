// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_FBX_MESH_SCENE_PLAN_H_
#define DUST3D_NATIVE_FBX_MESH_SCENE_PLAN_H_
#include <cstddef>

namespace dust3d {
namespace native {
    /// Logical staging-array counts; excludes source/encoded buffers and allocator overhead.
    struct FbxMeshSceneGeometryPlan {
        static constexpr size_t maximumStagingBytes = 256 * 1024 * 1024;
        size_t positionValueCount;
        size_t polygonIndexCount;
        size_t normalValueCount;
        size_t textureCoordinateValueCount;
        size_t stagingBytes;
    };
    /// Admit counts before any geometry allocation or traversal. No source arrays are inspected.
    /// @param vertices Position count, 1...1M.
    /// @param triangles Triangle count, 1...5M.
    /// @param hasNormals Whether complete three-component corner normals will be staged.
    /// @param hasUvs Whether complete two-component corner UVs will be staged.
    /// @return Exact scalar counts and logical aggregate staging bytes, at most256MiB.
    /// @throws DocumentError INVALID_DOCUMENT for zero counts or LIMIT_EXCEEDED for bounds/budget.
    /// Example: planFbxMeshSceneGeometry(4, 2, true, true).stagingBytes == 360.
    FbxMeshSceneGeometryPlan planFbxMeshSceneGeometry(size_t vertices, size_t triangles, bool hasNormals, bool hasUvs);
}
}
#endif
