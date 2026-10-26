// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_IMPORTED_CATALOG_VIEWS_H_
#define DUST3D_NATIVE_WORKER_IMPORTED_CATALOG_VIEWS_H_
#include "WorkerImportedMaterials.h"
#include "WorkerImportedSurface.h"
#include <functional>
#include <string_view>
namespace dust3d {
namespace worker {
    namespace detail {
        /// Independently inspect the complete immutable wire span before visiting borrowed
        /// catalog entries in order. Views may not escape the source lifetime. Callbacks
        /// are required and may throw original causes. No retained output arrays allocated.
        void visitImportedSurfaceModels(const uint8_t* bytes, size_t size, const ImportedSurfaceLimits& limits,
            const std::function<void(uint64_t, std::string_view)>& visitor);
        /// Same lifetime/inspection contract; visits identity and encoded-image presence.
        void visitImportedMaterialModels(const uint8_t* bytes, size_t size, const ImportedMaterialsLimits& limits,
            const std::function<void(uint64_t, std::string_view, bool)>& visitor);
    }
}
}
#endif
