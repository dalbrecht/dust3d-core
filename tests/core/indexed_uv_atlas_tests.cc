// SPDX-License-Identifier: MIT
#include <algorithm>
#include <cmath>
#include <dust3d/base/object.h>
#include <dust3d/uv/uv_atlas_builder.h>
#include <iostream>
#include <limits>
#include <native/Engine/GenerationPipeline.h>
#include <native/Engine/ImportedModels.h>
#include <native/Engine/ImportedSurface.h>
#include <native/Engine/SourceBakedMesh.h>
#include <stdexcept>

namespace {
void require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}
const dust3d::Uuid first("{aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa}");
const dust3d::Uuid second("{bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb}");
using Uvs = std::array<dust3d::Vector2, 3>;

std::map<dust3d::Uuid, dust3d::UvAtlasComponent> components()
{
    return { { first, { dust3d::Color(1, 0, 0), std::nullopt } },
        { second, { dust3d::Color(0, 0, 1), std::nullopt } } };
}

dust3d::Object coincident()
{
    dust3d::Object object;
    object.vertices = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 },
        { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } };
    object.triangles = { { 0, 1, 2 }, { 3, 4, 5 }, { 0, 1, 2 } };
    object.triangleProceduralUvs = {
        dust3d::ProceduralTriangleUv { first, { dust3d::Vector2(0, 0), dust3d::Vector2(1, 0), dust3d::Vector2(0, 1) } },
        dust3d::ProceduralTriangleUv { second, { dust3d::Vector2(0.2, 0.3), dust3d::Vector2(0.4, 0.5), dust3d::Vector2(0.6, 0.7) } },
        std::nullopt
    };
    // Legacy positional data must not fill a known-absent indexed mapping.
    const std::array<dust3d::PositionKey, 3> key { object.vertices[0], object.vertices[1], object.vertices[2] };
    object.componentTriangleUvs[first][key] = object.triangleProceduralUvs[0]->uv;
    object.triangleComponentIds = { second, first, second };
    return object;
}

void mappedCornerMatches(const dust3d::UvAtlas& atlas, size_t triangle, const dust3d::ProceduralTriangleUv& input)
{
    const dust3d::UvMapPacker::Layout* layout = nullptr;
    for (const auto& candidate : atlas.layouts)
        if (candidate.sourceId == input.componentId)
            layout = &candidate;
    require(layout != nullptr, "Indexed owner must retain its own layout");
    for (size_t corner = 0; corner < 3; ++corner) {
        const double x = layout->flipped ? input.uv[corner].y() : input.uv[corner].x();
        const double y = layout->flipped ? input.uv[corner].x() : input.uv[corner].y();
        const double width = layout->flipped ? layout->height : layout->width;
        const double height = layout->flipped ? layout->width : layout->height;
        require(std::abs(atlas.triangleUvs[triangle][corner].x() - (layout->left + x * width)) < 1e-5,
            "Packed U must follow indexed corner and chart owner");
        require(std::abs(atlas.triangleUvs[triangle][corner].y() - (layout->top + y * height)) < 1e-5,
            "Packed V must follow indexed corner and chart owner");
    }
}

void coincidentOwnership()
{
    auto object = coincident();
    const auto atlas = dust3d::UvAtlasBuilder::build(object, components());
    require(atlas.layouts.size() == 2, "Coincident owners need independent charts");
    require(atlas.triangleUvKnown == std::vector<uint8_t>({ 1, 1, 0 }), "Unknown indexed triangle must not borrow positional UVs");
    require(atlas.triangleComponentIds == std::vector<dust3d::Uuid>({ first, second, dust3d::Uuid {} }),
        "Indexed UV owner is authoritative; unknown ownership must stay unknown");
    mappedCornerMatches(atlas, 0, *object.triangleProceduralUvs[0]);
    mappedCornerMatches(atlas, 1, *object.triangleProceduralUvs[1]);
    object.triangleProceduralUvs[1]->componentId = first;
    const auto sameOwner = dust3d::UvAtlasBuilder::build(object, components());
    require(sameOwner.layouts.size() == 1, "One owner shares one chart while retaining distinct triangle UVs");
    mappedCornerMatches(sameOwner, 0, *object.triangleProceduralUvs[0]);
    mappedCornerMatches(sameOwner, 1, *object.triangleProceduralUvs[1]);
    require(sameOwner.triangleUvs[0][0].x() != sameOwner.triangleUvs[1][0].x(), "Duplicate geometry must retain distinct authored procedural corners");
    auto options = dust3d::UvAtlasOptions {};
    options.maximumPayloadBytes = atlas.payloadBytes;
    require(dust3d::UvAtlasBuilder::build(coincident(), components(), options).payloadBytes == atlas.payloadBytes,
        "Exact indexed payload budget must succeed");
    --options.maximumPayloadBytes;
    try {
        (void)dust3d::UvAtlasBuilder::build(coincident(), components(), options);
        throw std::runtime_error("One-byte-short indexed budget unexpectedly succeeded");
    } catch (const dust3d::UvAtlasError& error) {
        require(error.code() == dust3d::UvAtlasErrorCode::limitExceeded, "Indexed quota rejection needs typed cause");
    }
}

void cornerOrderAndQuantization()
{
    auto object = coincident();
    for (size_t index = 3; index < 6; ++index)
        object.vertices[index][0] += 0.000001;
    object.triangles[0] = { 1, 2, 0 };
    object.triangles[1] = { 5, 4, 3 };
    object.triangleProceduralUvs[0]->uv = { dust3d::Vector2(0.1, 0.2), dust3d::Vector2(0.7, 0.3), dust3d::Vector2(0.4, 0.9) };
    auto inputs = components();
    const dust3d::Uuid image("{cccccccc-cccc-4ccc-8ccc-cccccccccccc}");
    inputs[first].image = dust3d::UvAtlasImage { image, 37, 11 };
    inputs[second].image = dust3d::UvAtlasImage { image, 13, 29 };
    const auto atlas = dust3d::UvAtlasBuilder::build(object, inputs);
    for (size_t index = 0; index < 2; ++index) {
        const auto& record = *object.triangleProceduralUvs[index];
        const dust3d::UvMapPacker::Layout* layout = nullptr;
        for (const auto& candidate : atlas.layouts)
            if (candidate.sourceId == record.componentId)
                layout = &candidate;
        require(layout != nullptr, "Near-coincident chart owner is retained");
        const auto& dimensions = *inputs.at(record.componentId).image;
        for (size_t corner = 0; corner < 3; ++corner) {
            const double u = layout->flipped ? record.uv[corner].y() : record.uv[corner].x();
            const double v = layout->flipped ? record.uv[corner].x() : record.uv[corner].y();
            const double width = layout->flipped ? dimensions.height : dimensions.width;
            const double height = layout->flipped ? dimensions.width : dimensions.height;
            require(std::abs(atlas.triangleUvs[index][corner].x() - (layout->left + u * width / atlas.packedTextureSize)) < 1e-8,
                "Cyclic/reversed/quantization-colliding U follows actual indexed corner");
            require(std::abs(atlas.triangleUvs[index][corner].y() - (layout->top + v * height / atlas.packedTextureSize)) < 1e-8,
                "Cyclic/reversed/quantization-colliding V follows actual indexed corner");
        }
    }
}

void retainedSourceBakeOrdinals()
{
    using namespace dust3d;
    using namespace dust3d::native;
    auto object = coincident();
    object.triangles = { { 0, 0, 0 }, { 0, 1, 2 }, { 3, 4, 5 } };
    const Uvs local { Vector2(0, 0), Vector2(1, 0), Vector2(0, 1) };
    object.triangleProceduralUvs = { ProceduralTriangleUv { first, local },
        ProceduralTriangleUv { first, local }, ProceduralTriangleUv { second, local } };
    object.triangleComponentIds = { first, first, second };
    object.triangleComponentKeys = { to_string(first), to_string(first), to_string(second) };
    Snapshot snapshot;
    snapshot.components[to_string(first)]["color"] = "#ff0000";
    snapshot.components[to_string(second)]["color"] = "#0000ff";
    ImportedModels imports;
    ImportedSurfaceData sources(snapshot, imports, 1024 * 1024);
    Dust3DSourceBakeOptions options {};
    options.struct_size = sizeof(options);
    options.legacy.struct_size = sizeof(options.legacy);
    options.legacy.atlas_texture_size = 64;
    options.legacy.color_size = options.legacy.material_size = options.legacy.normal_size = 64;
    SourceBakedData baked(options, imports);
    GenerationOutputs outputs;
    outputs.surfaces = &sources;
    outputs.sourceBaked = &baked;
    Dust3DMesh geometry;
    prepareGeneratedObject(object, &snapshot, geometry, outputs);
    require(baked.triangles.size() == 2 && geometry.vertices.size() == 6,
        "Filtered source bake must retain original triangle ordinals after a dropped prefix");
    require(baked.triangles[0].uv_known && baked.triangles[1].uv_known,
        "Both coincident indexed procedural owners must survive source bake filtering");
    require(baked.triangles[0].component_index != baked.triangles[1].component_index,
        "Coincident procedural source bake must retain distinct source identities");
    require(baked.maps[DUST3D_TEXTURE_COLOR].has_value(), "Indexed source bake needs owned color output");
    const auto& image = *baked.maps[DUST3D_TEXTURE_COLOR];
    for (size_t index = 0; index < 2; ++index) {
        const auto& triangle = baked.triangles[index];
        double u = 0, v = 0;
        for (size_t corner = 0; corner < 3; ++corner) {
            u += triangle.uv[corner][0] / 3.0;
            v += triangle.uv[corner][1] / 3.0;
        }
        const size_t x = std::min(image.width - 1, size_t(u * image.width));
        const size_t y = std::min(image.height - 1, size_t(v * image.height));
        const auto* pixel = &image.pixels[(y * image.width + x) * 4];
        require(index == 0 ? pixel[0] > 240 && pixel[2] < 15 : pixel[2] > 240 && pixel[0] < 15,
            "Source bake must sample each indexed owner's chart rather than a coincident positional chart");
    }
}

void explicitGeneratedGradient()
{
    auto object = coincident();
    const auto source = object.triangleProceduralUvs[0]->uv;
    dust3d::ProceduralTriangleUv generated;
    generated.uv = source;
    generated.gradient = dust3d::ProceduralUvGradient { second, first, second };
    object.triangleProceduralUvs[2] = generated;
    const auto atlas = dust3d::UvAtlasBuilder::build(object, components());
    require(atlas.layouts.size() == 3 && atlas.gradients.size() == 1,
        "An explicitly indexed generated seam needs its own gradient chart");
    require(atlas.triangleUvKnown[2] == 1 && atlas.triangleComponentIds[2].isNull(),
        "Generated gradient is mapped without inventing single-component source ownership");
    const auto& gradient = atlas.gradients[0];
    require(gradient.layoutIndex == 0 && atlas.layouts[0].sourceId.isNull(),
        "Indexed seams must precede ordinary charts as in the legacy atlas");
    require(atlas.layouts[0].indexedGlobalUv.at(2) == atlas.triangleUvs[2],
        "The indexed seam corners must refer to the generated gradient layout");
    mappedCornerMatches(atlas, 0, *object.triangleProceduralUvs[0]);
    mappedCornerMatches(atlas, 1, *object.triangleProceduralUvs[1]);
    require(gradient.largeColor.r() == 1 && gradient.largeColor.b() == 0
            && gradient.smallColor.r() == 0 && gradient.smallColor.b() == 1,
        "Explicit topology-derived endpoint owners determine gradient colors");
}

void gradientOrderIgnoresOpaqueIdentity()
{
    auto object = coincident();
    for (size_t index = 0; index < 2; ++index) {
        object.triangleProceduralUvs[index]->componentId = {};
        object.triangleProceduralUvs[index]->gradient = dust3d::ProceduralUvGradient { index == 0 ? first : second,
            index == 0 ? first : second, index == 0 ? second : first };
    }
    const auto original = dust3d::UvAtlasBuilder::build(object, components());
    std::swap(object.triangleProceduralUvs[0]->gradient->chartId, object.triangleProceduralUvs[1]->gradient->chartId);
    const auto rebuilt = dust3d::UvAtlasBuilder::build(object, components());
    require(original.triangleUvs == rebuilt.triangleUvs,
        "Opaque seam identity must not reorder charts across equivalent cold generations");
    require(original.gradients[0].largeColor.r() == rebuilt.gradients[0].largeColor.r(),
        "Seam gradient insertion order must follow actual triangle provenance");
}

void gradientChartsFollowSeamGenerationOrder()
{
    auto object = coincident();
    const auto local = object.triangleProceduralUvs[0]->uv;
    object.triangleProceduralUvs[0] = dust3d::ProceduralTriangleUv {
        {}, local, dust3d::ProceduralUvGradient { first, first, second }
    };
    object.triangleProceduralUvs[1] = dust3d::ProceduralTriangleUv {
        {}, local, dust3d::ProceduralUvGradient { second, second, first }
    };
    const dust3d::Uuid removed("{cccccccc-cccc-4ccc-8ccc-cccccccccccc}");
    object.proceduralGradientOrder = { removed, second, second, first };
    const auto atlas = dust3d::UvAtlasBuilder::build(object, components());
    require(atlas.gradients.size() == 2, "Removed or duplicate seams must not add charts");
    require(atlas.layouts[0].indexedGlobalUv.count(1) == 1
            && atlas.layouts[1].indexedGlobalUv.count(0) == 1,
        "Seam chart packing must follow generation order even when final faces are reordered");
    require(atlas.gradients[0].largeColor.b() == 1 && atlas.gradients[1].largeColor.r() == 1,
        "Reordering charts must keep each indexed seam's endpoint colors");
    dust3d::Object copy;
    copy.triangles = object.triangles;
    copy.copyUvFrom(object);
    require(copy.proceduralGradientOrder == object.proceduralGradientOrder,
        "Compatible UV copies retain indexed seam ordering");
    copy.triangles.pop_back();
    copy.copyUvFrom(object);
    require(copy.proceduralGradientOrder.empty(), "An incompatible UV copy clears stale seam ordering");

    dust3d::Snapshot snapshot;
    dust3d::native::ImportedModels imports;
    dust3d::native::ImportedSurfaceData sources(snapshot, imports, 1024 * 1024);
    sources.prepare(object, "");
    sources.appendTriangle(1);
    dust3d::Object subset;
    std::vector<size_t> retained;
    dust3d::native::populateProceduralSourceSubset(object, { 1 }, sources, subset, retained, nullptr);
    const auto filtered = dust3d::UvAtlasBuilder::build(subset, components());
    require(subset.proceduralGradientOrder == object.proceduralGradientOrder
            && filtered.gradients.size() == 1 && filtered.gradients[0].largeColor.b() == 1,
        "Source bake retains seam order and omits a clipped chart without losing its neighbor's colors");
}

void componentChartsFollowSourceOrder()
{
    auto object = coincident();
    const dust3d::Uuid removed("{cccccccc-cccc-4ccc-8ccc-cccccccccccc}");
    object.proceduralComponentOrder = { removed, second, second, first };
    const auto atlas = dust3d::UvAtlasBuilder::build(object, components());
    require(atlas.layouts.size() == 2, "Removed and duplicate owners must not add charts");
    require(atlas.layouts[0].sourceId == second && atlas.layouts[1].sourceId == first,
        "Component packing must retain the source chart submission order");
    require(atlas.layouts[0].indexedGlobalUv.at(1) == atlas.triangleUvs[1]
            && atlas.layouts[1].indexedGlobalUv.at(0) == atlas.triangleUvs[0],
        "Changing chart order must retain indexed triangle ownership for coincident faces");

    dust3d::Object copy;
    copy.triangles = object.triangles;
    copy.copyUvFrom(object);
    require(copy.proceduralComponentOrder == object.proceduralComponentOrder,
        "Compatible UV copies retain component packing order");

    dust3d::Snapshot snapshot;
    dust3d::native::ImportedModels imports;
    dust3d::native::ImportedSurfaceData sources(snapshot, imports, 1024 * 1024);
    sources.prepare(object, "");
    sources.appendTriangle(0);
    sources.appendTriangle(1);
    dust3d::Object subset;
    std::vector<size_t> retained;
    dust3d::native::populateProceduralSourceSubset(object, { 0, 1 }, sources, subset, retained, nullptr);
    const auto filtered = dust3d::UvAtlasBuilder::build(subset, components());
    require(filtered.layouts[0].sourceId == second && filtered.layouts[1].sourceId == first,
        "Source bake subsets retain component packing order");
}

void proceduralSubsetRetainsTopology()
{
    using namespace dust3d;
    using namespace dust3d::native;
    for (const bool disconnected : { false, true }) {
        Object original;
        original.vertices = { { 0, 0, 0 }, { 1, 0, 0 }, { 1, 1, 0 }, { 0, 1, 0 }, { 0, 0, 0 } };
        original.triangles = { { 0, 1, 2 }, { disconnected ? size_t(4) : size_t(0), 2, 3 } };
        original.triangleComponentKeys.assign(2, to_string(first));
        const Uvs local { Vector2(0, 0), Vector2(1, 0), Vector2(1, 1) };
        original.triangleProceduralUvs.assign(2, ProceduralTriangleUv { first, local });
        Snapshot snapshot;
        snapshot.components[to_string(first)]["color"] = "#ff0000";
        ImportedModels imports;
        ImportedSurfaceData sources(snapshot, imports, 1024 * 1024);
        sources.prepare(original, "");
        sources.appendTriangle(0);
        sources.appendTriangle(1);
        Object subset;
        std::vector<size_t> indices;
        populateProceduralSourceSubset(original, { 0, 1 }, sources, subset, indices, nullptr);
        require(subset.vertices.size() == (disconnected ? 5 : 4),
            "Procedural subset must preserve source vertex sharing without welding coincident IDs");
        require(subset.triangles[0][2] == subset.triangles[1][1],
            "Shared source vertex must remain shared after procedural filtering");
        require((subset.triangles[0][0] != subset.triangles[1][0]) == disconnected,
            "Disconnected coincident source vertices must remain distinct after filtering");
        require(indices == std::vector<size_t>({ 0, 1 }) && subset.triangleProceduralUvs.size() == 2,
            "Procedural subset must preserve retained triangle/corner association");
    }
}

void invalidInputs()
{
    for (int variant = 0; variant < 4; ++variant) {
        auto object = coincident();
        if (variant == 0)
            object.triangleProceduralUvs.pop_back();
        else if (variant == 1)
            object.triangleProceduralUvs[0]->uv[0][0] = std::numeric_limits<double>::quiet_NaN();
        else if (variant == 2)
            object.triangleProceduralUvs[0]->uv[0][1] = 1.1;
        else
            object.triangleProceduralUvs[0]->componentId = {};
        try {
            (void)dust3d::UvAtlasBuilder::build(object, components());
            throw std::runtime_error("Invalid indexed record unexpectedly succeeded");
        } catch (const dust3d::UvAtlasError& error) {
            require(error.code() == dust3d::UvAtlasErrorCode::invalidInput, "Invalid indexed record needs typed cause");
        }
    }
}
}
int main()
{
    try {
        coincidentOwnership();
        invalidInputs();
        cornerOrderAndQuantization();
        retainedSourceBakeOrdinals();
        explicitGeneratedGradient();
        gradientOrderIgnoresOpaqueIdentity();
        gradientChartsFollowSeamGenerationOrder();
        componentChartsFollowSourceOrder();
        proceduralSubsetRetainsTopology();
        std::cout << "Indexed procedural atlas contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
