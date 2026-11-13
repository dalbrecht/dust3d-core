// SPDX-License-Identifier: MIT
#ifndef DUST3D_AUDIO_WAV_H
#define DUST3D_AUDIO_WAV_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum Dust3DAudioWavStatus {
    DUST3D_AUDIO_WAV_OK = 0,
    DUST3D_AUDIO_WAV_INVALID_RATE,
    DUST3D_AUDIO_WAV_INVALID_CHANNELS,
    DUST3D_AUDIO_WAV_INCOMPLETE_FRAME,
    DUST3D_AUDIO_WAV_RIFF_LIMIT,
    DUST3D_AUDIO_WAV_QUOTA,
    DUST3D_AUDIO_WAV_PLATFORM_LIMIT,
    DUST3D_AUDIO_WAV_NULL_SAMPLES,
    DUST3D_AUDIO_WAV_ALLOCATION_FAILED,
    DUST3D_AUDIO_WAV_ENCODING_FAILED
} Dust3DAudioWavStatus;

typedef struct Dust3DAudioWav Dust3DAudioWav;

/** Validate signed PCM16 interleaved sample metadata without allocation or sample access.
 * sample_count counts samples, not frames. output_quota includes the 44-byte header.
 * Empty PCM is valid. If nonnull, output_size is zeroed on failure and set on success.
 */
Dust3DAudioWavStatus dust3d_audio_wav_plan(uint64_t sample_count, uint32_t sample_rate,
    uint32_t channels, uint64_t output_quota, uint64_t* output_size);

/** Copy samples into an owned RIFF/WAVE PCM16 encoding. No synthesis or I/O occurs.
 * samples must point to sample_count readable int16_t values for this call; null is
 * allowed only for zero samples. Input must remain stable until the call returns.
 * Returns null on failure; optional status receives the precise result.
 */
Dust3DAudioWav* dust3d_audio_wav_create(const int16_t* samples, uint64_t sample_count,
    uint32_t sample_rate, uint32_t channels, uint64_t output_quota, Dust3DAudioWavStatus* status);
/** Borrow immutable bytes until owner destruction. Null owner returns null. */
const uint8_t* dust3d_audio_wav_bytes(const Dust3DAudioWav* owner);
/** Encoded byte count; null owner returns zero. */
uint64_t dust3d_audio_wav_size(const Dust3DAudioWav* owner);
/** Release the owner and all borrowed bytes. Null is accepted. */
void dust3d_audio_wav_destroy(Dust3DAudioWav* owner);
#ifdef __cplusplus
}
#endif
#endif
