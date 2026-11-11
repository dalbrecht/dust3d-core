// SPDX-License-Identifier: MIT
#include "GroundedFishIdleSound.h"
#include "Cancellation.h"
#include <dust3d/animation/animation_catalog.h>
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
        // Keep the existing retained event and PCM work budgets.
        constexpr size_t maximumEvents = 128;
        constexpr std::array fishBones { "BodyMid", "BodyFront", "BodyRear", "TailStart", "TailEnd" };
        constexpr std::array snakeBones { "Spine3", "Spine1", "Spine2", "Spine4", "Spine5", "Spine6",
            "Tail1", "Tail2", "Tail3", "Tail4" };
        const auto sound = groundedClipSound(clip.animationType);
        const auto& catalog = animation::clipCatalog();
        const auto definition = catalog.find(clip.animationType);
        if (sound == GroundedClipSound::unsupported || definition == catalog.end()
            || clip.loop != definition->second.loop || !std::isfinite(clip.durationSeconds)
            || clip.durationSeconds < 0.001f || clip.durationSeconds > 30.0f
            || clip.frames.size() < 2 || clip.frames.size() > maximumEvents + 1)
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Animation sound is outside its bounded clip range.");
        if (sound == GroundedClipSound::silent)
            return { {}, clip.durationSeconds, 0 };
        const bool underwater = sound == GroundedClipSound::underwater;
        const bool usesNamedTracks = definition->second.rig == "Fish" || definition->second.rig == "Snake";
        const auto tracks = underwater ? fishBones.data() : snakeBones.data();
        // Impact detection uses only BodyMid or Spine3, the first bone in each list.
        const size_t trackCount = clip.loop ? (underwater ? fishBones.size() : snakeBones.size()) : 1;
        const auto validateCoordinates = [](const Matrix4x4& transform) {
            const auto* values = transform.constData();
            for (const auto axis : { Matrix4x4::M30, Matrix4x4::M31, Matrix4x4::M32 }) {
                if (!std::isfinite(values[axis]) || std::abs(values[axis]) > 1e6)
                    throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "Animation sound track coordinates are invalid.");
            }
        };
        for (const auto& frame : clip.frames) {
            checkCancellation(cancellation);
            // Surface detectors can discover leg tips by name and try root aliases on impact.
            if (!usesNamedTracks) {
                if (frame.boneWorldTransforms.empty() || frame.boneWorldTransforms.size() > 256)
                    throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Animation sound exceeds its detector track limits.");
                for (const auto& track : frame.boneWorldTransforms)
                    validateCoordinates(track.second);
                continue;
            }
            for (size_t index = 0; index < trackCount; ++index) {
                const auto bone = frame.boneWorldTransforms.find(tracks[index]);
                if (bone == frame.boneWorldTransforms.end())
                    throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "Animation sound requires complete detector tracks.");
                validateCoordinates(bone->second);
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
        // The reference detector computes its final event time in float as
        // frameIndex * (duration / intervals), which can round one ULP past duration.
        const auto lastEventTime = std::nextafter(clip.durationSeconds, INFINITY);
        const bool allowsWhoosh = clip.animationType == "QuadrupedAttack" || clip.animationType == "QuadrupedRoar";
        // The Roar detector retains muted impact events when the Stomp control is zero.
        const bool allowsMutedImpact = clip.animationType == "QuadrupedRoar";
        if (events.size() > maximumEvents
            || !std::all_of(events.begin(), events.end(), [&](const auto& event) {
                   const bool validWhoosh = !event.isWhoosh || (allowsWhoosh && std::isfinite(event.whooshDuration)
                       && event.whooshDuration > 0 && event.whooshDuration <= lastEventTime);
                   const bool validIntensity = std::isfinite(event.intensity) && event.intensity <= 1
                       && (event.intensity > 0 || (allowsMutedImpact && !event.isWhoosh && event.intensity == 0));
                   return event.isUnderwater == underwater && validWhoosh && validIntensity && std::isfinite(event.timeSeconds)
                       && event.timeSeconds >= 0 && event.timeSeconds <= lastEventTime;
               }))
            throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "Animation sound events are outside the bounded detector contract.");
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
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Animation sound surface is invalid.");
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
        const Dust3DCancellationToken* cancellation, const std::string& clipType, const GeneratedRigDrawBakeInput* bake)
    {
        AnimationSoundData sound;
        auto visual = generateGroundedFishIdleDrawPreview(input, std::move(metadata), templateRig, parameters,
            options, maximumDrawVertices, cancellation, [&](const RigAnimationClip& clip, size_t remainingBytes) {
                sound = renderFishIdleSound(clip, parameters, material, remainingBytes, cancellation);
                return sound.pcmSamples.size() * sizeof(int16_t);
            }, clipType, bake);
        checkCancellation(cancellation);
        return { std::move(visual), std::move(sound) };
    }
}
