// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_FISH_IDLE_EVALUATION_H_
#define DUST3D_NATIVE_FISH_IDLE_EVALUATION_H_

#include "OwnedEvaluatedSkeletonClip.h"
#include <dust3d/animation/animation_generator.h>
#include <functional>
#include <string_view>

namespace dust3d::native {
    /// Borrow the exact evaluated clip and stage bounded derived output synchronously.
    /// Returns its retained logical bytes. Must not publish output or reacquire engineMutex.
    using FishIdleClipHandoff = std::function<size_t(const RigAnimationClip&, size_t remainingBytes)>;

    inline constexpr bool supportsFishLoop(std::string_view type)
    {
        return type == "FishIdle" || type == "FishSwim" || type == "FishForward";
    }

    inline constexpr bool supportsFishClip(std::string_view type)
    {
        return supportsFishLoop(type) || type == "FishAttack" || type == "FishHurt" || type == "FishDie";
    }

    /// Source-only prototype for bounded reference Fish clips.
    /// The caller must keep the grounded rig and its matching inverse binds immutable for the call.
    /// The result owns its pose data. The payload limit covers copied output and excludes evaluator
    /// workspace. Cancellation is checked around reference evaluation and during the copy.
    OwnedEvaluatedSkeletonClip evaluateFishIdle(const RigStructure& groundedRig,
        const std::map<std::string, Matrix4x4>& inverseBindMatrices,
        const AnimationParams& authoredParameters, size_t maximumPayloadBytes,
        const std::atomic_bool* cancellation = nullptr, const FishIdleClipHandoff& = {}, const std::string& clipType = "FishIdle");
}
#endif
