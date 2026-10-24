// SPDX-License-Identifier: MIT
#include "FbxBakedMaterial.h"
#include "EngineInternal.h"
#include "PngEncoding.h"
#include <dust3d/base/retained_payload_budget.h>
namespace dust3d {
namespace native {
    FbxSceneMaterial makeFbxBakedMaterial(const FbxBakedImageViews& images, size_t maximumBytes)
    {
        const auto maximum = maximumBytes ? maximumBytes : 64 * 1024 * 1024;
        if (maximum < 57 || maximum > 64 * 1024 * 1024)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Baked FBX PNG capacity quota must be57bytes...64MiB.");
        if (!images[0])
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Baked FBX material requires its color atlas.");
        // Validate all spans before any image compression, including optional later channels.
        for (const auto* image : images) {
            if (image)
                planPngEncoding(image->width, image->height, image->pixels.size());
        }
        FbxSceneMaterial result;
        result.metallic = images[2] ? 1 : 0;
        RetainedPayloadBudget retained(maximum);
        for (size_t channel = 0; channel < images.size(); ++channel) {
            if (!images[channel])
                continue;
            const auto remaining = maximum - retained.usedBytes();
            if (remaining < 57)
                throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Baked FBX material has no PNG capacity budget remaining.");
            result.pngMaps[channel] = encodePng(*images[channel], remaining);
            if (!retained.tryAdd(result.pngMaps[channel].capacity()))
                throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Baked FBX PNG capacities exceed aggregate quota.");
        }
        return result;
    }
}
}
