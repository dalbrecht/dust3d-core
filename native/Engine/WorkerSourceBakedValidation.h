// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_SOURCE_BAKED_VALIDATION_H_
#define DUST3D_NATIVE_WORKER_SOURCE_BAKED_VALIDATION_H_
#include "WorkerSourceBaked.h"
#include <string_view>
#include <unordered_set>
namespace dust3d {
namespace worker {
    namespace detail {
        struct BakedSurfaceMeasurement {
            uint64_t wireBytes, logicalBytes;
        };
        /// Validate counts/correspondence/quotas without output allocation. Throws SourceBakedError.
        /// Example: auto shape = measureBakedSurfaceCounts(triangles, models, components, text, expected, limits);
        BakedSurfaceMeasurement measureBakedSurfaceCounts(uint64_t triangles, uint64_t models, uint64_t components, uint64_t text, uint64_t expected, const BakedSurfaceLimits& limits);
        /// Validate a copied scalar record against catalog dimensions; no allocation.
        /// Throws SourceBakedError. Example: validateBakedSurfaceTriangle(record, models, components);
        void validateBakedSurfaceTriangle(const BakedSurfaceTriangle& value, uint64_t models, uint64_t components);
        /// Validate exact non-NUL UTF8 key/uniqueness in a caller-owned per-catalog workspace set.
        /// Key storage must outlive set usage. Throws SourceBakedError/allocation errors.
        /// Example: validateBakedCatalogKey(key, keys);
        void validateBakedCatalogKey(std::string_view key, std::unordered_set<std::string_view>& keys);
        /// Require original/generated membership; throws SourceBakedError. Example: validateBakedComponentKind(kind);
        void validateBakedComponentKind(uint32_t kind);
    }
}
}
#endif
