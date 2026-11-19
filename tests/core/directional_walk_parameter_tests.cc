// SPDX-License-Identifier: MIT
#include "EngineInternal.h"
#include "FishIdleEvaluation.h"
#include <cmath>
#include <dust3d/animation/animation_catalog.h>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

dust3d::RigStructure walkingRig()
{
    // Required gait bones and rest coordinates copied from the repository's
    // MIT-licensed application/resources/rig_biped.xml. No generated mesh/native owner needed.
    return { "Biped", "", {
                              { "Root", "", 0, 0, 0, 0, 0.9f, 0 },
                              { "Hips", "Root", 0, 0.9f, 0, 0, 1.0f, 0 },
                              { "Spine", "Hips", 0, 1.0f, 0, 0, 1.2f, 0 },
                              { "Chest", "Spine", 0, 1.2f, 0, 0, 1.4f, 0 },
                              { "Neck", "Chest", 0, 1.4f, 0, 0, 1.6f, 0 },
                              { "Head", "Neck", 0, 1.6f, 0, 0, 1.85f, 0 },
                              { "LeftShoulder", "Chest", 0.2f, 1.4f, 0, 0.35f, 1.35f, 0 },
                              { "LeftUpperArm", "LeftShoulder", 0.35f, 1.35f, 0, 0.6f, 1.15f, 0 },
                              { "LeftLowerArm", "LeftUpperArm", 0.6f, 1.15f, 0, 0.8f, 0.95f, 0 },
                              { "LeftHand", "LeftLowerArm", 0.8f, 0.95f, 0, 0.95f, 0.9f, 0 },
                              { "RightShoulder", "Chest", -0.2f, 1.4f, 0, -0.35f, 1.35f, 0 },
                              { "RightUpperArm", "RightShoulder", -0.35f, 1.35f, 0, -0.6f, 1.15f, 0 },
                              { "RightLowerArm", "RightUpperArm", -0.6f, 1.15f, 0, -0.8f, 0.95f, 0 },
                              { "RightHand", "RightLowerArm", -0.8f, 0.95f, 0, -0.95f, 0.9f, 0 },
                              { "LeftUpperLeg", "Hips", 0.1f, 0.9f, 0, 0.1f, 0.5f, 0 },
                              { "LeftLowerLeg", "LeftUpperLeg", 0.1f, 0.5f, 0, 0.1f, 0.1f, 0 },
                              { "LeftFoot", "LeftLowerLeg", 0.1f, 0.1f, 0, 0.1f, 0.0f, 0.2f },
                              { "RightUpperLeg", "Hips", -0.1f, 0.9f, 0, -0.1f, 0.5f, 0 },
                              { "RightLowerLeg", "RightUpperLeg", -0.1f, 0.5f, 0, -0.1f, 0.1f, 0 },
                              { "RightFoot", "RightLowerLeg", -0.1f, 0.1f, 0, -0.1f, 0.0f, 0.2f },
                          } };
}

dust3d::AnimationParams parameters(const std::string& armSwing)
{
    return { { { "durationSeconds", "0.4" }, { "frameCount", "4" }, { "armSwingFactor", armSwing } } };
}

void rejects(const dust3d::RigStructure& rig, const std::map<std::string, dust3d::Matrix4x4>& inverse,
    const std::string& type, const std::string& armSwing)
{
    try {
        (void)dust3d::native::evaluateFishIdle(rig, inverse, parameters(armSwing), 1024 * 1024, nullptr, {}, type);
    } catch (const dust3d::native::DocumentError& error) {
        require(error.status() == DUST3D_STATUS_INVALID_DOCUMENT, type + " rejected with an unexpected status");
        return;
    }
    throw std::runtime_error(type + " unexpectedly admitted armSwingFactor=" + armSwing);
}
}

int main()
{
    try {
        using namespace dust3d;
        using namespace dust3d::native;
        const auto rig = walkingRig();
        std::map<std::string, Matrix4x4> inverse;
        require(RigGenerator().computeBoneInverseBindMatrices(rig, inverse), "Reference inverse binds failed");
        size_t walkingTypes = 0;
        for (const auto& [type, definition] : animation::clipCatalog()) {
            if (definition.rig != "Biped" || definition.generator != "BipedWalk")
                continue;
            ++walkingTypes;
            for (const std::string value : { "0.2", "0", "0.25", "2" }) {
                const auto authored = parameters(value);
                RigAnimationClip reference;
                require(AnimationGenerator::generate(rig, inverse, reference, type, authored),
                    type + " reference rejected armSwingFactor=" + value);
                const auto expected = OwnedEvaluatedSkeletonClip::copy(reference, rig, 1024 * 1024);
                const auto actual = [&] {
                    try {
                        return evaluateFishIdle(rig, inverse, authored, 1024 * 1024, nullptr, {}, type);
                    } catch (const DocumentError& error) {
                        throw std::runtime_error(type + " native rejected reference-supported armSwingFactor=" + value + ": " + error.what());
                    }
                }();
                require(actual.animationType() == type && actual.loops(), type + " lost requested clip identity/loop");
                require(actual.sampleTimes() == expected.sampleTimes(), type + " differs from reference timing");
                require(actual.modelFromJoint() == expected.modelFromJoint(),
                    type + " differs from reference joint matrices for armSwingFactor=" + value);
                require(actual.sampleTimes().size() == 5 && actual.sampleTimes().back() == static_cast<float>(0.4),
                    type + " lost its authored samples/closing pose");
                for (const auto& matrix : actual.modelFromJoint()) {
                    for (double element : matrix)
                        require(std::isfinite(element), type + " returned a nonfinite matrix");
                }
            }
            for (const std::string value : { "-0.01", "2.01", "nan", "inf", "1e999" })
                rejects(rig, inverse, type, value);
            auto mismatched = rig;
            mismatched.type = "Fish";
            rejects(mismatched, inverse, type, "0.2");
        }
        require(walkingTypes == 5, "Expected the base walk and all four diagonal derivatives");
        // Separate gait generators retain their existing slider bounds.
        for (const std::string type : { "BipedRun", "BipedRunForwardLeft", "BipedWalkBackward", "BipedStrafeLeft" })
            rejects(rig, inverse, type, "0.2");
        std::cout << "Directional walk authored parameters match the reference for all five clip types\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
