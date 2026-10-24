// SPDX-License-Identifier: MIT
#include "FbxMeshScenePlan.h"
#include "EngineInternal.h"
#include <cstdint>
#include <dust3d/base/retained_payload_budget.h>

namespace dust3d {
namespace native {
    FbxMeshSceneGeometryPlan planFbxMeshSceneGeometry(size_t vertices, size_t triangles, bool hasNormals, bool hasUvs)
    {
        if (vertices == 0 || triangles == 0)
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "FBX mesh scene requires positions and triangles.");
        if (vertices > 1000000 || triangles > 5000000)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "FBX mesh scene exceeds geometry counts.");
        RetainedPayloadBudget staging(FbxMeshSceneGeometryPlan::maximumStagingBytes);
        staging.tryAdd(vertices, 3 * sizeof(double));
        staging.tryAdd(triangles, 3 * sizeof(int32_t));
        if (hasNormals)
            staging.tryAdd(triangles, 9 * sizeof(double));
        if (hasUvs)
            staging.tryAdd(triangles, 6 * sizeof(double));
        if (staging.isExceeded())
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "FBX mesh scene staging arrays exceed budget.");
        // Admitted count ceilings make every scalar-count multiplication representable.
        return { vertices * 3, triangles * 3, hasNormals ? triangles * 9 : 0,
            hasUvs ? triangles * 6 : 0, staging.usedBytes() };
    }
}
}
