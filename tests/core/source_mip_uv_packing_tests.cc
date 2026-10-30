// SPDX-License-Identifier: MIT
#include <cmath>
#include <dust3d/base/object.h>
#include <dust3d/uv/source_texture_baker.h>
#include <dust3d/uv/source_uv_atlas.h>
#include <dust3d/uv/uv_atlas_builder.h>
#include <dust3d/uv/uv_map_packer.h>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* cause)
{
    if (!condition)
        throw std::runtime_error(cause);
}
dust3d::UvMapPacker::Part part(double width, double height, double identity)
{
    dust3d::UvMapPacker::Part value;
    value.width = width;
    value.height = height;
    value.localUv[std::array<dust3d::PositionKey, 3> { dust3d::PositionKey(identity, 0, 0), dust3d::PositionKey(identity, 1, 0), dust3d::PositionKey(identity, 0, 1) }]
        = { dust3d::Vector2(0, 0), dust3d::Vector2(1, 0), dust3d::Vector2(0, 1) };
    return value;
}
void verifyCharts(double a, double b, double c, double d, bool expectRotation)
{
    dust3d::UvMapPacker legacy;
    legacy.addPart(part(a, b, 1));
    legacy.addPart(part(c, d, 2));
    legacy.pack();
    dust3d::UvMapPacker packer;
    packer.setUsePackedChartExtents(true);
    packer.addPart(part(a, b, 1));
    packer.addPart(part(c, d, 2));
    packer.pack();
    const auto& layouts = packer.packedLayouts();
    const auto& legacyLayouts = legacy.packedLayouts();
    require(layouts.size() == 2, "Two charts must pack");
    require(legacyLayouts.size() == layouts.size() && legacy.packedTextureSize() == packer.packedTextureSize(), "Opt-in must preserve packed rectangle placement");
    require(layouts[0].left + layouts[0].width <= layouts[1].left || layouts[1].left + layouts[1].width <= layouts[0].left
            || layouts[0].top + layouts[0].height <= layouts[1].top || layouts[1].top + layouts[1].height <= layouts[0].top,
        "Packed chart interiors must remain disjoint");
    bool hasRotation = false;
    for (size_t index = 0; index < layouts.size(); ++index) {
        const auto& layout = layouts[index];
        const auto& original = legacyLayouts[index];
        require(layout.globalUv.size() == 1 && original.globalUv.size() == 1, "Each chart retains its triangle");
        require(layout.left == original.left && layout.top == original.top && layout.width == original.width
                && layout.height == original.height && layout.flipped == original.flipped,
            "Opt-in must retain legacy layout and rotation");
        const double originalWidth = index ? c : a;
        const double originalHeight = index ? d : b;
        const auto& originalUvs = original.globalUv.begin()->second;
        const double width = original.flipped ? originalHeight : originalWidth;
        const double height = original.flipped ? originalWidth : originalHeight;
        const size_t xCorner = original.flipped ? 2 : 1;
        const size_t yCorner = original.flipped ? 1 : 2;
        require(std::abs(originalUvs[xCorner].x() - (original.left * legacy.packedTextureSize() + width) / legacy.packedTextureSize()) < 1e-15
                && std::abs(originalUvs[yCorner].y() - (original.top * legacy.packedTextureSize() + height) / legacy.packedTextureSize()) < 1e-15,
            "Legacy default retains its inherited unquantized UV mapping");
        hasRotation |= layout.flipped;
        for (const auto& entry : layout.globalUv)
            for (const auto& uv : entry.second)
                require(uv.x() >= layout.left && uv.y() >= layout.top
                        && uv.x() <= layout.left + layout.width && uv.y() <= layout.top + layout.height,
                    "Chart corner must be contained by its producing layout");
    }
    require(hasRotation == expectRotation, "Fixture must exercise the intended rotation case");
}
void optedAtlasFeedsMipOwnership()
{
    const dust3d::Uuid first("00000000-0000-0000-0000-000000000001");
    const dust3d::Uuid second("00000000-0000-0000-0000-000000000002");
    dust3d::Object object;
    object.vertices = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 }, { 2, 0, 0 }, { 2.7654321, 0, 0 }, { 2, 1, 0 } };
    object.triangles = { { 0, 1, 2 }, { 3, 4, 5 } };
    const std::array<dust3d::Vector2, 3> corners { dust3d::Vector2(0, 0), dust3d::Vector2(1, 0), dust3d::Vector2(0, 1) };
    for (size_t index = 0; index < object.triangles.size(); ++index) {
        const auto& triangle = object.triangles[index];
        object.componentTriangleUvs[index ? second : first][{ dust3d::PositionKey(object.vertices[triangle[0]]),
            dust3d::PositionKey(object.vertices[triangle[1]]), dust3d::PositionKey(object.vertices[triangle[2]]) }] = corners;
    }
    std::map<dust3d::Uuid, dust3d::UvAtlasComponent> components;
    components[first] = {};
    components[second] = {};
    dust3d::UvAtlasOptions options;
    options.textureSize = 256;
    const auto legacy = dust3d::UvAtlasBuilder::build(object, components, options);
    bool legacyEscapes = false;
    for (const auto& layout : legacy.layouts)
        for (const auto& entry : layout.globalUv)
            for (const auto& uv : entry.second)
                legacyEscapes |= uv.x() > layout.left + layout.width + 1e-7 || uv.y() > layout.top + layout.height + 1e-7;
    require(legacyEscapes, "Production fixture must reproduce the inherited rounding discrepancy");
    options.usePackedChartExtents = true;
    const auto opted = dust3d::UvAtlasBuilder::build(object, components, options);
    require(opted.layouts.size() == 2 && opted.triangleUvKnown == std::vector<uint8_t>({ 1, 1 }), "Opt-in production atlas retains both known triangles");
    for (const auto& layout : opted.layouts)
        for (const auto& entry : layout.globalUv)
            for (const auto& uv : entry.second)
                require(uv.x() >= layout.left && uv.y() >= layout.top
                        && uv.x() <= layout.left + layout.width && uv.y() <= layout.top + layout.height,
                    "Opt-in production atlas confines every UV to its source layout");
    const auto source = dust3d::SourceUvAtlasBuilder::build(object, opted);
    std::vector<uint8_t> pixels(64 * 64 * 4, 255);
    dust3d::SourceTextureBakeInputs inputs;
    inputs.legacy.color = dust3d::TextureImageView { 64, 64, 256, pixels.size(), pixels.data() };
    dust3d::SourceTextureMipBakeOptions mip;
    mip.bake.colorSize = mip.bake.materialSize = 64;
    mip.legacyAtlas = &opted;
    const auto result = dust3d::SourceTextureBaker::bakeMipChain(source, inputs, mip);
    require(!result.hasUnprovenLegacyOwnership, "Production atlas mappings retain proven procedural ownership");
    const auto legacySource = dust3d::SourceUvAtlasBuilder::build(object, legacy);
    mip.legacyAtlas = &legacy;
    require(dust3d::SourceTextureBaker::bakeMipChain(legacySource, inputs, mip).hasUnprovenLegacyOwnership,
        "The same input reproduces the inherited ownership failure without opt-in");
    options.usePackedChartExtents = false;
    const auto defaultAgain = dust3d::UvAtlasBuilder::build(object, components, options);
    require(legacy.triangleUvs == defaultAgain.triangleUvs, "Legacy default mapping remains unchanged by opt-in");
}
}
int main()
{
    try {
        verifyCharts(123.4567, 97.1234, 61.23456, 47.89123, false);
        verifyCharts(15.23456, 47.89123, 15.23456, 97.1234, true);
        optedAtlasFeedsMipOwnership();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
