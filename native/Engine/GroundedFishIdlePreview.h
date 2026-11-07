// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_GROUNDED_FISH_IDLE_PREVIEW_H_
#define DUST3D_NATIVE_GROUNDED_FISH_IDLE_PREVIEW_H_

#include "GeneratedRigDrawOwner.h"
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

    struct GroundedFishIdleDrawPreview {
        std::unique_ptr<Dust3DGeneratedRig> rest;
        OwnedEvaluatedSkeletonClip clip;
    };

    /// One staged draw owner and matching pose records. The retained aggregate quota also
    /// reserves the concurrently held original rest result and the temporary draw-corner map.
    GroundedFishIdleDrawPreview generateGroundedFishIdleDrawPreview(const GeneratedRigInput&,
        std::unique_ptr<Dust3DMesh> metadata, const RigStructure&, const AnimationParams&,
        const RigFinalDataOptions&, size_t maximumDrawVertices, const Dust3DCancellationToken*);
}
#endif
