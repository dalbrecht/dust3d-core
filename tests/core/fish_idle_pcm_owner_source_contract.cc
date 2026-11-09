// SPDX-License-Identifier: MIT
// Unregistered. Syntax-check only while FishIdle evaluation is quarantined.
#include "ExperimentalFishIdle.h"
#include "fish_idle_pcm_fixture.h"
#include <cassert>
#include <cmath>
#include <iterator>
#include <limits>
#include <memory>
#include <string_view>

void checkFishIdlePCMOwner()
{
    Dust3DError error {};
    auto options = dust3d_experimental_fish_idle_default_pcm_options();
    options.surface = "water";
    options.surface_length = 5;
    assert(!dust3d_experimental_fish_idle_load_pcm(fishArchive, std::size(fishArchive), fishBones, std::size(fishBones),
        fishParameters, std::size(fishParameters), &options, &error));
    assert(error.status == DUST3D_STATUS_INVALID_DOCUMENT);

    options.surface = fishSurface;
    options.surface_length = std::size(fishSurface) - 1;
    auto destroy = &dust3d_experimental_fish_idle_destroy;
    std::unique_ptr<Dust3DExperimentalFishIdle, decltype(destroy)> owner(
        dust3d_experimental_fish_idle_load_pcm(fishArchive, std::size(fishArchive), fishBones, std::size(fishBones),
            fishParameters, std::size(fishParameters), &options, &error),
        destroy);
    assert(owner && error.status == DUST3D_STATUS_OK);

    Dust3DExperimentalFishIdlePCMInfo info {};
    assert(dust3d_experimental_fish_idle_describe_pcm(owner.get(), &info));
    assert(info.visual.rest && info.visual.frame_count == 33 && info.visual.duration_seconds == 1);
    assert(info.visual.joint_count == std::size(fishBones));
    const auto* mesh = dust3d_generated_rig_mesh(info.visual.rest);
    assert(mesh && dust3d_mesh_vertex_count(mesh) > 0);
    assert(dust3d_generated_rig_weights(info.visual.rest));
    for (size_t joint = 0; joint < info.visual.joint_count; ++joint) {
        size_t nameLength = 0;
        const auto* name = dust3d_generated_rig_joint_name(info.visual.rest, joint, &nameLength);
        assert(name && std::string_view(name, nameLength) == fishBones[joint].name);
    }
    assert(info.sample_rate == 44100 && info.channel_count == 1 && info.sample_count == 44100);
    assert(info.output_bytes == info.visual.output_bytes + info.sample_count * sizeof(int16_t));

    auto selectedOptions = dust3d_experimental_fish_idle_default_options();
    std::unique_ptr<Dust3DExperimentalFishIdle, decltype(destroy)> selected(
        dust3d_experimental_fish_idle_load_selected(fishArchive, std::size(fishArchive), fishBones, std::size(fishBones),
            fishAnimationId, std::size(fishAnimationId) - 1, 1, &selectedOptions, &error),
        destroy);
    assert(selected && error.status == DUST3D_STATUS_OK);
    size_t selectedLength = 0;
    const auto* selectedId = dust3d_experimental_fish_idle_selected_id(selected.get(), &selectedLength);
    assert(selectedId && std::string_view(selectedId, selectedLength) == fishAnimationId);
    Dust3DExperimentalFishIdlePCMInfo selectedInfo {};
    assert(dust3d_experimental_fish_idle_describe_pcm(selected.get(), &selectedInfo));
    assert(selectedInfo.visual.frame_count == info.visual.frame_count && selectedInfo.sample_count == info.sample_count);

    Dust3DExperimentalFishIdleGroundMotion motion {};
    assert(dust3d_experimental_fish_idle_ground_motion(owner.get(), &motion));
    assert(std::isfinite(motion.speed) && std::isfinite(motion.direction_x) && std::isfinite(motion.direction_z));
    const auto savedMotion = motion;
    assert(!dust3d_experimental_fish_idle_ground_motion(nullptr, &motion));
    assert(motion.speed == savedMotion.speed && motion.direction_x == savedMotion.direction_x
        && motion.direction_z == savedMotion.direction_z);
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
