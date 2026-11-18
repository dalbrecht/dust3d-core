// SPDX-License-Identifier: MIT
#include "include/Dust3DAudioWav.h"
#include <limits>
#include <memory>
#include <new>
#include <vector>

struct Dust3DAudioWav {
    std::vector<uint8_t> bytes;
};

Dust3DAudioWavStatus dust3d_audio_wav_plan(uint64_t sampleCount, uint32_t sampleRate,
    uint32_t channels, uint64_t outputQuota, uint64_t* outputSize)
{
    if (outputSize)
        *outputSize = 0;
    if (!channels || channels > std::numeric_limits<uint16_t>::max() / 2)
        return DUST3D_AUDIO_WAV_INVALID_CHANNELS;
    const uint32_t blockAlign = channels * 2;
    if (!sampleRate || sampleRate > std::numeric_limits<uint32_t>::max() / blockAlign)
        return DUST3D_AUDIO_WAV_INVALID_RATE;
    if (sampleCount % channels)
        return DUST3D_AUDIO_WAV_INCOMPLETE_FRAME;
    if (sampleCount > (uint64_t(std::numeric_limits<uint32_t>::max()) - 36) / 2)
        return DUST3D_AUDIO_WAV_RIFF_LIMIT;
    const uint64_t total = 44 + sampleCount * 2;
    if (total > std::numeric_limits<size_t>::max())
        return DUST3D_AUDIO_WAV_PLATFORM_LIMIT;
    if (total > outputQuota)
        return DUST3D_AUDIO_WAV_QUOTA;
    if (outputSize)
        *outputSize = total;
    return DUST3D_AUDIO_WAV_OK;
}

Dust3DAudioWav* dust3d_audio_wav_create(const int16_t* samples, uint64_t sampleCount,
    uint32_t sampleRate, uint32_t channels, uint64_t outputQuota, Dust3DAudioWavStatus* status)
{
    uint64_t total = 0;
    auto result = dust3d_audio_wav_plan(sampleCount, sampleRate, channels, outputQuota, &total);
    if (result == DUST3D_AUDIO_WAV_OK && sampleCount && !samples)
        result = DUST3D_AUDIO_WAV_NULL_SAMPLES;
    if (status)
        *status = result;
    if (result != DUST3D_AUDIO_WAV_OK)
        return nullptr;
    try {
        auto owner = std::make_unique<Dust3DAudioWav>();
        owner->bytes.reserve(static_cast<size_t>(total));
        auto& bytes = owner->bytes;
        auto u16 = [&](uint16_t value) {
            bytes.push_back(static_cast<uint8_t>(value));
            bytes.push_back(static_cast<uint8_t>(value >> 8));
        };
        auto u32 = [&](uint32_t value) {
            u16(static_cast<uint16_t>(value));
            u16(static_cast<uint16_t>(value >> 16));
        };
        auto tag = [&](const char* text) {
            for (size_t index = 0; index < 4; ++index)
                bytes.push_back(static_cast<uint8_t>(text[index]));
        };
        tag("RIFF");
        u32(static_cast<uint32_t>(total - 8));
        tag("WAVE");
        tag("fmt ");
        u32(16);
        u16(1);
        u16(static_cast<uint16_t>(channels));
        u32(sampleRate);
        u32(sampleRate * channels * 2);
        u16(static_cast<uint16_t>(channels * 2));
        u16(16);
        tag("data");
        u32(static_cast<uint32_t>(sampleCount * 2));
        // Conversion to unsigned preserves negative PCM bit patterns without signed shifts.
        for (size_t index = 0; index < static_cast<size_t>(sampleCount); ++index)
            u16(static_cast<uint16_t>(samples[index]));
        return owner.release();
    } catch (const std::bad_alloc&) {
        if (status)
            *status = DUST3D_AUDIO_WAV_ALLOCATION_FAILED;
    } catch (...) {
        if (status)
            *status = DUST3D_AUDIO_WAV_ENCODING_FAILED;
    }
    return nullptr;
}

const uint8_t* dust3d_audio_wav_bytes(const Dust3DAudioWav* owner)
{
    return owner ? owner->bytes.data() : nullptr;
}
uint64_t dust3d_audio_wav_size(const Dust3DAudioWav* owner)
{
    return owner ? owner->bytes.size() : 0;
}
void dust3d_audio_wav_destroy(Dust3DAudioWav* owner)
{
    delete owner;
}
