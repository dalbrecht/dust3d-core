// SPDX-License-Identifier: MIT
// Unregistered fixture. Syntax-check only until the FishIdle evaluator is cleared to run.
#include "GroundedFishIdleSound.h"
#include <algorithm>
#include <cassert>

using dust3d::native::GroundedClipSound;
using dust3d::native::groundedClipSound;
static_assert(groundedClipSound("FishSwim") == GroundedClipSound::underwater);
static_assert(groundedClipSound("SnakeSlither") == GroundedClipSound::surface);
static_assert(groundedClipSound("SnakeForward") == GroundedClipSound::surface);
static_assert(groundedClipSound("SnakeIdle") == GroundedClipSound::surface);
static_assert(groundedClipSound("SnakeDie") == GroundedClipSound::surface);
static_assert(groundedClipSound("SnakeHurt") == GroundedClipSound::silent);
static_assert(groundedClipSound("SnakeStrike") == GroundedClipSound::silent);
static_assert(groundedClipSound("SnakeWalk") == GroundedClipSound::unsupported);
static_assert(dust3d::native::evaluatedClipRig("SpiderRunBackwardRight") == "Spider");
static_assert(dust3d::native::evaluatedClipRig("SpiderWalkForward").empty());
static_assert(groundedClipSound("SpiderWalk") == GroundedClipSound::surface);
static_assert(groundedClipSound("SpiderDie") == GroundedClipSound::surface);
static_assert(groundedClipSound("SpiderRunBackwardRight") == GroundedClipSound::silent);
static_assert(dust3d::native::evaluatedClipRig("InsectWalkForwardLeft") == "Insect");
static_assert(dust3d::native::evaluatedClipRig("InsectForward") == "Insect");
static_assert(dust3d::native::evaluatedClipRig("InsectFlyLeft").empty());
static_assert(groundedClipSound("InsectFly") == GroundedClipSound::surface);
static_assert(groundedClipSound("InsectRubHands") == GroundedClipSound::surface);
static_assert(groundedClipSound("InsectWalkForwardLeft") == GroundedClipSound::silent);
static_assert(dust3d::native::evaluatedClipRig("BirdRunBackwardLeft") == "Bird");
static_assert(dust3d::native::evaluatedClipRig("BirdForward") == "Bird");
static_assert(dust3d::native::evaluatedClipRig("BirdGlideLeft").empty());
static_assert(groundedClipSound("BirdForward") == GroundedClipSound::surface);
static_assert(groundedClipSound("BirdEat") == GroundedClipSound::surface);
static_assert(groundedClipSound("BirdRunBackwardLeft") == GroundedClipSound::silent);
static_assert(dust3d::native::evaluatedClipRig("QuadrupedRunBackwardLeft") == "Quadruped");
static_assert(dust3d::native::evaluatedClipRig("QuadrupedEatLeft").empty());
static_assert(groundedClipSound("QuadrupedRoar") == GroundedClipSound::surface);
static_assert(groundedClipSound("QuadrupedRunBackwardLeft") == GroundedClipSound::silent);
static_assert(dust3d::native::evaluatedClipRig("BipedCrouchEnter") == "Biped");
static_assert(dust3d::native::evaluatedClipRig("BipedWalkForwardRight") == "Biped");
static_assert(dust3d::native::evaluatedClipRig("BipedWalkRight").empty());
static_assert(groundedClipSound("BipedSprint") == GroundedClipSound::surface);
static_assert(groundedClipSound("BipedCast") == GroundedClipSound::surface);
static_assert(groundedClipSound("BipedCrouchEnter") == GroundedClipSound::silent);

int main()
{
    using namespace dust3d;
    using namespace dust3d::native;

    // The seven required names from application/resources/rig_fish.xml, in a
    // grounded 1.5-unit chain. The preview integration supplies this same shape
    // through actual authored edge assignments and the owned grounded mesh.
    const RigStructure rig { "Fish", "", {
                                             { "Root", "", 0, 0, 0.75f, 0, 0.25f, 0.75f },
                                             { "Head", "Root", 0, 0, 0.75f, 0, 0, 0.50f },
                                             { "BodyFront", "Head", 0, 0, 0.50f, 0, 0, 0.25f },
                                             { "BodyMid", "BodyFront", 0, 0, 0.25f, 0, 0, 0 },
                                             { "BodyRear", "BodyMid", 0, 0, 0, 0, 0, -0.25f },
                                             { "TailStart", "BodyRear", 0, 0, -0.25f, 0, 0, -0.50f },
                                             { "TailEnd", "TailStart", 0, 0, -0.50f, 0, 0, -0.75f },
                                         } };
    RigGenerator generator;
    std::map<std::string, Matrix4x4> inverse;
    const auto prepared = generator.computeBoneInverseBindMatrices(rig, inverse);
    assert(prepared);

    const AnimationParams parameters { { { "durationSeconds", "1" }, { "frameCount", "32" }, { "driftFactor", "3" } } };
    AnimationSoundData sound;
    std::vector<AnimationSoundData> surfaces;
    const auto poses = evaluateFishIdle(rig, inverse, parameters, 4 * 1024 * 1024, nullptr,
        [&](const RigAnimationClip& clip, size_t remainingBytes) {
            sound = renderFishIdleSound(clip, parameters, SurfaceMaterial::Water, remainingBytes, nullptr);
            size_t retainedBytes = sound.pcmSamples.size() * sizeof(int16_t);
            surfaces = renderFishIdleSoundSurfaces(clip, parameters, remainingBytes - retainedBytes, nullptr);
            for (const auto& surface : surfaces)
                retainedBytes += surface.pcmSamples.size() * sizeof(int16_t);
            return retainedBytes;
        });
    assert(poses.loops() && poses.sampleTimes().size() == 33 && poses.sampleTimes().back() == 1);
    assert(sound.sampleRate == 44100 && sound.channels == 1 && sound.durationSeconds == poses.durationSeconds());
    assert(sound.pcmSamples.size() == 44100);
    assert(std::any_of(sound.pcmSamples.begin(), sound.pcmSamples.end(), [](int16_t value) { return value != 0; }));
    const auto& catalog = surfaceMaterials();
    assert(surfaces.size() == catalog.size());
    for (size_t index = 0; index < catalog.size(); ++index) {
        assert(surfaces[index].sampleRate == 44100 && surfaces[index].channels == 1);
        assert(surfaces[index].pcmSamples.size() == sound.pcmSamples.size());
        if (catalog[index] == SurfaceMaterial::Water)
            assert(surfaces[index].pcmSamples == sound.pcmSamples);
    }
}
