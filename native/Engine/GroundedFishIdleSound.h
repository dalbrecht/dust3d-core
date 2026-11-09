// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_GROUNDED_FISH_IDLE_SOUND_H_
#define DUST3D_NATIVE_GROUNDED_FISH_IDLE_SOUND_H_

#include "GroundedFishIdlePreview.h"
#include <dust3d/animation/sound_generator.h>
#include <vector>

namespace dust3d::native {
    /// Stage reference mono PCM from the exact already-admitted FishIdle clip.
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
