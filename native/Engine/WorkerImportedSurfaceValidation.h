// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_IMPORTED_SURFACE_VALIDATION_H_
#define DUST3D_NATIVE_WORKER_IMPORTED_SURFACE_VALIDATION_H_
#include "WorkerImportedSurface.h"
#include <string_view>
#include <unordered_set>
namespace dust3d {
namespace worker {
    namespace detail {
        struct ImportedSurfaceMeasurement {
            uint64_t wire, logical;
        };
        /// Pure count/quota/correspondence preflight; throws ImportedSurfaceError.
        ImportedSurfaceMeasurement measureImportedSurface(uint64_t triangles, uint64_t models, uint64_t components, uint64_t text, uint64_t expected, const ImportedMetadataLimits& limits);
        /// Pure authored record validation; throws ImportedSurfaceError.
        void validateImportedTriangle(const ImportedSurfaceTriangle& value, uint64_t models, uint64_t components);
        /// Borrowed key must remain stable while the workspace set lives; throws typed errors.
        void validateImportedKey(std::string_view key, std::unordered_set<std::string_view>& keys);
        /// Original/generated catalog membership only; throws ImportedSurfaceError.
        void validateImportedKind(uint32_t kind);
    }
}
}
#endif
