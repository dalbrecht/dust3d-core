// SPDX-License-Identifier: MIT
#include "Dust3DAudioWav.h"
#include <stdio.h>
int main(void)
{
    const int16_t samples[] = { -32768, -1, 0, 1, 32767 };
    Dust3DAudioWavStatus status = DUST3D_AUDIO_WAV_ENCODING_FAILED;
    Dust3DAudioWav* owner = dust3d_audio_wav_create(samples, 5, 44100, 1, 54, &status);
    if (!owner || status != DUST3D_AUDIO_WAV_OK)
        return 1;
    const uint64_t size = dust3d_audio_wav_size(owner);
    const size_t written = fwrite(dust3d_audio_wav_bytes(owner), 1, (size_t)size, stdout);
    dust3d_audio_wav_destroy(owner);
    return written == size ? 0 : 2;
}
