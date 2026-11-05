// SPDX-License-Identifier: MIT
#include <algorithm>
#include <cmath>
#include <dust3d/base/object.h>
#include <dust3d/uv/texture_baker.h>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}
const dust3d::Uuid owner("{cccccccc-cccc-4ccc-8ccc-cccccccccccc}");
struct Fixture {
    dust3d::Object object;
    dust3d::UvAtlas atlas;
    dust3d::TextureBakeOptions options;
    Fixture()
    {
        object.vertices = { { 0, 0, 0 }, { 1, 0, 0 }, { 1, 1, 0 }, { 0, 1, 0 } };
        object.triangles = { { 0, 1, 2 }, { 0, 2, 3 } };
        atlas.triangleUvs = { { dust3d::Vector2(0, 0), dust3d::Vector2(1, 0), dust3d::Vector2(1, 1) },
            { dust3d::Vector2(0, 0), dust3d::Vector2(1, 1), dust3d::Vector2(0, 1) } };
        atlas.triangleUvKnown = { 1, 1 };
        atlas.triangleComponentIds = { owner, owner };
        dust3d::UvMapPacker::Layout layout;
        layout.width = layout.height = 1;
        layout.sourceId = owner;
        layout.color = dust3d::Color(.2, .4, .8);
        for (size_t index = 0; index < 2; ++index)
            layout.indexedGlobalUv[index] = atlas.triangleUvs[index];
        atlas.layouts.push_back(layout);
        options.colorSize = options.normalSize = 32;
        options.materialSize = 8;
    }
    dust3d::TextureBakeResult bake(const dust3d::TextureBakeInputs& inputs = {}) const
    {
        return dust3d::TextureBaker::bake(object, atlas, inputs, options);
    }
    void useLegacy()
    {
        auto& layout = atlas.layouts[0];
        layout.indexedGlobalUv.clear();
        for (size_t index = 0; index < object.triangles.size(); ++index) {
            const auto& face = object.triangles[index];
            layout.globalUv[{ dust3d::PositionKey(object.vertices[face[0]]), dust3d::PositionKey(object.vertices[face[1]]), dust3d::PositionKey(object.vertices[face[2]]) }] = atlas.triangleUvs[index];
        }
    }
};
dust3d::TextureBakeInputs details()
{
    dust3d::TextureBakeInputs inputs;
    auto& surface = inputs.surfaces[owner];
    dust3d::SurfacePattern::Settings pattern;
    pattern.type = dust3d::SurfacePattern::Type::Spots;
    pattern.baseColor = dust3d::Color(1, 1, 1);
    pattern.patternColor = dust3d::Color(0, 0, 0);
    pattern.scale = .1;
    surface.pattern = pattern;
    dust3d::ClothFolds::Fold fold;
    fold.points = { { .5, 0, 0 }, { .5, 1, 0 } };
    fold.normals = { { 0, 0, 1 }, { 0, 0, 1 } };
    fold.width = .2;
    fold.depth = .04;
    surface.folds = { fold };
    surface.wrinkleStrength = 1;
    return inputs;
}
void indexedDetailsMatchOrdinaryTopology()
{
    Fixture indexed;
    Fixture legacy;
    legacy.useLegacy();
    const auto inputs = details();
    const auto result = indexed.bake(inputs);
    const auto expected = legacy.bake(inputs);
    require(result.normal.has_value(), "Indexed chart lost cloth fold normals");
    require(result.color.pixels == expected.color.pixels && result.normal->pixels == expected.normal->pixels,
        "Indexed procedural pattern/fold output changed ordinary geometry behavior");
    require(result.surfaceVertexIndexEntries == 4 && result.rasterTriangleVisits > 0,
        "Indexed chart did not prepare actual topology and raster details");
    const auto original = indexed.atlas.layouts[0].indexedGlobalUv;
    indexed.bake(inputs);
    require(indexed.atlas.layouts[0].indexedGlobalUv == original, "Texture baking mutated caller-owned ordinal corners");
    indexed.options.maximumPayloadBytes = result.payloadBytes - 1;
    bool limited = false;
    try {
        indexed.bake(inputs);
    } catch (const dust3d::TextureBakeError& error) {
        limited = error.code() == dust3d::TextureBakeErrorCode::limitExceeded;
    }
    require(limited, "Indexed normal geometry bypassed the retained-output quota witness");
}
void disconnectedCoincidentVerticesRemainDistinct()
{
    Fixture fixture;
    fixture.object.vertices.push_back(fixture.object.vertices[0]);
    fixture.object.vertices.push_back(fixture.object.vertices[2]);
    fixture.object.triangles[1] = { 4, 5, 3 };
    const auto result = fixture.bake(details());
    require(result.normal && result.surfaceVertexIndexEntries == 6,
        "Indexed surface topology welded distinct coincident geometry vertices");
}
void indexedCoordinatesDoNotNeedPositionKeys()
{
    Fixture fixture;
    for (auto& vertex : fixture.object.vertices)
        vertex *= 1e14;
    const auto result = fixture.bake();
    require(result.color.pixels.size() == fixture.options.colorSize * fixture.options.colorSize * 4,
        "Finite indexed geometry must not depend on unused legacy position-key range");
}
void invalidOrdinalMappingsReject()
{
    for (int variant = 0; variant < 2; ++variant) {
        Fixture fixture;
        if (variant == 0)
            fixture.atlas.layouts[0].indexedGlobalUv[99] = fixture.atlas.triangleUvs[0];
        else
            fixture.atlas.layouts[0].indexedGlobalUv[0][0][0] = std::numeric_limits<double>::quiet_NaN();
        bool invalid = false;
        try {
            fixture.bake();
        } catch (const dust3d::TextureBakeError& error) {
            invalid = error.code() == dust3d::TextureBakeErrorCode::invalidInput && *error.what();
        }
        require(invalid, "Malformed indexed chart mapping was not rejected at admission");
    }
}
}
int main()
{
    try {
        indexedDetailsMatchOrdinaryTopology();
        disconnectedCoincidentVerticesRemainDistinct();
        invalidOrdinalMappingsReject();
        indexedCoordinatesDoNotNeedPositionKeys();
        std::cout << "Indexed texture bake contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
