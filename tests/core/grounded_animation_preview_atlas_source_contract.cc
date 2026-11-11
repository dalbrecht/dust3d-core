// SPDX-License-Identifier: MIT
// Compile-only until the paired animation preview runtime is reviewed.
#include "ExperimentalFishIdle.h"
#include "GroundedFishIdlePreview.h"
#include "SourceBakedMesh.h"
#include <cassert>

void checkGroundedPreviewAtlas(const dust3d::native::GeneratedRigInput& input,
    std::unique_ptr<Dust3DMesh> metadata, const dust3d::RigStructure& rig,
    dust3d::native::GeneratedRigDrawBakeInput& bake, const Dust3DSourceBakeOptions& options)
{
    using namespace dust3d::native;
    const auto preview = generateGroundedFishIdleDrawPreview(input, std::move(metadata), rig,
        {}, {}, 15000000, options.legacy.cancellation, {}, "FishIdle", &bake);
    assert(preview.rest->sourceBake);
    assert(preview.rest->sourceSurfaces);
    const auto cornerCount = preview.rest->mesh->vertices.size();
    assert(preview.rest->weights.size() == cornerCount);
    assert(preview.rest->sourceBake->triangles.size() == cornerCount / 3);
    assert(preview.rest->sourceSurfaces->triangles().size() == cornerCount / 3);
}

void checkPreparedAnimationMaterials(const uint8_t* archive, size_t archiveSize,
    const Dust3DGeneratedRigBoneInput* bones, size_t boneCount, const Dust3DSourceBakeOptions& bake)
{
    using namespace dust3d::native;
    auto options = dust3d_generated_rig_default_options();
    options.cancellation = bake.legacy.cancellation;
    const auto prepared = prepareGeneratedRigSource(
        archive, archiveSize, "Fish", 4, bones, boneCount, options, &bake);
    assert(prepared.bake && prepared.bake->surfaces && prepared.bake->atlas);
}

void checkSelectedAtlasBridge(const uint8_t* archive, size_t archiveSize,
    const Dust3DGeneratedRigBoneInput* bones, size_t boneCount, const Dust3DSourceBakeOptions& bake)
{
    auto options = dust3d_experimental_fish_idle_default_options();
    options.rig.cancellation = bake.legacy.cancellation;
    Dust3DError error {};
    auto* owner = dust3d_experimental_fish_idle_load_selected_textured(
        archive, archiveSize, bones, boneCount, "selected", 8, 1, &options, &bake, &error);
    if (owner) {
        Dust3DExperimentalFishIdleInfo info {};
        assert(dust3d_experimental_fish_idle_describe(owner, &info));
        size_t triangleCount = 0;
        assert(dust3d_experimental_fish_idle_atlas_triangles(owner, &triangleCount));
        assert(triangleCount == dust3d_mesh_vertex_count(dust3d_generated_rig_mesh(info.rest)) / 3);
        const auto modelCount = dust3d_experimental_fish_idle_atlas_model_count(owner);
        const auto componentCount = dust3d_experimental_fish_idle_atlas_component_count(owner);
        const auto* triangles = dust3d_experimental_fish_idle_atlas_triangles(owner, &triangleCount);
        for (size_t index = 0; index < triangleCount; ++index) {
            const auto& triangle = triangles[index];
            if (triangle.model_index != SIZE_MAX) {
                assert(triangle.model_index < modelCount);
                assert(dust3d_experimental_fish_idle_atlas_model_id(owner, triangle.model_index));
            }
            if (triangle.component_index != SIZE_MAX) {
                assert(triangle.component_index < componentCount);
                Dust3DSurfaceComponentKind kind = DUST3D_SURFACE_COMPONENT_UNKNOWN;
                assert(dust3d_experimental_fish_idle_atlas_component_id(owner, triangle.component_index, &kind));
                assert(kind != DUST3D_SURFACE_COMPONENT_UNKNOWN);
            }
        }
        assert(!dust3d_experimental_fish_idle_atlas_component_id(owner, componentCount, nullptr));
        const auto payload = dust3d_experimental_fish_idle_atlas_payload_bytes(owner);
        assert(payload > 0 && payload <= dust3d_generated_rig_output_bytes(info.rest));
        const auto color = dust3d_experimental_fish_idle_atlas_texture(owner, DUST3D_TEXTURE_COLOR);
        assert(color.pixels && color.width > 0);
        assert(!dust3d_experimental_fish_idle_atlas_texture(owner, UINT32_MAX).pixels);
    }
    size_t count = 1;
    assert(!dust3d_experimental_fish_idle_atlas_triangles(nullptr, &count) && count == 0);
    dust3d_experimental_fish_idle_destroy(owner);
}

void checkSelectedVertexColorFallback(const uint8_t* archive, size_t archiveSize,
    const Dust3DGeneratedRigBoneInput* bones, size_t boneCount)
{
    auto options = dust3d_experimental_fish_idle_default_options();
    Dust3DError error {};
    auto* owner = dust3d_experimental_fish_idle_load_selected_vertex_colors(
        archive, archiveSize, bones, boneCount, "selected", 8, 1, &options, &error);
    if (owner) {
        Dust3DExperimentalFishIdlePCMInfo info {};
        assert(dust3d_experimental_fish_idle_describe_pcm(owner, &info));
        assert(info.visual.rest);
        assert(info.sample_count > 0);
        assert(dust3d_experimental_fish_idle_atlas_payload_bytes(owner) == 0);
    }
    dust3d_experimental_fish_idle_destroy(owner);
}
