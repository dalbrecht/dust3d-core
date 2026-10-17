// SPDX-License-Identifier: MIT
#ifndef DUST3D_UV_TEXTURE_BAKER_INTERNAL_H_
#define DUST3D_UV_TEXTURE_BAKER_INTERNAL_H_

#include <array>
#include <dust3d/uv/texture_baker.h>
#include <set>

namespace dust3d {
namespace detail {
    struct SurfaceTriangle {
        std::array<Vector3, 3> positions;
        std::array<Vector2, 3> uvs;
        std::array<size_t, 3> vertices {};
        Vector3 tangent;
        double handedness = 1;
        double worldPerUv = 0;
    };
    // Internal prevalidated stages, isolated for deterministic cancellation contracts.
    std::vector<Vector3> computeTextureChartNormals(size_t vertexCount, const std::vector<SurfaceTriangle>& triangles, const TextureBakeOptions& options);
    void applyTextureRowEvents(const std::vector<size_t>& ending, const std::vector<size_t>& starting, std::set<size_t>& active, const TextureBakeOptions& options);

    struct TextureMaterialActivation {
        bool hasMetallic = false;
        bool hasRoughness = false;
        bool hasEmissive = false;
    };
    // Prevalidated, allocation-free activation stage before output accounting.
    TextureMaterialActivation detectTextureMaterialMaps(const UvAtlas& atlas, const TextureBakeInputs& inputs, const TextureBakeOptions& options);

    struct TextureChartSource {
        const UvAtlasGradient* gradient = nullptr;
        const TextureImageView* image = nullptr;
        TextureMaterial material;
    };

    // Internal source table: records borrow atlas/inputs for the current call.
    // Resolve once per layout so pixel callbacks perform no collection searches.
    std::vector<TextureChartSource> resolveTextureChartSources(const UvAtlas& atlas, const TextureBakeInputs& inputs, const TextureBakeOptions& options);

    // Internal stage: caller validates geometry/UVs and charges output first.
    // Kept separate so cancellation after validation is tested without timing races.
    std::vector<TextureTangent> buildTextureTangents(const Object& object, const UvAtlas& atlas, const TextureBakeOptions& options);
}
}
#endif
