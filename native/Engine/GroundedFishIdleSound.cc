// SPDX-License-Identifier: MIT
#include "GroundedFishIdleSound.h"
#include "Cancellation.h"
#include <dust3d/animation/sound_event_detector.h>
#include <algorithm>
#include <array>
#include <cmath>

namespace dust3d::native {
    static constexpr int fishIdleSampleRate = 44100;

    struct FishIdleSoundPlan {
        std::vector<SoundEvent> events;
        float duration;
        size_t sampleCount;
    };

    static FishIdleSoundPlan prepareFishIdleSound(const RigAnimationClip& clip, const AnimationParams& parameters,
        size_t remainingBytes, const Dust3DCancellationToken* cancellation)
    {
        checkCancellation(cancellation);
        // Mirror the bounded audio branch's limits. A complete Fish track can emit
        // at most one underwater event per adjacent sample pair, including the seam.
        constexpr size_t maximumEvents = 128;
        constexpr std::array<const char*, 5> trackedBones { "BodyMid", "BodyFront", "BodyRear", "TailStart", "TailEnd" };
        if (!supportsFishClip(clip.animationType) || clip.loop != supportsFishLoop(clip.animationType)
            || !std::isfinite(clip.durationSeconds)
            || clip.durationSeconds < 0.001f || clip.durationSeconds > 30.0f
            || clip.frames.size() < 2 || clip.frames.size() > maximumEvents + 1)
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Fish clip sound is outside its bounded clip range.");
        // Qt emits no FishAttack or FishHurt events. FishDie detects impact on BodyMid.
        if (clip.animationType == "FishAttack" || clip.animationType == "FishHurt")
            return { {}, clip.durationSeconds, 0 };
        const size_t trackCount = clip.loop ? trackedBones.size() : 1;
        for (const auto& frame : clip.frames) {
            checkCancellation(cancellation);
            for (size_t index = 0; index < trackCount; ++index) {
                const auto bone = frame.boneWorldTransforms.find(trackedBones[index]);
                if (bone == frame.boneWorldTransforms.end())
                    throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "Fish clip sound requires complete detector tracks.");
                const auto* values = bone->second.constData();
                for (const auto axis : { Matrix4x4::M30, Matrix4x4::M31, Matrix4x4::M32 }) {
                    if (!std::isfinite(values[axis]) || std::abs(values[axis]) > 1e6)
                        throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "Fish clip sound track coordinates are invalid.");
                }
            }
        }
        const auto sampleCount = static_cast<size_t>(clip.durationSeconds * fishIdleSampleRate);
        // Qt allocates a float mix and PCM concurrently. The bounded event list is
        // detector workspace, independent of the retained output charge.
        if (sampleCount > remainingBytes / (sizeof(float) + sizeof(int16_t)))
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "FishIdle PCM and mix exceed their staging quota.");

        checkCancellation(cancellation);
        auto events = SoundEventDetector::detect(clip, clip.animationType, parameters);
        checkCancellation(cancellation);
        if (events.size() > maximumEvents
            || !std::all_of(events.begin(), events.end(), [&](const auto& event) {
                   return event.isUnderwater && !event.isWhoosh && std::isfinite(event.timeSeconds)
                       && event.timeSeconds >= 0 && event.timeSeconds <= clip.durationSeconds
                       && std::isfinite(event.intensity) && event.intensity > 0 && event.intensity <= 1;
               }))
            throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "Fish clip sound events are outside the bounded underwater contract.");
        return { std::move(events), clip.durationSeconds, sampleCount };
    }

    static AnimationSoundData renderPreparedSound(const FishIdleSoundPlan& plan, SurfaceMaterial material,
        const Dust3DCancellationToken* cancellation)
    {
        checkCancellation(cancellation);
        if (plan.events.empty())
            return {};
        auto sound = SoundGenerator::generate(plan.events, plan.duration, material);
        checkCancellation(cancellation);
        if (sound.sampleRate != fishIdleSampleRate || sound.channels != 1 || sound.durationSeconds != plan.duration
            || sound.pcmSamples.size() != plan.sampleCount)
            throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "FishIdle PCM does not match its evaluated timeline.");
        return sound;
    }

    AnimationSoundData renderFishIdleSound(const RigAnimationClip& clip, const AnimationParams& parameters,
        SurfaceMaterial material, size_t remainingBytes, const Dust3DCancellationToken* cancellation)
    {
        checkCancellation(cancellation);
        const auto& surfaces = surfaceMaterials();
        if (std::find(surfaces.begin(), surfaces.end(), material) == surfaces.end())
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Fish clip sound surface is invalid.");
        return renderPreparedSound(prepareFishIdleSound(clip, parameters, remainingBytes, cancellation), material, cancellation);
    }

    std::vector<AnimationSoundData> renderFishIdleSoundSurfaces(const RigAnimationClip& clip,
        const AnimationParams& parameters, size_t remainingBytes, const Dust3DCancellationToken* cancellation)
    {
        const auto plan = prepareFishIdleSound(clip, parameters, remainingBytes, cancellation);
        const auto& surfaces = surfaceMaterials();
        if (!plan.events.empty() && plan.sampleCount > remainingBytes / (sizeof(float) + surfaces.size() * sizeof(int16_t)))
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "FishIdle surface PCM and mix exceed their staging quota.");

        std::vector<AnimationSoundData> sounds;
        sounds.reserve(surfaces.size());
        for (const auto material : surfaces)
            sounds.push_back(renderPreparedSound(plan, material, cancellation));
        return sounds;
    }

    GroundedFishIdleSoundPreview generateGroundedFishIdleSoundPreview(const GeneratedRigInput& input,
        std::unique_ptr<Dust3DMesh> metadata, const RigStructure& templateRig, const AnimationParams& parameters,
        SurfaceMaterial material, const RigFinalDataOptions& options, size_t maximumDrawVertices,
        const Dust3DCancellationToken* cancellation, const std::string& clipType)
    {
        AnimationSoundData sound;
        auto visual = generateGroundedFishIdleDrawPreview(input, std::move(metadata), templateRig, parameters,
            options, maximumDrawVertices, cancellation, [&](const RigAnimationClip& clip, size_t remainingBytes) {
                sound = renderFishIdleSound(clip, parameters, material, remainingBytes, cancellation);
                return sound.pcmSamples.size() * sizeof(int16_t);
            }, clipType);
        checkCancellation(cancellation);
        return { std::move(visual), std::move(sound) };
    }
}
