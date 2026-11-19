// SPDX-License-Identifier: MIT
// Executed by the opt-in private animation runtime contract target.
#include "ExperimentalFishIdle.h"
#include "ExperimentalFishIdleFbx.h"
#include "fish_idle_pcm_fixture.h"
#include <cassert>
#include <iterator>
#include <memory>
#include <string_view>

void checkSelectedFishIdleFbxOwner()
{
    Dust3DError error {};
    const auto config = dust3d_generated_rig_default_options();
    const Dust3DExperimentalFishIdleFbxClipID id { fishAnimationId, std::size(fishAnimationId) - 1 };
    const Dust3DExperimentalFishIdleFbxClipID duplicate[] { id, id };
    assert(!dust3d_experimental_fish_idle_fbx_create(
        fishArchive, std::size(fishArchive), fishBones, std::size(fishBones), duplicate, 2, &config, 16 * 1024 * 1024, &error));
    assert(error.status == DUST3D_STATUS_INVALID_DOCUMENT);

    assert(!dust3d_experimental_fish_idle_fbx_create_baked(
        fishArchive, std::size(fishArchive), fishBones, std::size(fishBones), &id, 1, &config, nullptr, 16 * 1024 * 1024, &error));
    assert(error.status == DUST3D_STATUS_INVALID_DOCUMENT);

    std::unique_ptr<Dust3DExperimentalFishIdleFbx, decltype(&dust3d_experimental_fish_idle_fbx_destroy)> owner(
        dust3d_experimental_fish_idle_fbx_create(
            fishArchive, std::size(fishArchive), fishBones, std::size(fishBones), &id, 1, &config, 16 * 1024 * 1024, &error),
        dust3d_experimental_fish_idle_fbx_destroy);
    assert(owner && error.status == DUST3D_STATUS_OK);
    const auto* data = dust3d_experimental_fish_idle_fbx_bytes(owner.get());
    const auto size = dust3d_experimental_fish_idle_fbx_size(owner.get());
    assert(data && size > 27);
    const std::string_view bytes(reinterpret_cast<const char*>(data), size);
    assert(bytes.find("Kaydara FBX Binary") == 0);
    assert(bytes.find("AnimationStack") != std::string_view::npos);

    Dust3DBakeOptions bake {};
    bake.struct_size = sizeof(bake);
    bake.maximum_uv_payload_bytes = 4 * 1024 * 1024;
    bake.maximum_bake_payload_bytes = 8 * 1024 * 1024;
    bake.atlas_texture_size = 256;
    bake.color_size = 256;
    bake.normal_size = 128;
    bake.material_size = 128;
    owner.reset(dust3d_experimental_fish_idle_fbx_create_baked(
        fishArchive, std::size(fishArchive), fishBones, std::size(fishBones), &id, 1, &config, &bake, 16 * 1024 * 1024, &error));
    assert(owner && error.status == DUST3D_STATUS_OK);
    const std::string_view textured(
        reinterpret_cast<const char*>(dust3d_experimental_fish_idle_fbx_bytes(owner.get())),
        dust3d_experimental_fish_idle_fbx_size(owner.get()));
    assert(textured.find("AnimationStack") != std::string_view::npos);
    assert(textured.find("LayerElementUV") != std::string_view::npos);
    assert(textured.find("Material") != std::string_view::npos);
}
