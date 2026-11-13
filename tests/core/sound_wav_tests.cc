// SPDX-License-Identifier: MIT
#include "Dust3DAudioWav.h"
#include <algorithm>
#include <dust3d/animation/sound_generator.h>
#include <limits>
#include <memory>
#include <stdexcept>

int main()
{
    using namespace dust3d;
    const std::vector<SoundEvent> events { { 0.1f, 0.75f, "SyntheticFoot" } };
    const auto expected = SoundGenerator::generate(events, 0.5f, SurfaceMaterial::Wood);
    const auto bounded = SoundGenerator::generateBoundedPcm(events, 0.5f, SurfaceMaterial::Wood);
    if (bounded.pcmSamples.size() != 22050 || bounded.pcmSamples != expected.pcmSamples
        || bounded.sampleRate != 44100 || bounded.channels != 1)
        return 1;

    const auto referenceWav = SoundGenerator::encodeWav(expected);
    Dust3DAudioWavStatus status {};
    const std::unique_ptr<Dust3DAudioWav, decltype(&dust3d_audio_wav_destroy)> wav(
        dust3d_audio_wav_create(bounded.pcmSamples.data(), bounded.pcmSamples.size(), bounded.sampleRate,
            bounded.channels, referenceWav.size(), &status),
        dust3d_audio_wav_destroy);
    if (!wav || status != DUST3D_AUDIO_WAV_OK || dust3d_audio_wav_size(wav.get()) != referenceWav.size()
        || !std::equal(referenceWav.begin(), referenceWav.end(), dust3d_audio_wav_bytes(wav.get())))
        return 1;

    const auto rejected = [](const std::vector<SoundEvent>& input, float duration, SurfaceMaterial material) {
        try {
            SoundGenerator::generateBoundedPcm(input, duration, material);
        } catch (const std::invalid_argument&) {
            return true;
        }
        return false;
    };
    auto badEvent = events;
    badEvent.front().timeSeconds = std::numeric_limits<float>::quiet_NaN();
    if (!rejected(badEvent, 0.5f, SurfaceMaterial::Wood)
        || !rejected(events, -0.5f, SurfaceMaterial::Wood)
        || !rejected(events, 31.0f, SurfaceMaterial::Wood)
        || !rejected(events, 0.5f, static_cast<SurfaceMaterial>(99))
        || !rejected(std::vector<SoundEvent>(129, events.front()), 0.5f, SurfaceMaterial::Wood))
        return 1;
    badEvent = events;
    badEvent.front().isWhoosh = true;
    badEvent.front().whooshDuration = 3.0f;
    return rejected(badEvent, 0.5f, SurfaceMaterial::Wood) ? 0 : 1;
}
