// SPDX-License-Identifier: MIT
// Unregistered. Syntax-check only while FishIdle evaluation is quarantined.
#include "ExperimentalFishIdle.h"
#include <cassert>
#include <limits>
#include <memory>

// Supply an ordinary authored Fish with assigned edges and the bundled Fish template.
void checkFishIdlePCMOwner(const uint8_t* archive, size_t archiveSize,
    const Dust3DGeneratedRigBoneInput* bones, size_t boneCount)
{
    Dust3DError error {};
    const Dust3DExperimentalFishIdleParameter parameters[] {
        { "durationSeconds", 15, "1", 1 },
        { "frameCount", 10, "32", 2 },
        { "driftFactor", 11, "3", 1 },
    };
    auto options = dust3d_experimental_fish_idle_default_pcm_options();
    options.surface = "water";
    options.surface_length = 5;
    assert(!dust3d_experimental_fish_idle_load_pcm(archive, archiveSize, bones, boneCount,
        parameters, 3, &options, &error));
    assert(error.status == DUST3D_STATUS_INVALID_DOCUMENT);

    options.surface = "Water";
    auto destroy = &dust3d_experimental_fish_idle_destroy;
    std::unique_ptr<Dust3DExperimentalFishIdle, decltype(destroy)> owner(
        dust3d_experimental_fish_idle_load_pcm(archive, archiveSize, bones, boneCount,
            parameters, 3, &options, &error),
        destroy);
    assert(owner && error.status == DUST3D_STATUS_OK);

    Dust3DExperimentalFishIdlePCMInfo info {};
    assert(dust3d_experimental_fish_idle_describe_pcm(owner.get(), &info));
    assert(info.visual.rest && info.visual.frame_count == 33 && info.visual.duration_seconds == 1);
    assert(info.sample_rate == 44100 && info.channel_count == 1 && info.sample_count == 44100);
    assert(info.output_bytes == info.visual.output_bytes + info.sample_count * sizeof(int16_t));
    Dust3DExperimentalFishIdleMatrix matrix {};
    assert(dust3d_experimental_fish_idle_matrix_at(owner.get(), 0, 0, &matrix));

    bool audible = false;
    for (size_t index = 0; index < info.sample_count; ++index) {
        int16_t sample = 0;
        assert(dust3d_experimental_fish_idle_pcm_at(owner.get(), index, &sample));
        audible |= sample != 0;
    }
    assert(audible);
    int16_t sentinel = 123;
    assert(!dust3d_experimental_fish_idle_pcm_at(owner.get(), info.sample_count, &sentinel));
    assert(!dust3d_experimental_fish_idle_pcm_at(owner.get(), std::numeric_limits<size_t>::max(), &sentinel));
    assert(!dust3d_experimental_fish_idle_pcm_at(nullptr, 0, &sentinel));
    assert(sentinel == 123);
    assert(!dust3d_experimental_fish_idle_describe_pcm(nullptr, &info));
    assert(info.visual.rest == nullptr && info.sample_count == 0 && info.output_bytes == 0);
}
