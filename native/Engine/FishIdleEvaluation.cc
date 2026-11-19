// SPDX-License-Identifier: MIT
#include "FishIdleEvaluation.h"
#include "EngineInternal.h"
#include <algorithm>
#include <cmath>
#include <dust3d/animation/animation_catalog.h>
#include <dust3d/animation/parameter_catalog.h>
#include <locale>
#include <sstream>

namespace dust3d::native {
namespace {
    double authoredNumber(const std::string& text)
    {
        if (text.empty() || text.size() > 64)
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Animation clip parameter is not a bounded decimal.");
        auto decimal = text;
        std::replace(decimal.begin(), decimal.end(), ',', '.');
        // Apple's floating-point from_chars requires macOS 26.
        std::istringstream input(decimal);
        input.imbue(std::locale::classic());
        double value = 0;
        if (!(input >> std::noskipws >> value) || !input.eof() || !std::isfinite(value))
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Animation clip parameter is not a finite decimal.");
        return value;
    }
}

size_t evaluatedClipParameterLimit(const std::string& clipType)
{
    // Authored parameters include two timing fields alongside the pinned controls.
    return animation::parameterDefinitions(clipType).size() + 2;
}

OwnedEvaluatedSkeletonClip evaluateFishIdle(const RigStructure& groundedRig,
    const std::map<std::string, Matrix4x4>& inverseBindMatrices,
    const AnimationParams& authoredParameters, size_t maximumPayloadBytes,
    const std::atomic_bool* cancellation, const FishIdleClipHandoff& handoff, const std::string& clipType)
{
    const auto checkCancellation = [cancellation] {
        if (cancellation && cancellation->load(std::memory_order_relaxed))
            throw DocumentError(DUST3D_STATUS_CANCELLED, "Animation clip evaluation cancelled.");
    };
    checkCancellation();
    const auto expectedRig = evaluatedClipRig(clipType);
    const auto& catalog = animation::clipCatalog();
    const auto definition = catalog.find(clipType);
    if (expectedRig.empty() || expectedRig != groundedRig.type || definition == catalog.end()
        || definition->second.rig != groundedRig.type || groundedRig.bones.empty()
        || inverseBindMatrices.size() != groundedRig.bones.size())
        throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Animation clip requires its matching rig and inverse binds.");

    const auto& controls = animation::parameterDefinitions(clipType);
    if (authoredParameters.values.size() > controls.size() + 2)
        throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Animation clip has unsupported parameters.");
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
            if (control == controls.end())
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Animation clip has an unknown parameter.");
            // The reference walk and its catalog-derived directional clips share
            // still-arm support. Its T. rex example authors 0.2 below the
            // editor slider's 0.25 minimum. Other generators retain their bounds.
            const auto minimum = definition->second.generator == "BipedWalk" && name == "armSwingFactor"
                ? 0.0
                : animation::parameterValue(*control, control->minimumSliderValue);
            if (value < minimum || value > animation::parameterValue(*control, control->maximumSliderValue))
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Animation clip parameter is outside its declared range.");
        }
    }
    const auto& timing = animation::commonTimingDefinition();
    if (!std::isfinite(duration) || duration < timing.minimumDurationSeconds || duration > timing.maximumDurationSeconds
        || !std::isfinite(frameCount) || frameCount < 2 || frameCount > timing.maximumFrameCount
        || std::trunc(frameCount) != frameCount)
        throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Animation clip timing is outside its declared sampling range.");

    // The reference evaluator appends a closing sample only for loops.
    const bool loops = definition->second.loop;
    const auto retainedFrameCount = static_cast<size_t>(frameCount) + (loops ? 1 : 0);
    const auto jointCount = groundedRig.bones.size();
    if (!animation::animationFrameWorkFits(retainedFrameCount, jointCount))
        throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Animation clip exceeds the joint-frame limit.");
    const auto retainedBytes = clipType.size()
        + retainedFrameCount * (sizeof(double) + jointCount * sizeof(RigMatrix));
    if (retainedBytes > std::min(maximumPayloadBytes, size_t { 64 * 1024 * 1024 }))
        throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Animation clip exceeds its copied payload limit.");

    checkCancellation();
    RigAnimationClip clip;
    if (!AnimationGenerator::generate(groundedRig, inverseBindMatrices, clip, clipType, authoredParameters))
        throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "Reference animation clip evaluation rejected the rig or clip.");
    checkCancellation();
    if (clip.animationType != clipType || clip.loop != loops || clip.frames.size() != retainedFrameCount
        || clip.durationSeconds != static_cast<float>(duration) || clip.frames.back().time != clip.durationSeconds)
        throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "Reference animation clip returned unexpected clip timing.");
    auto owned = OwnedEvaluatedSkeletonClip::copy(clip, groundedRig, maximumPayloadBytes, cancellation);
    if (handoff) {
        checkCancellation();
        const auto remaining = maximumPayloadBytes - owned.logicalPayloadBytes();
        if (handoff(clip, remaining) > remaining)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Animation clip derived output exceeds its retained quota.");
        checkCancellation();
    }
    return owned;
}
}
