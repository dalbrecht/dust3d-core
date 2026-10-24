// SPDX-License-Identifier: MIT
#include "EngineInternal.h"
#include "ObjDocument.h"
#include <dust3d/base/retained_payload_budget.h>
namespace dust3d {
namespace native {
    ObjGeometryPlan planObjGeometry(size_t vertices, size_t polygons, size_t maximumBytes)
    {
        const auto maximum = maximumBytes ? maximumBytes : 64 * 1024 * 1024;
        if (maximum <= sizeof(OwnedObjDocument) || maximum > 256 * 1024 * 1024)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "OBJ quota must fit its owner and be at most256MiB.");
        if (vertices > 1000000 || polygons > 5000000)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "OBJ geometry exceeds source counts.");
        if (!polygons)
            throw DocumentError(DUST3D_STATUS_NO_GEOMETRY, "OBJ requires original polygon faces.");
        if (!vertices)
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "OBJ polygon faces require source positions.");
        RetainedPayloadBudget minimum(maximum);
        minimum.tryAdd(sizeof(OwnedObjDocument) + objDocumentHeader.size());
        minimum.tryAdd(vertices, 8);
        minimum.tryAdd(polygons, 8);
        if (minimum.isExceeded())
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "OBJ minimum text exceeds its quota.");
        return { maximum, minimum.usedBytes() };
    }
}
}
