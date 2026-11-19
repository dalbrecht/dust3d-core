// SPDX-License-Identifier: MIT
#include "OwnedEvaluatedSkeletonClip.h"
#include "EngineInternal.h"
#include <algorithm>
#include <cmath>
#include <dust3d/animation/animation_generator.h>
#include <dust3d/animation/parameter_catalog.h>
#include <set>
#include <string_view>

namespace dust3d {
namespace native {
    OwnedEvaluatedSkeletonClip OwnedEvaluatedSkeletonClip::copy(const RigAnimationClip& clip, const RigStructure& rig,
        size_t maximumPayloadBytes, const std::atomic_bool* cancellation)
    {
        const auto checkCancellation = [cancellation] {
            if (cancellation && cancellation->load(std::memory_order_relaxed))
                throw DocumentError(DUST3D_STATUS_CANCELLED, "Evaluated skeleton clip copy cancelled.");
        };
        checkCancellation();
        const auto frameCount = clip.frames.size();
        const auto jointCount = rig.bones.size();
        if (frameCount < 2 || jointCount == 0 || clip.animationType.empty()
            || !std::isfinite(clip.durationSeconds) || clip.durationSeconds <= 0
            || !std::isfinite(clip.movementSpeed) || !std::isfinite(clip.movementDirectionX)
            || !std::isfinite(clip.movementDirectionZ)
            || clip.frames.front().time != 0
            || (clip.loop && clip.frames.back().time != clip.durationSeconds))
            throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "Invalid evaluated skeleton clip metadata.");
        if (!animation::animationFrameWorkFits(frameCount, jointCount))
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Evaluated skeleton clip exceeds the joint-frame limit.");

        size_t remaining = std::min(maximumPayloadBytes, size_t { 64 * 1024 * 1024 });
        const auto charge = [&remaining](size_t count, size_t stride) {
            if (count > remaining / stride)
                throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Evaluated skeleton clip exceeds its payload limit.");
            remaining -= count * stride;
        };
        charge(clip.name.size(), 1);
        charge(clip.animationType.size(), 1);
        charge(frameCount, sizeof(double));
        const auto matrixCount = frameCount * jointCount; // Bounded by animationFrameWorkFits.
        charge(matrixCount, sizeof(RigMatrix));

        std::set<std::string_view> jointNames;
        for (const auto& joint : rig.bones) {
            checkCancellation();
            if (joint.name.empty() || !jointNames.insert(joint.name).second)
                throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "Evaluated skeleton clip has invalid source joint names.");
        }

        OwnedEvaluatedSkeletonClip result;
        result.m_name = clip.name;
        result.m_animationType = clip.animationType;
        result.m_durationSeconds = clip.durationSeconds;
        result.m_loops = clip.loop;
        result.m_groundMotion = { clip.movementSpeed, clip.movementDirectionX, clip.movementDirectionZ };
        result.m_jointCount = jointCount;
        result.m_sampleTimes.reserve(frameCount);
        result.m_modelFromJoint.reserve(matrixCount);
        double previous = -1;
        for (const auto& frame : clip.frames) {
            checkCancellation();
            if (!animation::animationFrameTimeFollows(frame.time, previous, clip.durationSeconds)
                || frame.boneWorldTransforms.size() != jointCount)
                throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "Evaluated skeleton clip has invalid frame timing or joint coverage.");
            for (const auto& joint : rig.bones) {
                checkCancellation();
                const auto matrix = frame.boneWorldTransforms.find(joint.name);
                if (matrix == frame.boneWorldTransforms.end())
                    throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "Evaluated skeleton frame is missing a source joint.");
                const auto* values = matrix->second.constData();
                if (!std::all_of(values, values + 16, [](double value) { return std::isfinite(value); }))
                    throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "Evaluated skeleton frame has a nonfinite matrix.");
                RigMatrix copy;
                std::copy_n(values, copy.size(), copy.begin());
                result.m_modelFromJoint.push_back(copy);
            }
            result.m_sampleTimes.push_back(frame.time);
            previous = frame.time;
        }
        const auto& matrices = result.m_modelFromJoint;
        if (clip.loop && !std::equal(matrices.begin(), matrices.begin() + jointCount, matrices.end() - jointCount))
            throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "Evaluated skeleton loop must close at its first pose.");
        checkCancellation();
        return result;
    }
}
}
