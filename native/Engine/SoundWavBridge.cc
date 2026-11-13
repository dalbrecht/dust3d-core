// SPDX-License-Identifier: MIT
#include "Dust3DAudioWav.h"
#include "EngineSupport.h"
#include <dust3d/animation/sound_generator.h>
#include <memory>
#include <stdexcept>

struct Dust3DSoundWav {
    std::unique_ptr<Dust3DAudioWav, decltype(&dust3d_audio_wav_destroy)> wav { nullptr, dust3d_audio_wav_destroy };
};

Dust3DSoundWav* dust3d_sound_wav_create(const Dust3DSoundEvent* events, size_t count,
    float durationSeconds, size_t surfaceIndex, Dust3DError* error)
{
    using dust3d::native::setError;
    setError(error, DUST3D_STATUS_OK, "");
    try {
        if (count > dust3d::SoundGenerator::maximumBoundedEvents) {
            setError(error, DUST3D_STATUS_LIMIT_EXCEEDED, "Sound synthesis accepts at most 128 events.");
            return nullptr;
        }
        const auto& surfaces = dust3d::surfaceMaterials();
        if ((count != 0 && !events) || surfaceIndex >= surfaces.size())
            throw std::invalid_argument("Invalid sound event buffer or surface index");
        std::vector<dust3d::SoundEvent> input;
        input.reserve(count);
        for (size_t index = 0; index < count; ++index) {
            const auto& event = events[index];
            if (event.kind > DUST3D_SOUND_WHOOSH)
                throw std::invalid_argument("Unknown sound event kind");
            input.push_back({ event.time_seconds, event.intensity, {}, event.kind == DUST3D_SOUND_UNDERWATER,
                event.kind == DUST3D_SOUND_WHOOSH, event.whoosh_duration_seconds });
        }
        dust3d::AnimationSoundData pcm;
        {
            std::lock_guard<std::mutex> lock(dust3d::native::engineMutex());
            pcm = dust3d::SoundGenerator::generateBoundedPcm(input, durationSeconds, surfaces[surfaceIndex]);
        }
        auto owner = std::make_unique<Dust3DSoundWav>();
        constexpr uint64_t maximumWavBytes = 44 + dust3d::SoundGenerator::maximumBoundedDurationSeconds * 44100 * 2;
        Dust3DAudioWavStatus status {};
        owner->wav.reset(dust3d_audio_wav_create(pcm.pcmSamples.data(), pcm.pcmSamples.size(),
            pcm.sampleRate, pcm.channels, maximumWavBytes, &status));
        if (!owner->wav) {
            if (status == DUST3D_AUDIO_WAV_ALLOCATION_FAILED || status == DUST3D_AUDIO_WAV_QUOTA)
                setError(error, DUST3D_STATUS_LIMIT_EXCEEDED, "Sound WAV exceeds the available output budget.");
            else
                setError(error, DUST3D_STATUS_GENERATION_FAILED, "The synthesized PCM could not be encoded.");
            return nullptr;
        }
        return owner.release();
    } catch (const std::invalid_argument& cause) {
        setError(error, DUST3D_STATUS_INVALID_DOCUMENT, cause.what());
    } catch (const std::bad_alloc&) {
        setError(error, DUST3D_STATUS_LIMIT_EXCEEDED, "Not enough memory to generate sound.");
    } catch (const std::exception& cause) {
        setError(error, DUST3D_STATUS_GENERATION_FAILED, cause.what());
    } catch (...) {
        setError(error, DUST3D_STATUS_GENERATION_FAILED, "An unknown error occurred during sound generation.");
    }
    return nullptr;
}

void dust3d_sound_wav_destroy(Dust3DSoundWav* wav) { delete wav; }
const uint8_t* dust3d_sound_wav_bytes(const Dust3DSoundWav* wav) { return wav ? dust3d_audio_wav_bytes(wav->wav.get()) : nullptr; }
size_t dust3d_sound_wav_size(const Dust3DSoundWav* wav) { return wav ? static_cast<size_t>(dust3d_audio_wav_size(wav->wav.get())) : 0; }
