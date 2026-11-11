// SPDX-License-Identifier: MIT
// Compile-only until the paired animation preview runtime is reviewed.
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
