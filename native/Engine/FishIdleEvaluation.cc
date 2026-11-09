// SPDX-License-Identifier: MIT
#include "FishIdleEvaluation.h"
#include "EngineInternal.h"
#include <dust3d/animation/parameter_catalog.h>
#include <algorithm>
#include <charconv>
#include <cmath>

namespace dust3d::native {
    namespace {
        double authoredNumber(const std::string& text)
        {
            if (text.empty() || text.size() > 64)
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Fish loop parameter is not a bounded decimal.");
            auto decimal = text;
            std::replace(decimal.begin(), decimal.end(), ',', '.');
            const auto* end = decimal.data() + decimal.size();
            const auto* start = decimal.data() + (decimal.front() == '+');
            double value = 0;
            const auto result = std::from_chars(start, end, value);
            if (result.ec != std::errc {} || result.ptr != end || !std::isfinite(value)
                || (decimal.front() == '+' && start != end && *start == '-'))
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Fish loop parameter is not a finite decimal.");
            return value;
        }
    }

    OwnedEvaluatedSkeletonClip evaluateFishIdle(const RigStructure& groundedRig,
        const std::map<std::string, Matrix4x4>& inverseBindMatrices,
        const AnimationParams& authoredParameters, size_t maximumPayloadBytes,
        const std::atomic_bool* cancellation, const FishIdleClipHandoff& handoff, const std::string& clipType)
    {
        const auto checkCancellation = [cancellation] {
            if (cancellation && cancellation->load(std::memory_order_relaxed))
                throw DocumentError(DUST3D_STATUS_CANCELLED, "Fish loop evaluation cancelled.");
        };
        checkCancellation();
        if (!supportsFishLoop(clipType) || groundedRig.type != "Fish" || groundedRig.bones.empty()
            || inverseBindMatrices.size() != groundedRig.bones.size())
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Fish loop requires a Fish rig and its inverse binds.");

        const auto& controls = animation::parameterDefinitions(clipType);
        if (authoredParameters.values.size() > controls.size() + 2)
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Fish loop has unsupported parameters.");
        auto [duration, defaultFrameCount] = AnimationGenerator::defaultTiming(clipType);
        double frameCount = defaultFrameCount;
        for (const auto& [name, rawValue] : authoredParameters.values) {
            const auto value = authoredNumber(rawValue);
            if (name == "durationSeconds") {
                duration = value;
            } else if (name == "frameCount") {
                frameCount = value;
            } else {
                const auto control = std::find_if(controls.begin(), controls.end(),
                    [&parameterName = name](const auto& candidate) { return candidate.parameterName == parameterName; });
                if (control == controls.end()
                    || value < animation::parameterValue(*control, control->minimumSliderValue)
                    || value > animation::parameterValue(*control, control->maximumSliderValue))
                    throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Fish loop parameter is outside its declared range.");
            }
        }
        const auto& timing = animation::commonTimingDefinition();
        if (!std::isfinite(duration) || duration < timing.minimumDurationSeconds || duration > timing.maximumDurationSeconds
            || !std::isfinite(frameCount) || frameCount < 2 || frameCount > timing.maximumFrameCount
            || std::trunc(frameCount) != frameCount)
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Fish loop timing is outside its declared sampling range.");

        // The reference evaluator appends one closing sample after generating the requested frames.
        const auto retainedFrameCount = static_cast<size_t>(frameCount) + 1;
        const auto jointCount = groundedRig.bones.size();
        if (!animation::animationFrameWorkFits(retainedFrameCount, jointCount))
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Fish loop exceeds the joint-frame limit.");
        const auto retainedBytes = clipType.size()
            + retainedFrameCount * (sizeof(double) + jointCount * sizeof(RigMatrix));
        if (retainedBytes > std::min(maximumPayloadBytes, size_t { 64 * 1024 * 1024 }))
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Fish loop exceeds its copied payload limit.");

        checkCancellation();
        RigAnimationClip clip;
        if (!AnimationGenerator::generate(groundedRig, inverseBindMatrices, clip, clipType, authoredParameters))
            throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "Reference Fish loop evaluation rejected the rig or clip.");
        checkCancellation();
        if (clip.animationType != clipType || !clip.loop || clip.frames.size() != retainedFrameCount
            || clip.durationSeconds != static_cast<float>(duration) || clip.frames.back().time != clip.durationSeconds)
            throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "Reference Fish loop returned unexpected clip timing.");
        auto owned = OwnedEvaluatedSkeletonClip::copy(clip, groundedRig, maximumPayloadBytes, cancellation);
        if (handoff) {
            checkCancellation();
            const auto remaining = maximumPayloadBytes - owned.logicalPayloadBytes();
            if (handoff(clip, remaining) > remaining)
                throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Fish loop derived output exceeds its retained quota.");
            checkCancellation();
        }
        return owned;
    }
}
