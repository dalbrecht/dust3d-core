// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_UV_VALIDATION_H_
#define DUST3D_NATIVE_WORKER_UV_VALIDATION_H_
#include "WorkerUv.h"
#include <string_view>
#include <unordered_set>
namespace dust3d {
namespace worker {
    namespace detail {
        /// Read one fixed 64-byte triangle only after complete inspectUvPayload succeeds.
        /// Caller guarantees the stable readable span is within that validated triangle table.
        /// Returns copied scalars; throws original UvPayloadError on invalid scalar/reserved bits.
        /// Example: auto triangle = readValidatedUvTriangle(validatedTriangleBytes);
        UvTriangle readValidatedUvTriangle(const uint8_t* bytes);
        /// Validate copied triangle scalars/references; throws original UvPayloadError, no allocation.
        /// Example: validateUvTriangle(value, componentCount, chartCount).
        void validateUvTriangle(const UvTriangle& value, uint64_t components, uint64_t charts);
        /// Validate chart bounds/color/image identity and references; throws original UvPayloadError.
        /// Example: validateUvChart(value, componentCount).
        void validateUvChart(const UvChart& value, uint64_t components);
        /// Validate gradient colors/resolution/chart reference; throws original UvPayloadError.
        /// Example: validateUvGradient(value, chartCount).
        void validateUvGradient(const UvGradient& value, uint64_t charts);
        /// Validate packed size against chart presence; throws original UvPayloadError.
        /// Example: validateUvTextureSize(value, chartCount).
        void validateUvTextureSize(double value, uint64_t charts);
        /// Validate borrowed exact key and uniqueness in a caller-owned workspace set.
        /// Storage must outlive set usage. Throws original UvPayloadError/allocation errors.
        /// Example: validateUvComponentKey(key, limits, keys).
        void validateUvComponentKey(std::string_view key, const UvPayloadLimits& limits, std::unordered_set<std::string_view>& keys);

    }
}
}
#endif
