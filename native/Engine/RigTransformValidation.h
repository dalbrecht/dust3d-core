// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_RIG_TRANSFORM_VALIDATION_H_
#define DUST3D_NATIVE_RIG_TRANSFORM_VALIDATION_H_
#include "RigMatrix.h"
namespace dust3d {
namespace native {
    enum class RigTransformUse { localInput,
        worldInput,
        generatedWorld,
        generatedSkin };
    /// Validate a finite affine right-handed unit rigid matrix using existing pose
    /// tolerances. Local/world/skin translation bounds are 1M/128M/256M. Invalid
    /// input contexts throw DocumentError INVALID_DOCUMENT, generated contexts
    /// GENERATION_FAILED; coordinate bounds always LIMIT_EXCEEDED. Unknown use
    /// rejects as INVALID_DOCUMENT. Input remains unchanged; no globals/ownership.
    /// Example: validateRigTransform(world, RigTransformUse::worldInput).
    void validateRigTransform(const RigMatrix& matrix, RigTransformUse use);
}
}
#endif
