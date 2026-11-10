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

    inline constexpr bool isEvaluatedDirectionalGait(std::string_view type, std::string_view gait)
    {
        if (type.substr(0, gait.size()) != gait)
            return false;
        const auto direction = type.substr(gait.size());
        return direction.empty() || direction == "Left" || direction == "Right" || direction == "Backward"
            || direction == "ForwardLeft" || direction == "ForwardRight"
            || direction == "BackwardLeft" || direction == "BackwardRight";
    }

    inline constexpr std::string_view evaluatedClipRig(std::string_view type)
    {
        if (supportsFishClip(type))
            return "Fish";
        if (type == "SnakeForward" || type == "SnakeSlither" || type == "SnakeIdle"
            || type == "SnakeHurt" || type == "SnakeStrike" || type == "SnakeDie")
            return "Snake";
        if (type == "SpiderAttack" || type == "SpiderDie" || type == "SpiderHurt" || type == "SpiderIdle"
            || isEvaluatedDirectionalGait(type, "SpiderWalk") || isEvaluatedDirectionalGait(type, "SpiderRun"))
            return "Spider";
        if (type == "InsectAttack" || type == "InsectBite" || type == "InsectDie" || type == "InsectFly"
            || type == "InsectForward" || type == "InsectHurt" || type == "InsectIdle" || type == "InsectRubHands"
            || isEvaluatedDirectionalGait(type, "InsectWalk"))
            return "Insect";
        if (type == "BirdAttack" || type == "BirdDie" || type == "BirdEat" || type == "BirdFly"
            || type == "BirdForward" || type == "BirdGlide" || type == "BirdHurt" || type == "BirdIdle"
            || type == "BirdStrike" || isEvaluatedDirectionalGait(type, "BirdWalk")
            || isEvaluatedDirectionalGait(type, "BirdRun"))
            return "Bird";
        if (type == "QuadrupedAttack" || type == "QuadrupedDie" || type == "QuadrupedEat"
            || type == "QuadrupedHurt" || type == "QuadrupedIdle" || type == "QuadrupedRoar"
            || isEvaluatedDirectionalGait(type, "QuadrupedWalk") || isEvaluatedDirectionalGait(type, "QuadrupedRun"))
            return "Quadruped";
        return {};
    }

    /// Source-only prototype for bounded reference animation clips.
    /// The caller must keep the grounded rig and its matching inverse binds immutable for the call.
    /// The result owns its pose data. The payload limit covers copied output and excludes evaluator
    /// workspace. Cancellation is checked around reference evaluation and during the copy.
    OwnedEvaluatedSkeletonClip evaluateFishIdle(const RigStructure& groundedRig,
        const std::map<std::string, Matrix4x4>& inverseBindMatrices,
        const AnimationParams& authoredParameters, size_t maximumPayloadBytes,
        const std::atomic_bool* cancellation = nullptr, const FishIdleClipHandoff& = {}, const std::string& clipType = "FishIdle");
}
#endif
