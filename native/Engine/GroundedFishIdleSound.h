// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_GROUNDED_FISH_IDLE_SOUND_H_
#define DUST3D_NATIVE_GROUNDED_FISH_IDLE_SOUND_H_

#include "GroundedFishIdlePreview.h"
#include <dust3d/animation/sound_generator.h>
#include <vector>

namespace dust3d::native {
    enum class GroundedClipSound { unsupported, silent, underwater, surface };

    inline constexpr GroundedClipSound groundedClipSound(std::string_view type)
    {
        const auto rig = evaluatedClipRig(type);
        if (rig.empty())
            return GroundedClipSound::unsupported;
        if (rig == "Spider")
            return type == "SpiderWalk" || type == "SpiderRun" || type == "SpiderDie"
                ? GroundedClipSound::surface
                : GroundedClipSound::silent;
        if (rig == "Insect")
            return type == "InsectWalk" || type == "InsectFly" || type == "InsectForward"
                    || type == "InsectAttack" || type == "InsectDie" || type == "InsectRubHands"
                ? GroundedClipSound::surface
                : GroundedClipSound::silent;
        if (type == "FishAttack" || type == "FishHurt" || type == "SnakeHurt" || type == "SnakeStrike")
            return GroundedClipSound::silent;
        return rig == "Fish" ? GroundedClipSound::underwater : GroundedClipSound::surface;
    }

    /// Stage reference mono PCM from the exact already-admitted animation clip.
    /// The quota includes the PCM and temporary float mix. A valid silent clip
    /// returns an empty sample vector. Cancellation polls around Qt routines.
    AnimationSoundData renderFishIdleSound(const RigAnimationClip&, const AnimationParams&,
        SurfaceMaterial, size_t remainingBytes, const Dust3DCancellationToken*);

    /// Detect once and retain PCM in exact surface catalog order. The quota
    /// includes all retained PCM and the one concurrently active float mix.
    std::vector<AnimationSoundData> renderFishIdleSoundSurfaces(const RigAnimationClip&, const AnimationParams&,
        size_t remainingBytes, const Dust3DCancellationToken*);

    struct GroundedFishIdleSoundPreview {
        GroundedFishIdleDrawPreview visual;
        AnimationSoundData sound;
    };

    /// Source-only owner. Source receipt, poses, draw weights and PCM come from
    /// one evaluation; no native playback or WAV writer is invoked.
    GroundedFishIdleSoundPreview generateGroundedFishIdleSoundPreview(const GeneratedRigInput&,
        std::unique_ptr<Dust3DMesh> metadata, const RigStructure&, const AnimationParams&,
        SurfaceMaterial, const RigFinalDataOptions&, size_t maximumDrawVertices, const Dust3DCancellationToken*,
        const std::string& clipType = "FishIdle");
}
#endif
