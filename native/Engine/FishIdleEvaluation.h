// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_FISH_IDLE_EVALUATION_H_
#define DUST3D_NATIVE_FISH_IDLE_EVALUATION_H_

#include "OwnedEvaluatedSkeletonClip.h"
#include <dust3d/animation/animation_generator.h>

namespace dust3d::native {
    /// Source-only prototype for the reference FishIdle clip.
    /// The caller must keep the grounded rig and its matching inverse binds immutable for the call.
    /// The result owns its pose data. The payload limit covers copied output and excludes evaluator
    /// workspace. Cancellation is checked around reference evaluation and during the copy.
    OwnedEvaluatedSkeletonClip evaluateFishIdle(const RigStructure& groundedRig,
        const std::map<std::string, Matrix4x4>& inverseBindMatrices,
        const AnimationParams& authoredParameters, size_t maximumPayloadBytes,
        const std::atomic_bool* cancellation = nullptr);
}
#endif
