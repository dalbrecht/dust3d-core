// SPDX-License-Identifier: MIT
#include <cmath>
#include <iostream>
#include <native/Engine/GenerationPipeline.h>
#include <native/Engine/ImportedModels.h>
#include <native/Engine/SourceBakedMesh.h>
#include <stdexcept>

namespace {
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
using TriangleKey = std::array<dust3d::PositionKey, 3>;
TriangleKey key(const dust3d::Object& object, size_t index)
{
    const auto& face = object.triangles[index];
    return { dust3d::PositionKey(object.vertices[face[0]]), dust3d::PositionKey(object.vertices[face[1]]), dust3d::PositionKey(object.vertices[face[2]]) };
}
void verifyProceduralSeamsAndNormalComposition()
{
    const std::string redId = "{cccccccc-cccc-4ccc-8ccc-cccccccccccc}";
    const std::string blueId = "{dddddddd-dddd-4ddd-8ddd-dddddddddddd}";
    const std::string redPart = "{aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa}";
    const std::string bluePart = "{bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb}";
    const dust3d::Uuid red(redId), blue(blueId);
    dust3d::Object object;
    // Two colored patches, a two-triangle seam strip, and a valid cap whose
    // inherited UV metadata is the explicit broken-chart solid-color fallback.
    object.vertices = { { 0, 0, 0 }, { 1, 0, 0 }, { 1, 1, 0 }, { 2, 0, 0 }, { 2, 1, 0 }, { 3, 0, 0 }, { 0, 1, 0 }, { 1, 2, 0 }, { 0, 2, 0 } };
    object.triangles = { { 0, 2, 1 }, { 3, 4, 5 }, { 1, 2, 3 }, { 4, 3, 2 }, { 6, 8, 7 } };
    object.triangleComponentIds = { red, blue, red, blue, red };
    object.triangleComponentKeys = { redId, blueId, redId, blueId, redId };
    object.componentTriangleUvs[red][key(object, 0)] = { dust3d::Vector2(0, 0), dust3d::Vector2(1, 1), dust3d::Vector2(1, 0) };
    object.componentTriangleUvs[blue][key(object, 1)] = { dust3d::Vector2(0, 0), dust3d::Vector2(0, 1), dust3d::Vector2(1, 0) };
    object.seamTriangleUvs.push_back({ { key(object, 2) }, { key(object, 3) } });
    object.brokenTrianglesToComponentIdMap[key(object, 4)] = red;
    dust3d::Snapshot snapshot;
    snapshot.parts[redPart] = { { "metallic", ".5" }, { "roughness", ".25" }, { "emissive", "1" } };
    snapshot.parts[bluePart] = { { "metallic", ".75" }, { "roughness", ".75" }, { "emissive", ".5" } };
    snapshot.components[redId] = { { "linkDataType", "partId" }, { "linkData", redPart }, { "color", "#ff0000" }, { "wrap", "true" }, { "wrapWrinkles", "1" } };
    snapshot.components[blueId] = { { "linkDataType", "partId" }, { "linkData", bluePart }, { "color", "#0000ff" } };
    dust3d::ClothFolds::Fold fold;
    fold.points = { { .5, 0, 0 }, { .5, 1, 0 } };
    fold.normals = { { 0, 0, -1 }, { 0, 0, -1 } };
    fold.width = .2;
    fold.depth = .04;
    snapshot.components[redId]["__wrapFolds"] = dust3d::ClothFolds::serialize({ fold });
    Dust3DSourceBakeOptions options {};
    options.struct_size = sizeof(options);
    options.legacy.struct_size = sizeof(options.legacy);
    options.legacy.color_size = options.legacy.normal_size = options.legacy.material_size = options.legacy.atlas_texture_size = 128;
    dust3d::native::ImportedModels imports;
    dust3d::native::ImportedSurfaceData sources(snapshot, imports, 256 * 1024 * 1024);
    Dust3DMesh geometry;
    for (const auto& face : object.triangles)
        for (const auto index : face) {
            Dust3DVertex vertex {};
            for (size_t axis = 0; axis < 3; ++axis)
                vertex.position[axis] = static_cast<float>(object.vertices[index][axis]);
            vertex.normal[2] = -1;
            geometry.vertices.push_back(vertex);
        }
    sources.prepare(object, geometry.warning);
    dust3d::native::SourceBakedData data(options, imports);
    data.validateVisuals(snapshot);
    for (size_t index = 0; index < object.triangles.size(); ++index) {
        sources.appendTriangle(index);
        data.appendTriangle(index);
    }
    sources.finish(geometry);
    data.prepare(object, snapshot, geometry, sources);
    require(data.triangles.size() == 5 && data.maps[0] && data.maps[1] && data.maps[2] && data.maps[3] && data.maps[4], "All procedural maps must survive native composition");
    auto sample = [&](size_t triangle, size_t map) {
        double u = 0, v = 0;
        for (size_t corner = 0; corner < 3; ++corner) {
            u += data.triangles[triangle].uv[corner][0] / 3;
            v += data.triangles[triangle].uv[corner][1] / 3;
        }
        const auto& image = *data.maps[map];
        const size_t x = std::min(image.width - 1, static_cast<size_t>(u * image.width));
        const size_t y = std::min(image.height - 1, static_cast<size_t>(v * image.height));
        const auto offset = (y * image.width + x) * 4;
        return std::array<uint8_t, 4> { image.pixels[offset], image.pixels[offset + 1], image.pixels[offset + 2], image.pixels[offset + 3] };
    };
    const auto redPixel = sample(0, 0), bluePixel = sample(1, 0), seamPixel = sample(2, 0), capPixel = sample(4, 0);
    require(redPixel[0] == 255 && redPixel[1] == 0 && redPixel[2] == 0 && bluePixel[0] == 0 && bluePixel[2] == 255, "Procedural patch colors preserve component ownership");
    require(seamPixel[0] > 0 && seamPixel[2] > 0 && seamPixel[1] == 0, "Inherited seam gradient must survive procedural subset filtering");
    require(capPixel[0] == 255 && capPixel[2] == 0 && data.triangles[4].uv_known && !data.triangles[4].tangent.known, "Broken-chart cap retains solid color with explicitly unknown tangent");
    require(sample(0, 2)[0] == 128 && sample(0, 3)[0] == 64 && sample(0, 4)[0] == 255, "Legacy metallic, roughness and emissive maps must compose without reinterpretation");
    bool hasFoldNormal = false;
    for (size_t offset = 0; offset < data.maps[1]->pixels.size(); offset += 4)
        hasFoldNormal = hasFoldNormal || data.maps[1]->pixels[offset] != 128 || data.maps[1]->pixels[offset + 1] != 128;
    require(hasFoldNormal, "Real fold normals must survive normal-map composition");
    require(data.maximumSamplingLod == 0 && data.payloadBytes > 0, "Composition preserves bounded ownership and output LOD policy");
}
}
int main()
{
    try {
        std::lock_guard<std::mutex> lock(dust3d::native::engineMutex());
        verifyProceduralSeamsAndNormalComposition();
        std::cout << "Native source-bake composition contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
