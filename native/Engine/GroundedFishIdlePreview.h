// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_GROUNDED_FISH_IDLE_PREVIEW_H_
#define DUST3D_NATIVE_GROUNDED_FISH_IDLE_PREVIEW_H_

#include "FinalRigGeneration.h"
#include "FishIdleEvaluation.h"

namespace dust3d::native {
    /// Owns the grounded original mesh, exact source-order joints/bindings, and matching sampled poses.
    struct GroundedFishIdlePreview {
        FinalRigGeneration rest;
        OwnedEvaluatedSkeletonClip clip;
    };

    /// Source-only prototype. The retained quota covers the final rest data and copied clip together.
    /// Evaluator workspace and later draw expansion have separate costs.
    GroundedFishIdlePreview generateGroundedFishIdlePreview(const GeneratedRigInput&, const RigStructure&,
        const AnimationParams&, const RigFinalDataOptions&);
}
#endif
