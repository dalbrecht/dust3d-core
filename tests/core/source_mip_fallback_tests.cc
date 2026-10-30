// SPDX-License-Identifier: MIT
#include <array>
#include <dust3d/uv/source_texture_baker.h>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void require(bool condition, const char* cause)
{
    if (!condition)
        throw std::runtime_error(cause);
}
void unprovenOwnershipKeepsBaseAndDisablesMipmaps()
{
    dust3d::SourceUvAtlas atlas;
    atlas.legacyChart = dust3d::SourceUvChart {};
    dust3d::UvAtlas legacy;
    dust3d::UvMapPacker::Layout layout;
    layout.left = .05;
    layout.top = .1;
    layout.width = .25;
    layout.height = .2;
    legacy.layouts.push_back(layout);
    dust3d::SourceUvTriangle triangle;
    triangle.known = true;
    triangle.packedUvs = { dust3d::Vector2(.4, .4), dust3d::Vector2(.5, .4), dust3d::Vector2(.4, .5) };
    atlas.triangles.push_back(triangle);
    legacy.triangleUvs = { triangle.packedUvs };
    legacy.triangleUvKnown = { 1 };
    std::vector<uint8_t> pixels(32 * 32 * 4, 255);
    dust3d::SourceTextureBakeInputs inputs;
    inputs.legacy.color = dust3d::TextureImageView { 32, 32, 128, pixels.size(), pixels.data() };
    dust3d::SourceTextureMipBakeOptions options;
    options.bake.colorSize = options.bake.materialSize = 32;
    options.legacyAtlas = &legacy;
    const auto base = dust3d::SourceTextureBaker::bake(atlas, inputs, options.bake);
    const auto fallback = dust3d::SourceTextureBaker::bakeMipChain(atlas, inputs, options);
    require(fallback.hasUnprovenLegacyOwnership, "Unproven ownership retains its cause");
    require(std::string(fallback.fallbackDiagnostic()).find("no single owning island") != std::string::npos, "Fallback diagnostic names the cause");
    require(fallback.maximumSamplingLod == 0 && fallback.colorLevels.empty() && fallback.metallicLevels.empty()
            && fallback.roughnessLevels.empty() && fallback.emissiveLevels.empty() && fallback.normalLevels.empty(),
        "Unproven ownership exposes no mip levels");
    require(fallback.base.color.pixels == base.color.pixels && fallback.base.metallic.pixels == base.metallic.pixels
            && fallback.base.roughness.pixels == base.roughness.pixels && fallback.base.emissive.pixels == base.emissive.pixels,
        "Fallback retains the base maps");
    triangle.packedUvs = { dust3d::Vector2(.05, .1), dust3d::Vector2(.3, .1), dust3d::Vector2(.05, .3) };
    atlas.triangles[0] = triangle;
    legacy.triangleUvs[0] = triangle.packedUvs;
    const auto proven = dust3d::SourceTextureBaker::bakeMipChain(atlas, inputs, options);
    require(!proven.hasUnprovenLegacyOwnership && std::string(proven.fallbackDiagnostic()).empty(), "Proven ownership does not report a fallback");
    require(proven.maximumSamplingLod > 0 && !proven.colorLevels.empty(), "Proven ownership retains safe mip levels");
}
void mixedSyntheticAtlasRetainsBaseWithoutGuessingOwnership()
{
    dust3d::SourceUvAtlas atlas;
    atlas.legacyChart = dust3d::SourceUvChart { 0, 0, .45, 1, false };
    dust3d::SourceUvTriangle imported;
    imported.known = true;
    imported.modelKey = "synthetic";
    imported.importedChart = dust3d::SourceUvChart { .65, .1, .3, .8, false };
    imported.packedUvs = { imported.importedChart->map({ 0, 0 }), imported.importedChart->map({ 1, 0 }), imported.importedChart->map({ 0, 1 }) };
    atlas.triangles.push_back(imported);
    const std::array<dust3d::Vector2, 3> localUvs { dust3d::Vector2(.4, .4), dust3d::Vector2(.5, .4), dust3d::Vector2(.4, .5) };
    dust3d::SourceUvTriangle procedural;
    procedural.known = true;
    for (size_t corner = 0; corner < localUvs.size(); ++corner)
        procedural.packedUvs[corner] = atlas.legacyChart->map(localUvs[corner]);
    atlas.triangles.push_back(procedural);
    dust3d::UvAtlas legacy;
    dust3d::UvMapPacker::Layout layout;
    layout.left = .05;
    layout.top = .1;
    layout.width = .25;
    layout.height = .2;
    legacy.layouts.push_back(layout);
    legacy.triangleUvs.resize(2);
    legacy.triangleUvs[1] = localUvs;
    legacy.triangleUvKnown = { 0, 1 };
    std::vector<uint8_t> pixels(32 * 32 * 4, 255);
    dust3d::SourceTextureBakeInputs inputs;
    inputs.legacy.color = dust3d::TextureImageView { 32, 32, 128, pixels.size(), pixels.data() };
    dust3d::SourceTextureMaterial material;
    material.baseColor = { 1, 0, 1, 1 };
    inputs.materials.emplace("synthetic", material);
    inputs.triangleColors.resize(2, { dust3d::Color(1, 1, 1), dust3d::Color(1, 1, 1), dust3d::Color(1, 1, 1) });
    dust3d::SourceTextureMipBakeOptions options;
    options.bake.colorSize = options.bake.materialSize = 64;
    options.legacyAtlas = &legacy;
    const auto base = dust3d::SourceTextureBaker::bake(atlas, inputs, options.bake);
    const auto fallback = dust3d::SourceTextureBaker::bakeMipChain(atlas, inputs, options);
    require(fallback.hasUnprovenLegacyOwnership && fallback.maximumSamplingLod == 0 && fallback.colorLevels.empty(), "Mixed atlas reports unproven procedural ownership and disables mips");
    require(fallback.base.color.pixels == base.color.pixels && fallback.base.metallic.pixels == base.metallic.pixels
            && fallback.base.roughness.pixels == base.roughness.pixels && fallback.base.emissive.pixels == base.emissive.pixels,
        "Mixed atlas fallback retains the base maps");
}
}
int main()
{
    try {
        unprovenOwnershipKeepsBaseAndDisablesMipmaps();
        mixedSyntheticAtlasRetainsBaseWithoutGuessingOwnership();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
