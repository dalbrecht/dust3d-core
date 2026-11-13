// SPDX-License-Identifier: MIT
#include "Dust3DBridge.h"
#include <string.h>

int main(void)
{
    const Dust3DSoundEvent event = { 0.1f, 0.75f, 0.0f, DUST3D_SOUND_IMPACT };
    Dust3DError error = { 0 };
    Dust3DSoundWav* owner = dust3d_sound_wav_create(&event, 1, 0.5f, 1, &error);
    if (!owner || error.status != DUST3D_STATUS_OK)
        return 1;

    const uint8_t* bytes = dust3d_sound_wav_bytes(owner);
    const size_t count = dust3d_sound_wav_size(owner);
    const int valid = bytes && count == 44144 && !memcmp(bytes, "RIFF", 4) && !memcmp(bytes + 8, "WAVE", 4);
    dust3d_sound_wav_destroy(owner);
    if (!valid)
        return 2;

    owner = dust3d_sound_wav_create(0, 1, 0.5f, 1, &error);
    if (owner || error.status != DUST3D_STATUS_INVALID_DOCUMENT) {
        dust3d_sound_wav_destroy(owner);
        return 3;
    }
    return 0;
}
