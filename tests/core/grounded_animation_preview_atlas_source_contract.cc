// SPDX-License-Identifier: MIT
// Compile-only until the paired animation preview runtime is reviewed.
#include "ExperimentalFishIdle.h"
#include "GroundedFishIdlePreview.h"
#include "SourceBakedMesh.h"
#include <cassert>

void checkGroundedPreviewAtlas(const dust3d::native::GeneratedRigInput& input,
    std::unique_ptr<Dust3DMesh> metadata, const dust3d::RigStructure& rig,
    const dust3d::Snapshot& authored, const Dust3DSourceBakeOptions& options)
{
    using namespace dust3d::native;
    const GeneratedRigDrawBakeInput bake { authored, options };
    const auto preview = generateGroundedFishIdleDrawPreview(input, std::move(metadata), rig,
        {}, {}, 15000000, options.legacy.cancellation, {}, "FishIdle", &bake);
    assert(preview.rest->sourceBake);
    assert(preview.rest->sourceSurfaces);
    const auto cornerCount = preview.rest->mesh->vertices.size();
    assert(preview.rest->weights.size() == cornerCount);
    assert(preview.rest->sourceBake->triangles.size() == cornerCount / 3);
    assert(preview.rest->sourceSurfaces->triangles().size() == cornerCount / 3);
}

void checkSelectedAtlasBridge(const uint8_t* archive, size_t archiveSize,
    const Dust3DGeneratedRigBoneInput* bones, size_t boneCount, const Dust3DSourceBakeOptions& bake)
{
    auto options = dust3d_experimental_fish_idle_default_options();
    options.rig.cancellation = bake.legacy.cancellation;
    Dust3DError error {};
    auto* owner = dust3d_experimental_fish_idle_load_selected_textured(
        archive, archiveSize, bones, boneCount, "selected", 8, 1, &options, &bake, &error);
    dust3d_experimental_fish_idle_destroy(owner);
}
