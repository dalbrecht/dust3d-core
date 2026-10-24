// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_RIG_MATRIX_H_
#define DUST3D_NATIVE_RIG_MATRIX_H_
#include <array>

namespace dust3d {
namespace native {
    /// Owned column-major double matrix acting on column vectors; translation is at indices 12..14.
    /// This fixed value has no rig-generator, pose-owner or platform dependency.
    using RigMatrix = std::array<double, 16>;
}
}
#endif
