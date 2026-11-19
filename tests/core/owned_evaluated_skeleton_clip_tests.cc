// SPDX-License-Identifier: MIT
// Synthetic copied-value contract. This source does not invoke an evaluator.
#include "native/Engine/EngineInternal.h"
#include "native/Engine/OwnedEvaluatedSkeletonClip.h"
#include <dust3d/animation/animation_generator.h>
#include <limits>
#include <stdexcept>

namespace {
using namespace dust3d;
using namespace dust3d::native;

void require(bool condition)
{
    if (!condition)
        throw std::runtime_error("Owned evaluated skeleton clip contract failed.");
}

template <typename Operation>
void rejects(Dust3DStatus status, Operation operation)
{
    try {
        operation();
    } catch (const DocumentError& error) {
        require(error.status() == status);
        return;
    }
    throw std::runtime_error("Invalid evaluated skeleton clip was accepted.");
}

RigAnimationClip fixture()
{
    RigAnimationClip clip;
    clip.name = "Copied walk";
    clip.animationType = "BipedWalk";
    clip.durationSeconds = 1.0f;
    clip.loop = true;
    clip.movementSpeed = 0.5f;
    clip.movementDirectionX = -0.25f;
    clip.movementDirectionZ = 0.75f;
    BoneAnimationFrame first;
    first.boneWorldTransforms["z-root"].data()[12] = 7.0;
    first.boneWorldTransforms["a-child"].data()[12] = 3.0;
    auto middle = first;
    middle.time = 0.5f;
    middle.boneWorldTransforms.at("a-child").data()[12] = 5.0;
    auto seam = first;
    seam.time = 1.0f;
    clip.frames = { first, middle, seam };
    return clip;
}
}

int main()
{
    RigStructure rig;
    rig.bones.push_back({ "z-root", "" });
    rig.bones.push_back({ "a-child", "z-root" });
    auto source = fixture();
    const auto owned = OwnedEvaluatedSkeletonClip::copy(source, rig);
    source.frames.clear();
    source.name.clear();
    source.movementSpeed = 0;
    source.movementDirectionX = 0;
    source.movementDirectionZ = 0;
    const auto motion = owned.groundMotion();
    require(motion.speed == 0.5f && motion.directionX == -0.25f && motion.directionZ == 0.75f);
    require(owned.name() == "Copied walk" && owned.animationType() == "BipedWalk");
    require(owned.loops() && owned.durationSeconds() == 1 && owned.jointCount() == 2);
    require(owned.sampleTimes() == std::vector<double> { 0, 0.5, 1 });
    const auto& matrices = owned.modelFromJoint();
    require(matrices.size() == 6 && matrices[0][12] == 7 && matrices[1][12] == 3 && matrices[3][12] == 5);

    auto invalid = fixture();
    invalid.movementDirectionZ = std::numeric_limits<float>::infinity();
    rejects(DUST3D_STATUS_GENERATION_FAILED, [&] { OwnedEvaluatedSkeletonClip::copy(invalid, rig); });
    invalid = fixture();
    invalid.frames.back().time = 0.75f;
    rejects(DUST3D_STATUS_GENERATION_FAILED, [&] { OwnedEvaluatedSkeletonClip::copy(invalid, rig); });
    invalid = fixture();
    invalid.frames.back().boneWorldTransforms.at("a-child").data()[12] = 4.0;
    rejects(DUST3D_STATUS_GENERATION_FAILED, [&] { OwnedEvaluatedSkeletonClip::copy(invalid, rig); });
    invalid.loop = false;
    require(OwnedEvaluatedSkeletonClip::copy(invalid, rig).modelFromJoint().back()[12] == 4.0);
    invalid = fixture();
    invalid.frames[1].boneWorldTransforms.erase("a-child");
    invalid.frames[1].boneWorldTransforms.emplace("other", Matrix4x4 {});
    rejects(DUST3D_STATUS_GENERATION_FAILED, [&] { OwnedEvaluatedSkeletonClip::copy(invalid, rig); });
    invalid = fixture();
    invalid.frames[1].boneWorldTransforms.at("a-child").data()[12] = std::numeric_limits<double>::infinity();
    rejects(DUST3D_STATUS_GENERATION_FAILED, [&] { OwnedEvaluatedSkeletonClip::copy(invalid, rig); });
    rejects(DUST3D_STATUS_LIMIT_EXCEEDED, [&] { OwnedEvaluatedSkeletonClip::copy(fixture(), rig, 1); });
    const std::atomic_bool cancelled { true };
    rejects(DUST3D_STATUS_CANCELLED, [&] { OwnedEvaluatedSkeletonClip::copy(fixture(), rig, 4096, &cancelled); });
}
