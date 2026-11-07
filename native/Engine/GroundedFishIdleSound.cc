// SPDX-License-Identifier: MIT
#include "GroundedFishIdleSound.h"
#include "Cancellation.h"
#include <dust3d/animation/sound_event_detector.h>
#include <algorithm>
#include <array>
#include <cmath>

namespace dust3d::native {
    AnimationSoundData renderFishIdleSound(const RigAnimationClip& clip, const AnimationParams& parameters,
        SurfaceMaterial material, size_t remainingBytes, const Dust3DCancellationToken* cancellation)
    {
        checkCancellation(cancellation);
        // Mirror the bounded audio branch's limits. A complete Fish track can emit
        // at most one underwater event per adjacent sample pair, including the seam.
        constexpr size_t maximumEvents = 128;
        constexpr int sampleRate = 44100;
        constexpr std::array<const char*, 5> trackedBones { "BodyFront", "BodyMid", "BodyRear", "TailStart", "TailEnd" };
        if (clip.animationType != "FishIdle" || !clip.loop || !std::isfinite(clip.durationSeconds)
            || clip.durationSeconds < 0.001f || clip.durationSeconds > 30.0f
            || clip.frames.size() < 2 || clip.frames.size() > maximumEvents + 1)
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "FishIdle sound is outside its bounded clip range.");
        const auto& surfaces = surfaceMaterials();
        if (std::find(surfaces.begin(), surfaces.end(), material) == surfaces.end())
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "FishIdle sound surface is invalid.");

        for (const auto& frame : clip.frames) {
            checkCancellation(cancellation);
            for (const auto* name : trackedBones) {
                const auto bone = frame.boneWorldTransforms.find(name);
                if (bone == frame.boneWorldTransforms.end())
                    throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "FishIdle sound requires complete body and tail tracks.");
                const auto* values = bone->second.constData();
                for (const auto axis : { Matrix4x4::M30, Matrix4x4::M31, Matrix4x4::M32 }) {
                    if (!std::isfinite(values[axis]) || std::abs(values[axis]) > 1e6)
                        throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "FishIdle sound track coordinates are invalid.");
                }
            }
        }
        const auto sampleCount = static_cast<size_t>(clip.durationSeconds * sampleRate);
        // Qt allocates a float mix and PCM concurrently. The bounded event list is
        // detector workspace, independent of the retained output charge.
        if (sampleCount > remainingBytes / (sizeof(float) + sizeof(int16_t)))
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "FishIdle PCM and mix exceed their staging quota.");

        checkCancellation(cancellation);
        const auto events = SoundEventDetector::detect(clip, clip.animationType, parameters);
        checkCancellation(cancellation);
        if (events.empty())
            return {};
        if (events.size() > maximumEvents
            || !std::all_of(events.begin(), events.end(), [&](const auto& event) {
                   return event.isUnderwater && !event.isWhoosh && std::isfinite(event.timeSeconds)
                       && event.timeSeconds >= 0 && event.timeSeconds <= clip.durationSeconds
                       && std::isfinite(event.intensity) && event.intensity > 0 && event.intensity <= 1;
               }))
            throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "FishIdle sound events are outside the bounded underwater contract.");
        auto sound = SoundGenerator::generate(events, clip.durationSeconds, material);
        checkCancellation(cancellation);
        if (sound.sampleRate != sampleRate || sound.channels != 1 || sound.durationSeconds != clip.durationSeconds
            || sound.pcmSamples.size() != sampleCount)
            throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "FishIdle PCM does not match its evaluated timeline.");
        return sound;
    }

    GroundedFishIdleSoundPreview generateGroundedFishIdleSoundPreview(const GeneratedRigInput& input,
        std::unique_ptr<Dust3DMesh> metadata, const RigStructure& templateRig, const AnimationParams& parameters,
        SurfaceMaterial material, const RigFinalDataOptions& options, size_t maximumDrawVertices,
        const Dust3DCancellationToken* cancellation)
    {
        AnimationSoundData sound;
        auto visual = generateGroundedFishIdleDrawPreview(input, std::move(metadata), templateRig, parameters,
            options, maximumDrawVertices, cancellation, [&](const RigAnimationClip& clip, size_t remainingBytes) {
                sound = renderFishIdleSound(clip, parameters, material, remainingBytes, cancellation);
                return sound.pcmSamples.size() * sizeof(int16_t);
            });
        checkCancellation(cancellation);
        return { std::move(visual), std::move(sound) };
    }
}
