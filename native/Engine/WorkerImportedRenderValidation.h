// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_IMPORTED_RENDER_VALIDATION_H_
#define DUST3D_NATIVE_WORKER_IMPORTED_RENDER_VALIDATION_H_
#include "WorkerImportedRender.h"
namespace dust3d {
namespace worker {
    namespace detail {
        /// Validate tight straight RGBA8 shape/presence using scalar counts only.
        /// Throws ImportedRenderError; no pixel access/allocation. Example:
        /// validateImportedRenderImage(image.width, image.height, image.pixels.size(), true);
        void validateImportedRenderImage(uint64_t width, uint64_t height, uint64_t bytes, bool present);
    }
}
}
#endif
