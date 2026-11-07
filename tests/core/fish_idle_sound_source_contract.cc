// SPDX-License-Identifier: MIT
// Unregistered fixture. Syntax-check only until the FishIdle evaluator is cleared to run.
#include "GroundedFishIdleSound.h"
#include <algorithm>
#include <cassert>

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
    const auto poses = evaluateFishIdle(rig, inverse, parameters, 1024 * 1024, nullptr,
        [&](const RigAnimationClip& clip, size_t remainingBytes) {
            sound = renderFishIdleSound(clip, parameters, SurfaceMaterial::Water, remainingBytes, nullptr);
            return sound.pcmSamples.size() * sizeof(int16_t);
        });
    assert(poses.loops() && poses.sampleTimes().size() == 33 && poses.sampleTimes().back() == 1);
    assert(sound.sampleRate == 44100 && sound.channels == 1 && sound.durationSeconds == poses.durationSeconds());
    assert(sound.pcmSamples.size() == 44100);
    assert(std::any_of(sound.pcmSamples.begin(), sound.pcmSamples.end(), [](int16_t value) { return value != 0; }));
}
