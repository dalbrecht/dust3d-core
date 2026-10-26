// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_TANGENT_VALIDATION_H_
#define DUST3D_NATIVE_WORKER_TANGENT_VALIDATION_H_
#include <array>
#include <cmath>
#include <cstdint>
namespace dust3d {
namespace worker {
    namespace detail {
        /// Check copied finite tangent scalars and mapped-unit/UV correspondence without allocation.
        /// Unknown directions need not be unit length. Caller validates the UV flag separately.
        /// No exceptions. Example: isValidTextureTangent(direction, handedness, known, uvKnown).
        inline bool isValidTextureTangent(const std::array<float, 3>& direction, float handedness, uint8_t known, uint8_t uvKnown)
        {
            if (known > 1 || (handedness != 1 && handedness != -1))
                return false;
            for (const auto scalar : direction)
                if (!std::isfinite(scalar))
                    return false;
            if (!known)
                return true;
            if (!uvKnown)
                return false;
            const float length = direction[0] * direction[0] + direction[1] * direction[1] + direction[2] * direction[2];
            return std::isfinite(length) && std::abs(length - 1) < 0.0001F;
        }
    }
}
}
#endif
