// SPDX-License-Identifier: MIT
#include "Dust3DAudioWav.h"
// Contract calls inside assertions must execute in Release builds too.
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdint>
#include <cstring>
#include <limits>

static uint32_t u32(const uint8_t* p)
{
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
int main()
{
    uint64_t size = 99;
    assert(dust3d_audio_wav_plan(5, 44100, 1, 54, &size) == DUST3D_AUDIO_WAV_OK && size == 54);
    assert(dust3d_audio_wav_plan(0, 44100, 1, 44, &size) == DUST3D_AUDIO_WAV_OK && size == 44);
    assert(dust3d_audio_wav_plan(2, 0, 1, 100, &size) == DUST3D_AUDIO_WAV_INVALID_RATE && size == 0);
    assert(dust3d_audio_wav_plan(2, 44100, 0, 100, &size) == DUST3D_AUDIO_WAV_INVALID_CHANNELS);
    assert(dust3d_audio_wav_plan(2, 44100, 32768, 100, &size) == DUST3D_AUDIO_WAV_INVALID_CHANNELS);
    assert(dust3d_audio_wav_plan(3, 44100, 2, 100, &size) == DUST3D_AUDIO_WAV_INCOMPLETE_FRAME);
    assert(dust3d_audio_wav_plan(2, UINT32_MAX, 1, 100, &size) == DUST3D_AUDIO_WAV_INVALID_RATE);
    assert(dust3d_audio_wav_plan(UINT64_MAX, 44100, 1, UINT64_MAX, &size) == DUST3D_AUDIO_WAV_RIFF_LIMIT);
    const uint64_t maximum = (uint64_t(UINT32_MAX) - 36) / 2;
    assert(dust3d_audio_wav_plan(maximum, 1, 1, UINT64_MAX, &size) == DUST3D_AUDIO_WAV_OK);
    assert(size == 44 + 2 * maximum);
    assert(dust3d_audio_wav_plan(maximum + 1, 1, 1, UINT64_MAX, &size) == DUST3D_AUDIO_WAV_RIFF_LIMIT);
    assert(dust3d_audio_wav_plan(5, 44100, 1, 53, &size) == DUST3D_AUDIO_WAV_QUOTA && size == 0);
    assert(dust3d_audio_wav_plan(0, 44100, 1, 43, nullptr) == DUST3D_AUDIO_WAV_QUOTA);
    int16_t samples[] = { 0, -1, 32767, -32768, 1 };
    Dust3DAudioWavStatus status = DUST3D_AUDIO_WAV_OK;
    auto* owner = dust3d_audio_wav_create(samples, 5, 44100, 1, 54, &status);
    assert(owner && status == DUST3D_AUDIO_WAV_OK);
    assert(dust3d_audio_wav_size(owner) == 54);
    const auto* bytes = dust3d_audio_wav_bytes(owner);
    assert(!std::memcmp(bytes, "RIFF", 4) && u32(bytes + 4) == 46);
    assert(!std::memcmp(bytes + 8, "WAVEfmt ", 8) && u32(bytes + 16) == 16);
    assert(bytes[20] == 1 && bytes[22] == 1 && u32(bytes + 24) == 44100);
    assert(u32(bytes + 28) == 88200 && bytes[32] == 2 && bytes[34] == 16);
    assert(!std::memcmp(bytes + 36, "data", 4) && u32(bytes + 40) == 10);
    const uint8_t expected[] = { 0, 0, 255, 255, 255, 127, 0, 128, 1, 0 };
    assert(!std::memcmp(bytes + 44, expected, 10));
    samples[0] = 99;
    assert(bytes[44] == 0); // owned copy must not alias the input
    int16_t stereo[] = { -1, 1, -32768, 32767 };
    auto* second = dust3d_audio_wav_create(stereo, 4, 48000, 2, 52, &status);
    assert(second && u32(dust3d_audio_wav_bytes(second) + 28) == 192000);
    assert(dust3d_audio_wav_bytes(second)[22] == 2 && dust3d_audio_wav_bytes(second)[32] == 4);
    const uint8_t stereoExpected[] = { 255, 255, 1, 0, 0, 128, 255, 127 };
    assert(!std::memcmp(dust3d_audio_wav_bytes(second) + 44, stereoExpected, 8));
    assert(bytes[44] == 0); // subsequent calls preserve earlier ownership
    dust3d_audio_wav_destroy(second);
    dust3d_audio_wav_destroy(owner);
    assert(!dust3d_audio_wav_create(nullptr, 1, 44100, 1, 46, &status));
    assert(status == DUST3D_AUDIO_WAV_NULL_SAMPLES);
    owner = dust3d_audio_wav_create(nullptr, 0, 44100, 1, 44, nullptr);
    assert(owner && dust3d_audio_wav_size(owner) == 44);
    dust3d_audio_wav_destroy(owner);
    dust3d_audio_wav_destroy(nullptr);
    assert(!dust3d_audio_wav_bytes(nullptr) && dust3d_audio_wav_size(nullptr) == 0);
}
