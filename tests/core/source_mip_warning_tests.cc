// SPDX-License-Identifier: MIT
#include <dust3d/base/retained_payload_budget.h>
#include <dust3d/base/object.h>
#include <dust3d/uv/texture_baker.h>
#include <iostream>
#include <native/Engine/EngineInternal.h>
#include <native/Engine/MipAtlasPreparation.h>
#include <native/Engine/SourceBakedMesh.h>
#include <stdexcept>

namespace {
void require(bool condition, const char* cause)
{
    if (!condition)
        throw std::runtime_error(cause);
}
void fallbackWarningPreservesCauseAndChargesOwnedGrowth()
{
    std::string warning = "Existing diagnostic.";
    dust3d::RetainedPayloadBudget budget(4096);
    budget.accountDynamic(warning);
    const auto before = budget.usedBytes();
    const auto previousCapacity = warning.capacity();
    dust3d::native::appendSourceMipFallbackWarning(warning, "Known procedural mip triangle has no single owning island; mipmaps are disabled.", budget);
    require(warning == "Existing diagnostic. Known procedural mip triangle has no single owning island; mipmaps are disabled.", "Both original causes remain visible");
    require(budget.usedBytes() == before + warning.capacity() - previousCapacity, "Warning capacity growth is charged exactly");
    const auto retained = warning;
    const auto charge = budget.usedBytes();
    dust3d::native::appendSourceMipFallbackWarning(warning, "", budget);
    require(warning == retained && budget.usedBytes() == charge, "An empty diagnostic does not alter the warning or charge");
}
void fallbackWarningRejectsInsufficientQuota()
{
    std::string warning;
    dust3d::RetainedPayloadBudget budget(warning.capacity() + 1);
    budget.accountDynamic(warning);
    try {
        dust3d::native::appendSourceMipFallbackWarning(warning, "Known procedural mip triangle has no single owning island; mipmaps are disabled.", budget);
    } catch (const dust3d::native::DocumentError& error) {
        require(error.status() == DUST3D_STATUS_LIMIT_EXCEEDED, "Quota failure preserves typed limit status");
        return;
    }
    throw std::runtime_error("Warning growth exceeded its quota without a failure");
}
void normalPlanMatchesBakedImages()
{
    const dust3d::Uuid first("00000000-0000-0000-0000-000000000001");
    const dust3d::Uuid second("00000000-0000-0000-0000-000000000002");
    dust3d::Object object;
    object.vertices = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 }, { 2, 0, 0 }, { 3, 0, 0 }, { 2, 1, 0 } };
    object.triangles = { { 0, 1, 2 }, { 3, 4, 5 } };
    const std::array<dust3d::Vector2, 3> corners { dust3d::Vector2(0, 0), dust3d::Vector2(1, 0), dust3d::Vector2(0, 1) };
    for (size_t index = 0; index < 2; ++index) {
        const auto& triangle = object.triangles[index];
        object.componentTriangleUvs[index ? second : first][{ dust3d::PositionKey(object.vertices[triangle[0]]), dust3d::PositionKey(object.vertices[triangle[1]]), dust3d::PositionKey(object.vertices[triangle[2]]) }] = corners;
    }
    std::map<dust3d::Uuid, dust3d::UvAtlasComponent> components { { first, {} }, { second, {} } };
    dust3d::UvAtlasOptions atlasOptions;
    atlasOptions.textureSize = 256;
    atlasOptions.usePackedChartExtents = true;
    atlasOptions.paddingFraction = .015625;
    const auto atlas = dust3d::UvAtlasBuilder::build(object, components, atlasOptions);
    dust3d::TextureBakeInputs inputs;
    dust3d::TextureBakeOptions options;
    options.colorSize = options.materialSize = 256;
    options.normalSize = 32;
    auto check = [&](bool expected) {
        require(dust3d::TextureBaker::willProduceNormal(object, atlas, inputs, options) == expected, "Normal preflight must match expected presence");
        require(bool(dust3d::TextureBaker::bake(object, atlas, inputs, options).normal) == expected, "Normal preflight must match real bake output");
    };
    check(false);
    bool absentMip = true;
    std::string absentDiagnostic;
    const auto absent = dust3d::native::buildNormalAwareMipAtlas(object, components, atlasOptions, options,
        dust3d::native::sourceMipPaddingFraction(256, 256, 32, true), absentMip, absentDiagnostic, [&] { return inputs; });
    require(atlas.layouts.size() == 2 && absent.layouts.size() == 2, "Both initial and absent-normal atlases retain two islands");
    require(absentMip && absent.layouts[0].left == atlas.layouts[0].left && absent.layouts[0].width == atlas.layouts[0].width,
        "Absent normal must preserve initial atlas packing");
    bool baseOnly = false;
    bool resolvedForBase = false;
    std::string baseDiagnostic;
    (void)dust3d::native::buildNormalAwareMipAtlas(object, components, atlasOptions, options,
        dust3d::native::sourceMipPaddingFraction(256, 256, 32, true), baseOnly, baseDiagnostic,
        [&] { resolvedForBase = true; return inputs; });
    require(!resolvedForBase, "Base-only requests must not run the normal preflight");
    auto& surface = inputs.surfaces[first];
    surface.wrinkleStrength = 1;
    dust3d::ClothFolds::Fold fold;
    fold.width = .1;
    fold.depth = .1;
    fold.points = { { .1, .1, 0 }, { .5, .1, 0 } };
    fold.normals = { { 0, 0, 1 }, { 0, 0, 1 } };
    surface.folds.push_back(fold);
    check(true);
    const auto normalPadding = dust3d::native::sourceMipPaddingFraction(256, 256, 32, true);
    bool shouldMip = true;
    std::string diagnostic;
    const auto padded = dust3d::native::buildNormalAwareMipAtlas(object, components, atlasOptions, options, normalPadding, shouldMip, diagnostic, [&] { return inputs; });
    require(padded.layouts.size() == 2, "Repacked atlas retains both islands");
    require(shouldMip && padded.layouts[0].left != atlas.layouts[0].left, "Production planning must repack with the enabled normal dimension");
    atlasOptions.paddingFraction = normalPadding;
    const auto baked = dust3d::TextureBaker::bake(object, padded, inputs, options);
    require(bool(baked.normal), "Normal-aware atlas must produce its normal map");
    dust3d::SourceUvAtlasOptions sourceOptions;
    sourceOptions.textureSize = 256;
    sourceOptions.paddingFraction = atlasOptions.paddingFraction;
    const auto source = dust3d::SourceUvAtlasBuilder::build(object, padded, sourceOptions);
    dust3d::SourceTextureBakeInputs sourceInputs;
    sourceInputs.legacy.color = dust3d::TextureImageView { baked.color.width, baked.color.height, baked.color.width * 4, baked.color.pixels.size(), baked.color.pixels.data() };
    sourceInputs.legacy.normal = dust3d::TextureImageView { baked.normal->width, baked.normal->height, baked.normal->width * 4, baked.normal->pixels.size(), baked.normal->pixels.data() };
    dust3d::SourceTextureMipBakeOptions mip;
    mip.bake.colorSize = mip.bake.materialSize = 256;
    mip.bake.normalSize = 32;
    mip.legacyAtlas = &padded;
    const auto chain = dust3d::SourceTextureBaker::bakeMipChain(source, sourceInputs, mip);
    require(chain.maximumSamplingLod > 0 && !chain.normalLevels.empty(), "Multiple normal-aware islands must admit useful normal mip levels");
    surface.wrinkleStrength = 0;
    check(false);
    surface.wrinkleStrength = 1;
    surface.folds[0].points[1] = surface.folds[0].points[0];
    check(false);
}
void paddingHeuristicUsesSmallestBoundedMap()
{
    require(dust3d::native::sourceMipPaddingFraction(256, 64, 128, true) == .0625, "Smallest map sets four-texel padding");
    require(dust3d::native::sourceMipPaddingFraction(1, 8192, 8192, true) == .125, "Small maps cap the padding fraction");
    require(dust3d::native::sourceMipPaddingFraction(256, 256, 1, false) == .015625, "Absent normal map must not affect padding");
    require(dust3d::native::sourceMipPaddingFraction(256, 256, 1, true) == .125, "Enabled normal map determines padding");
    try {
        (void)dust3d::native::sourceMipPaddingFraction(0, 64, 128, true);
        throw std::runtime_error("Zero map dimension unexpectedly succeeded");
    } catch (const dust3d::native::DocumentError& error) {
        require(error.status() == DUST3D_STATUS_INVALID_DOCUMENT, "Invalid map dimension retains typed status");
    }
}
}
int main()
{
    try {
        fallbackWarningPreservesCauseAndChargesOwnedGrowth();
        fallbackWarningRejectsInsufficientQuota();
        paddingHeuristicUsesSmallestBoundedMap();
        normalPlanMatchesBakedImages();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
