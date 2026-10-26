// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_PROCEDURAL_IMAGES_NATIVE_H_
#define DUST3D_NATIVE_WORKER_PROCEDURAL_IMAGES_NATIVE_H_
#include "WorkerProceduralImages.h"
namespace dust3d {
namespace worker {
    /// Inspect one stable archive using the maintained validated metadata reader.
    /// @param archive Complete immutable readable archive, borrowed through return.
    /// @param limits Independent metadata/selected encoded-copy policies; excludes
    /// native parser/workspace/capacity/RSS. Validates policy before native reading.
    /// @return Owned selected encoded images in component-tree first-reference order.
    /// @throws Original native DocumentError/parser/observer cause, ProceduralImageError
    /// or allocation errors. Engine access serialized; no GLB/pixel decoding/generation.
    /// Example: auto inputs = loadProceduralImageInputs(archive, {});
    std::vector<ProceduralImageInput> loadProceduralImageInputs(const std::vector<uint8_t>& archive,
        const ProceduralImageLimits& limits);
}
}
#endif
