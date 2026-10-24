// SPDX-License-Identifier: MIT
#include "FbxMaterialImagePlan.h"
#include "EngineInternal.h"
#include <dust3d/base/retained_payload_budget.h>

namespace dust3d {
namespace native {
    FbxMaterialImagePlan planFbxMaterialImages(const std::array<size_t, 6>& mapBytes, bool hasUvs)
    {
        RetainedPayloadBudget images(FbxMaterialImagePlan::maximumEncodedBytes);
        size_t count = 0;
        for (size_t bytes : mapBytes) {
            if (bytes == 0)
                continue;
            if (!images.tryAdd(bytes))
                throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "FBX embedded PNGs exceed the aggregate image budget.");
            if (bytes < 8)
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "FBX embedded images require prevalidated encoded PNG bytes.");
            if (!hasUvs)
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "FBX embedded textures require complete corner UVs.");
            ++count;
        }
        return { images.usedBytes(), count };
    }
}
}
