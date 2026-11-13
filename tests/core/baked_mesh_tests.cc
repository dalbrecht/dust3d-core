// SPDX-License-Identifier: MIT
#include "precision_fixture.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <native/Engine/BakedMesh.h>
#include <native/Engine/Cancellation.h>
#include <native/Engine/GenerationPipeline.h>
#include <sstream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
void nullBakedOwners()
{
    require(!dust3d_baked_mesh_uvs(nullptr) && !dust3d_baked_mesh_tangents(nullptr), "Null baked owners must expose no borrowed storage");
    require(dust3d_baked_mesh_tangent_count(nullptr) == 0 && dust3d_baked_mesh_payload_bytes(nullptr) == 0, "Null baked owner counts must be zero");
    const auto view = dust3d_baked_mesh_texture(nullptr, DUST3D_TEXTURE_COLOR);
    require(!view.pixels && view.width == 0 && view.height == 0 && view.row_stride == 0 && view.byte_count == 0, "Null baked texture views must be fully zero initialized");
    dust3d_baked_mesh_destroy(nullptr);
    Dust3DError error {};
    require(!dust3d_baked_mesh_load(nullptr, 0, nullptr, &error) && error.status == DUST3D_STATUS_INVALID_DOCUMENT && *error.message, "Missing bake options must fail with a typed cause and no owner");
}
void invalidBakeInputs()
{
    Dust3DError error {};
    const size_t shortOptions = sizeof(size_t);
    require(!dust3d_baked_mesh_load(nullptr, 0, reinterpret_cast<const Dust3DBakeOptions*>(&shortOptions), &error) && error.status == DUST3D_STATUS_INVALID_DOCUMENT, "Sized options must reject short foreign buffers before reading their absent fields");
    Dust3DBakeOptions options {};
    auto rejected = [&](Dust3DStatus status, const char* expected) {
        require(!dust3d_baked_mesh_load(nullptr, 0, &options, &error) && error.status == status && std::strstr(error.message, expected), "Invalid bake input must preserve its specific boundary cause");
    };
    rejected(DUST3D_STATUS_INVALID_DOCUMENT, "sized options");
    options.struct_size = sizeof(options);
    options.maximum_bake_payload_bytes = size_t(268435457);
    rejected(DUST3D_STATUS_LIMIT_EXCEEDED, "budgets");
    options.maximum_bake_payload_bytes = 0;
    options.color_size = 8193;
    rejected(DUST3D_STATUS_INVALID_DOCUMENT, "dimensions");
    options.color_size = 0;
    options.maximum_spatial_grid_entries = 16000001;
    rejected(DUST3D_STATUS_INVALID_DOCUMENT, "spatial grid");
    options.maximum_spatial_grid_entries = 0;
    options.image_count = 1;
    rejected(DUST3D_STATUS_INVALID_DOCUMENT, "descriptor buffer");
    Dust3DBakeImage image {};
    options.images = &image;
    std::memset(image.image_id, 'x', sizeof(image.image_id));
    rejected(DUST3D_STATUS_INVALID_DOCUMENT, "terminated");
    std::strcpy(image.image_id, "{eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee}");
    uint8_t pixel[4] {};
    image.pixels = pixel;
    image.width = image.height = 1;
    image.row_stride = image.byte_count = 4;
    image.row_stride = 3;
    rejected(DUST3D_STATUS_INVALID_DOCUMENT, "stride/span");
    image.row_stride = std::numeric_limits<size_t>::max();
    image.height = 2;
    rejected(DUST3D_STATUS_INVALID_DOCUMENT, "stride/span");
    image.height = 0;
    rejected(DUST3D_STATUS_INVALID_DOCUMENT, "dimensions");
    image.height = 1;
    image.row_stride = image.byte_count = 4;
    Dust3DBakeImage duplicate[] = { image, image };
    options.images = duplicate;
    options.image_count = 2;
    rejected(DUST3D_STATUS_INVALID_DOCUMENT, "unique");
    options.images = nullptr;
    options.image_count = 0;
    auto* token = dust3d_cancellation_token_create(&error);
    require(token != nullptr, "Cancelled load contract requires an owning token");
    dust3d_cancellation_token_cancel(token);
    options.cancellation = token;
    rejected(DUST3D_STATUS_CANCELLED, "cancelled");
    dust3d_cancellation_token_destroy(token);
}
void visualResolutionBoundaries()
{
    Dust3DBakeOptions options {};
    options.struct_size = sizeof(options);
    options.color_size = options.normal_size = options.material_size = 8;
    dust3d::native::BakedMeshData data(options);
    dust3d::Snapshot snapshot;
    const std::string identity = "{cccccccc-cccc-4ccc-8ccc-cccccccccccc}";
    auto& component = snapshot.components[identity];
    component["color"] = "#ffffff";
    component["wrap"] = "true";
    component["wrapPattern"] = "FuturePattern";
    bool rejected = false;
    try {
        data.validateVisuals(snapshot);
    } catch (const dust3d::native::DocumentError& cause) {
        rejected = cause.status() == DUST3D_STATUS_UNSUPPORTED_ASSET && std::strstr(cause.what(), "pattern");
    }
    require(rejected, "Unknown active patterns must fail explicitly instead of silently baking plain color");
    component["wrapPattern"] = "None";
    snapshot.components[identity.substr(1, 36)] = component;
    snapshot.components[identity.substr(1, 36)]["name"] = "Different editor label";
    snapshot.components[identity.substr(1, 36)]["dirty"] = "true";
    data.validateVisuals(snapshot);
    snapshot.components["not-an-engine-identity"] = { { "color", "red" } };
    data.validateVisuals(snapshot);
    snapshot.components[identity.substr(1, 36)]["wrapWrinkles"] = ".5";
    rejected = false;
    try {
        data.validateVisuals(snapshot);
    } catch (const dust3d::native::DocumentError& cause) {
        rejected = cause.status() == DUST3D_STATUS_UNSUPPORTED_ASSET;
    }
    require(rejected, "Aliased identities with conflicting visual definitions must remain explicit failures");
}
struct AnalyticBake {
    const dust3d::Uuid component { "{cccccccc-cccc-4ccc-8ccc-cccccccccccc}" };
    const dust3d::Uuid image { "{eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee}" };
    dust3d::Snapshot snapshot;
    dust3d::Object object;
    dust3d::UvAtlas atlas;
    Dust3DBakeOptions options {};
    AnalyticBake()
    {
        options.struct_size = sizeof(options);
        options.color_size = options.normal_size = options.material_size = 8;
        snapshot.components[component.toString()]["color"] = "#ffffff";
        snapshot.components[component.toString()]["linkDataType"] = "partId";
        snapshot.components[component.toString()]["linkData"] = "part";
        snapshot.parts["part"] = { { "metallic", ".5" }, { "roughness", ".25" }, { "emissive", "2" } };
        object.vertices = { { 0, 0, 1 }, { 0, 1, 1 }, { 1, 0, 1 }, { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } };
        object.triangles = { { 0, 1, 2 }, { 3, 4, 5 } };
        atlas.triangleUvs = { { dust3d::Vector2(0, 0), dust3d::Vector2(1, 0), dust3d::Vector2(0, 1) },
            { dust3d::Vector2(0, 0), dust3d::Vector2(1, 0), dust3d::Vector2(0, 1) } };
        atlas.triangleUvKnown = { 0, 1 };
        atlas.triangleComponentIds = { component, component };
        dust3d::UvMapPacker::Layout layout;
        layout.sourceId = component;
        layout.width = layout.height = 1;
        layout.color = dust3d::Color(1, 1, 1);
        const auto& triangle = object.triangles[1];
        layout.globalUv[{ dust3d::PositionKey(object.vertices[triangle[0]]), dust3d::PositionKey(object.vertices[triangle[1]]), dust3d::PositionKey(object.vertices[triangle[2]]) }] = atlas.triangleUvs[1];
        atlas.layouts.push_back(layout);
    }
};
void archivedImageEncodingSelectsTheRaster()
{
    AnalyticBake fixture;
    // This extent separates the fixed RGB32 and transformed vertical samples
    // without building multiple production-size atlases under sanitizers.
    fixture.options.color_size = 384;
    auto& layout = fixture.atlas.layouts[0];
    layout.left = layout.top = 20.25 / 384;
    layout.width = layout.height = 317.25 / 384;
    layout.id = fixture.image;
    std::vector<uint8_t> pixels(8 * 8 * 4, 255);
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            pixels[(y * 8 + x) * 4] = static_cast<uint8_t>(x);
            pixels[(y * 8 + x) * 4 + 1] = static_cast<uint8_t>(y);
        }
    }
    Dust3DBakeImage source {};
    std::strcpy(source.image_id, fixture.image.toString().c_str());
    source.width = source.height = 8;
    source.row_stride = 32;
    source.byte_count = pixels.size();
    source.pixels = pixels.data();
    fixture.options.images = &source;
    fixture.options.image_count = 1;
    const std::vector<uint8_t> rgb { 137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68, 82, 0, 0, 0, 8, 0, 0, 0, 8, 8, 2, 0, 0, 0, 75, 109, 41, 220, 0, 0, 0, 12, 73, 68, 65, 84, 120, 156, 99, 96, 24, 30, 0, 0, 0, 200, 0, 1, 173, 64, 118, 34, 0, 0, 0, 0, 73, 69, 78, 68, 174, 66, 96, 130 };
    const std::vector<uint8_t> rgba { 137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68, 82, 0, 0, 0, 8, 0, 0, 0, 8, 8, 6, 0, 0, 0, 196, 15, 190, 139, 0, 0, 0, 17, 73, 68, 65, 84, 120, 156, 99, 96, 96, 96, 248, 79, 0, 143, 4, 5, 0, 130, 72, 63, 193, 232, 155, 203, 121, 0, 0, 0, 0, 73, 69, 78, 68, 174, 66, 96, 130 };
    const std::vector<uint8_t> keyed { 137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68, 82, 0, 0, 0, 8, 0, 0, 0, 8, 8, 2, 0, 0, 0, 75, 109, 41, 220, 0, 0, 0, 6, 116, 82, 78, 83, 0, 1, 0, 0, 0, 0, 83, 198, 46, 33, 0, 0, 0, 12, 73, 68, 65, 84, 120, 156, 99, 96, 24, 30, 0, 0, 0, 200, 0, 1, 173, 64, 118, 34, 0, 0, 0, 0, 73, 69, 78, 68, 174, 66, 96, 130 };
    const std::vector<uint8_t> indexed { 137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68, 82, 0, 0, 0, 8, 0, 0, 0, 8, 8, 3, 0, 0, 0, 243, 209, 78, 185, 0, 0, 0, 6, 80, 76, 84, 69, 0, 0, 0, 255, 255, 255, 165, 217, 159, 221, 0, 0, 0, 12, 73, 68, 65, 84, 120, 156, 99, 96, 160, 14, 0, 0, 0, 72, 0, 1, 46, 184, 60, 126, 0, 0, 0, 0, 73, 69, 78, 68, 174, 66, 96, 130 };
    const std::vector<uint8_t> indexedAlpha { 137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68, 82, 0, 0, 0, 8, 0, 0, 0, 8, 8, 3, 0, 0, 0, 243, 209, 78, 185, 0, 0, 0, 6, 80, 76, 84, 69, 0, 0, 0, 255, 255, 255, 165, 217, 159, 221, 0, 0, 0, 2, 116, 82, 78, 83, 0, 255, 91, 145, 34, 181, 0, 0, 0, 12, 73, 68, 65, 84, 120, 156, 99, 96, 160, 14, 0, 0, 0, 72, 0, 1, 46, 184, 60, 126, 0, 0, 0, 0, 73, 69, 78, 68, 174, 66, 96, 130 };
    const std::vector<uint8_t> grayKey { 137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68, 82, 0, 0, 0, 8, 0, 0, 0, 8, 8, 0, 0, 0, 0, 225, 100, 225, 87, 0, 0, 0, 2, 116, 82, 78, 83, 0, 0, 118, 147, 205, 56, 0, 0, 0, 12, 73, 68, 65, 84, 120, 156, 99, 96, 160, 14, 0, 0, 0, 72, 0, 1, 46, 184, 60, 126, 0, 0, 0, 0, 73, 69, 78, 68, 174, 66, 96, 130 };
    const std::vector<uint8_t> grayAlpha { 137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68, 82, 0, 0, 0, 8, 0, 0, 0, 8, 8, 4, 0, 0, 0, 110, 6, 118, 0, 0, 0, 0, 15, 73, 68, 65, 84, 120, 156, 99, 96, 248, 143, 6, 7, 70, 0, 0, 208, 152, 63, 193, 184, 135, 215, 6, 0, 0, 0, 0, 73, 69, 78, 68, 174, 66, 96, 130 };
    const std::vector<uint8_t> gray { 137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68, 82, 0, 0, 0, 8, 0, 0, 0, 8, 8, 0, 0, 0, 0, 225, 100, 225, 87, 0, 0, 0, 12, 73, 68, 65, 84, 120, 156, 99, 96, 160, 14, 0, 0, 0, 72, 0, 1, 46, 184, 60, 126, 0, 0, 0, 0, 73, 69, 78, 68, 174, 66, 96, 130 };
    struct Samples {
        uint8_t indexedTransition;
        std::array<uint8_t, 2> scale;
        std::array<uint8_t, 4> overlap;
        std::array<uint8_t, 4> clear;
    };
    const auto sample = [&](const std::vector<uint8_t>* image, bool alias = false) {
        dust3d::native::BakedMeshData bake(fixture.options);
        if (image) {
            const auto key = "images/" + fixture.image.toString() + ".png";
            bake.item("asset", key, image->data(), image->size());
            if (alias)
                bake.item("asset", "images/eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee.png", image->data(), image->size());
        }
        bake.appendTriangle(1);
        bake.prepare(fixture.object, fixture.snapshot, fixture.atlas);
        const auto& output = bake.maps[0]->pixels;
        Samples result {};
        result.indexedTransition = output[(100 * 384 + 99) * 4];
        result.scale = { output[(100 * 384 + 297) * 4], output[(297 * 384 + 100) * 4 + 1] };
        std::copy_n(output.data() + (35 * 384 + 58) * 4, 4, result.overlap.begin());
        std::copy_n(output.data() + (35 * 384 + 35) * 4, 4, result.clear.begin());
        return result;
    };
    require(sample(&rgb).scale == std::array<uint8_t, 2> { 6, 6 }, "RGB32 must advance fixed-point sampling on both axes");
    for (const auto* image : { &rgba, &keyed, static_cast<const std::vector<uint8_t>*>(nullptr) })
        require(sample(image).scale == std::array<uint8_t, 2> { 6, 7 }, "Alpha-capable and caller-only images must use the transformed raster");
    require(sample(&rgb, true).scale == std::array<uint8_t, 2> { 6, 7 }, "Ambiguous image keys must not select RGB32");
    require(sample(&indexed).indexedTransition == 1, "Indexed images must keep Qt's 12-bit scale step");
    require(sample(&gray).indexedTransition == 2, "Opaque grayscale must keep the transformed raster");
    pixels[3] = 0;
    for (const auto* image : { &indexedAlpha, &grayKey, &grayAlpha }) {
        const auto result = sample(image);
        require(result.overlap == std::array<uint8_t, 4> { 1, 0, 255, 255 }, "Alpha-capable indexed and grayscale PNGs must reveal the padded layer");
        require(result.clear == std::array<uint8_t, 4> {}, "Fully transparent PNG samples must clear hidden RGB");
    }
    const auto transparent = sample(&keyed);
    require(transparent.overlap == std::array<uint8_t, 4> { 1, 0, 255, 255 }, "The keyed inset must reveal the padded layer");
    require(transparent.clear == std::array<uint8_t, 4> {}, "Fully transparent chart output must clear hidden RGB");
    require(sample(nullptr).clear == std::array<uint8_t, 4> { 0, 0, 255, 0 }, "Caller-only straight-alpha samples must retain hidden RGB");

    const std::array<uint8_t, 8> partial { 20, 15, 25, 127, 49, 15, 106, 91 };
    std::copy(partial.begin(), partial.end(), pixels.begin());
    require(sample(nullptr).overlap == std::array<uint8_t, 4> { 20, 15, 25, 127 }, "Caller-provided straight-alpha pixels must be preserved");
    require(sample(&rgba).overlap == std::array<uint8_t, 4> { 28, 15, 46, 173 }, "The inset and padding must use Qt's premultiplied rounding");
}
void analyticMaterialsImagesAndTangents()
{
    AnalyticBake fixture;
    std::vector<uint8_t> pixels { 255, 0, 0, 128, 0, 255, 0, 64, 99, 99, 99, 99, 0, 0, 255, 255, 255, 255, 0, 32 };
    Dust3DBakeImage source {};
    std::strcpy(source.image_id, fixture.image.toString().c_str());
    source.width = source.height = 2;
    source.row_stride = 12;
    source.byte_count = pixels.size();
    source.pixels = pixels.data();
    fixture.options.images = &source;
    fixture.options.image_count = 1;
    fixture.snapshot.components[fixture.component.toString()]["colorImageId"] = fixture.image.toString();
    fixture.atlas.layouts[0].id = fixture.image;
    dust3d::native::BakedMeshData data(fixture.options);
    data.validateVisuals(fixture.snapshot);
    data.appendTriangle(1);
    data.prepare(fixture.object, fixture.snapshot, fixture.atlas);
    require(data.tangents.size() == 1 && data.tangents[0].known == 1 && data.tangents[0].direction[0] > .99 && std::abs(data.tangents[0].direction[1]) < 1e-6, "Retained tangent records must follow original triangle ordinals rather than a dropped prefix");
    require(data.maps[0] && data.maps[2] && data.maps[3] && data.maps[4] && !data.maps[1], "Resolved material scalars must activate only their actual maps");
    require(data.maps[2]->pixels[0] == 128 && data.maps[3]->pixels[0] == 64 && data.maps[4]->pixels[0] == 255, "Material scalar pixels must preserve independent metallic, roughness and emission values");
    require(data.maps[0]->pixels[0] == 255 && data.maps[0]->pixels[1] == 0 && data.maps[0]->pixels[3] == 128, "Borrowed image pixels must preserve source color and straight alpha");
    const auto retained = data.maps[0]->pixels;
    std::fill(pixels.begin(), pixels.end(), 0);
    require(data.maps[0]->pixels == retained, "Baked output must not retain caller image buffers");
    fixture.options.maximum_bake_payload_bytes = data.payloadBytes;
    dust3d::native::BakedMeshData exact(fixture.options);
    exact.appendTriangle(1);
    exact.prepare(fixture.object, fixture.snapshot, fixture.atlas);
    require(exact.payloadBytes == data.payloadBytes, "Exact mixed-map budgets must account for retained C tangents instead of temporary original records");
}
void proceduralSnapshotInputs()
{
    AnalyticBake fixture;
    fixture.options.color_size = fixture.options.normal_size = 64;
    fixture.snapshot.parts.clear();
    fixture.object.vertices.erase(fixture.object.vertices.begin(), fixture.object.vertices.begin() + 3);
    fixture.object.triangles = { { 0, 1, 2 } };
    fixture.atlas.triangleUvs.erase(fixture.atlas.triangleUvs.begin());
    fixture.atlas.triangleUvKnown = { 1 };
    fixture.atlas.triangleComponentIds = { fixture.component };
    auto& attributes = fixture.snapshot.components[fixture.component.toString()];
    attributes["wrap"] = "true";
    attributes["wrapPattern"] = "Spots";
    attributes["wrapPatternColor"] = "#ff0000";
    attributes["wrapPatternScale"] = ".1";
    attributes["wrapBelly"] = ".4";
    attributes["wrapWrinkles"] = "1";
    dust3d::ClothFolds::Fold fold;
    fold.points = { { .5, 0, 0 }, { .5, 1, 0 } };
    fold.normals = { { 0, 0, 1 }, { 0, 0, 1 } };
    fold.width = .2;
    fold.depth = .04;
    attributes["__wrapFolds"] = dust3d::ClothFolds::serialize({ fold });
    dust3d::native::BakedMeshData data(fixture.options);
    data.validateVisuals(fixture.snapshot);
    data.appendTriangle(0);
    data.prepare(fixture.object, fixture.snapshot, fixture.atlas);
    dust3d::TextureBakeInputs inputs;
    auto& surface = inputs.surfaces[fixture.component];
    dust3d::SurfacePattern::Settings pattern;
    pattern.type = dust3d::SurfacePattern::Type::Spots;
    pattern.baseColor = dust3d::Color(1, 1, 1);
    pattern.patternColor = dust3d::Color(1, 0, 0);
    pattern.scale = .1;
    pattern.belly = .4;
    pattern.seed = dust3d::SurfacePattern::seedFromString(fixture.component.toString());
    surface.pattern = pattern;
    surface.folds = { fold };
    surface.wrinkleStrength = 1;
    dust3d::TextureBakeOptions options;
    options.colorSize = options.normalSize = 64;
    options.materialSize = 8;
    const auto expected = dust3d::TextureBaker::bake(fixture.object, fixture.atlas, inputs, options);
    require(data.maps[0] && data.maps[1] && expected.normal && data.maps[0]->pixels == expected.color.pixels && data.maps[1]->pixels == expected.normal->pixels, "Snapshot pattern/ink/scale/belly/seed/folds must match independently constructed resolved inputs");
    attributes["wrapPatternScale"] = "0,100000";
    attributes["wrapBelly"] = "0,400000";
    attributes["wrapWrinkles"] = "1,000000";
    dust3d::native::BakedMeshData legacy(fixture.options);
    legacy.appendTriangle(0);
    legacy.prepare(fixture.object, fixture.snapshot, fixture.atlas);
    require(legacy.maps[0]->pixels == data.maps[0]->pixels && legacy.maps[1]->pixels == data.maps[1]->pixels, "Legacy comma-decimal wrapping settings must retain their pattern, belly and fold outputs");
    fixture.options.maximum_bake_payload_bytes = data.payloadBytes;
    dust3d::native::BakedMeshData exact(fixture.options);
    exact.appendTriangle(0);
    exact.prepare(fixture.object, fixture.snapshot, fixture.atlas);
    require(exact.payloadBytes == data.payloadBytes, "Exact normal-map budgets must survive C tangent conversion");
    attributes["wrapPatternScale"] = ".001";
    pattern.scale = .005;
    surface.pattern = pattern;
    const auto clampedReference = dust3d::TextureBaker::bake(fixture.object, fixture.atlas, inputs, options);
    dust3d::native::BakedMeshData clamped(fixture.options);
    clamped.appendTriangle(0);
    clamped.prepare(fixture.object, fixture.snapshot, fixture.atlas);
    require(clamped.maps[0]->pixels == clampedReference.color.pixels, "Snapshot pattern scale must retain the reference UV generator minimum of .005");
}
std::string archive(const std::string& xml)
{
    const auto header = "<ds3><model name='model.xml' offset='0' size='" + std::to_string(xml.size()) + "'/></ds3>";
    std::ostringstream prefix;
    prefix << "DUST3D 1.0 xml " << std::setw(10) << std::setfill('0') << (26 + header.size()) << '\n';
    require(prefix.str().size() == 26, "Independent archive prefix width must be exact");
    return prefix.str() + header + xml;
}
void actualFilteredTriangles()
{
    const auto bytes = archive(precisionModelXml());
    std::vector<dust3d::TextureTangent> expected;
    size_t originalCount = 0;
    {
        std::lock_guard<std::mutex> lock(dust3d::native::engineMutex());
        std::string warning;
        auto snapshot = dust3d::native::readDocument(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size(), warning);
        dust3d::MeshGenerator generator(snapshot.get());
        snapshot.release();
        generator.generate();
        std::unique_ptr<dust3d::Object> object(generator.takeObject());
        std::unique_ptr<dust3d::Snapshot> generated(generator.takeSnapshot());
        require(generator.isSuccessful() && object && generated, "Independent precision geometry must generate");
        originalCount = object->triangles.size();
        require(object->vertices[object->triangles.front()[0]].x() > 1, "Precision fixture must drop a prefix to detect ordinal remapping errors");
        dust3d::native::UvMetadata metadata(*generated, 1024 * 1024);
        metadata.prepare(*object, *generated, {}, 4096);
        dust3d::TextureBakeOptions options;
        options.colorSize = options.normalSize = options.materialSize = 8;
        const auto result = dust3d::TextureBaker::bake(*object, metadata.atlas(), {}, options);
        for (size_t index = 0; index < object->triangles.size(); ++index) {
            const auto& triangle = object->triangles[index];
            // Independently classify representability using the exported float points.
            auto converted = [&](size_t corner) {
                const auto& point = object->vertices[triangle[corner]];
                return dust3d::Vector3(static_cast<float>(point.x()), static_cast<float>(point.y()), static_cast<float>(point.z()));
            };
            const auto a = converted(0), b = converted(1), c = converted(2);
            if (dust3d::Vector3::crossProduct(b - a, c - a).lengthSquared() > 0)
                expected.push_back(result.tangents[index]);
        }
    }
    Dust3DBakeOptions options {};
    options.struct_size = sizeof(options);
    options.color_size = options.normal_size = options.material_size = 8;
    Dust3DError error {};
    std::unique_ptr<Dust3DBakedMesh, decltype(&dust3d_baked_mesh_destroy)> owner(dust3d_baked_mesh_load(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size(), &options, &error), dust3d_baked_mesh_destroy);
    if (!owner)
        throw std::runtime_error(error.message);
    const auto count = dust3d_baked_mesh_tangent_count(owner.get());
    const auto* uvs = dust3d_baked_mesh_uvs(owner.get());
    require(count > 0 && count < originalCount && count == expected.size() && count == dust3d_uv_mesh_triangle_count(uvs), "Actual float-filtered C geometry, UVs and tangents must retain identical triangle ordinals");
    const auto* tangents = dust3d_baked_mesh_tangents(owner.get());
    const auto* vertices = dust3d_mesh_vertices(dust3d_uv_mesh_geometry(uvs));
    for (size_t index = 0; index < count; ++index) {
        require(std::abs(vertices[index * 3].position[0]) < 1, "Far precision triangles must not escape float export filtering");
        require(tangents[index].known == expected[index].known && tangents[index].handedness == expected[index].handedness, "Filtered tangent validity and handedness must follow original records");
        for (size_t axis = 0; axis < 3; ++axis)
            require(std::abs(tangents[index].direction[axis] - expected[index].direction[axis]) < 1e-6, "Filtered tangent vectors must match independently selected original frames");
    }
}
void publicImagesAndMaterials()
{
    const std::string part = "{aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa}";
    const std::string imageKey = "{eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee}";
    auto model = [&](const std::string& material) {
        return archive("<canvas originX='0' originY='0' originZ='0'><nodes><node id='{11111111-1111-4111-8111-111111111111}' partId='" + part
            + "' x='0' y='0' z='0' radius='.1'/></nodes><edges/><parts><part id='" + part + "' " + material
            + "/></parts><components><component id='{cccccccc-cccc-4ccc-8ccc-cccccccccccc}' linkDataType='partId' linkData='" + part
            + "' color='#ffffff' colorImageId='" + imageKey + "'/></components></canvas>");
    };
    const auto bytes = model("metallic='.5' roughness='.25' emissive='2'");
    std::vector<uint8_t> pixels { 17, 91, 200, 128 };
    Dust3DBakeImage image {};
    std::strcpy(image.image_id, imageKey.c_str());
    image.width = image.height = 1;
    image.row_stride = image.byte_count = 4;
    image.pixels = pixels.data();
    Dust3DBakeOptions options {};
    options.struct_size = sizeof(options);
    options.color_size = options.normal_size = options.material_size = 8;
    options.images = &image;
    options.image_count = 1;
    Dust3DError error {};
    using Owner = std::unique_ptr<Dust3DBakedMesh, decltype(&dust3d_baked_mesh_destroy)>;
    Owner owner(dust3d_baked_mesh_load(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size(), &options, &error), dust3d_baked_mesh_destroy);
    if (!owner)
        throw std::runtime_error(error.message);
    const auto color = dust3d_baked_mesh_texture(owner.get(), DUST3D_TEXTURE_COLOR);
    for (size_t index = 0; index < color.byte_count; index += 4)
        require(std::equal(pixels.begin(), pixels.end(), color.pixels + index), "Actual archive baking must resolve borrowed pixels and preserve straight alpha");
    require(dust3d_baked_mesh_texture(owner.get(), DUST3D_TEXTURE_METALLIC).pixels[0] == 128 && dust3d_baked_mesh_texture(owner.get(), DUST3D_TEXTURE_ROUGHNESS).pixels[0] == 64 && dust3d_baked_mesh_texture(owner.get(), DUST3D_TEXTURE_EMISSIVE).pixels[0] == 255, "Actual generated snapshot must retain material scalar inputs");
    const auto halfBytes = model("emissive='.5'");
    Owner half(dust3d_baked_mesh_load(reinterpret_cast<const uint8_t*>(halfBytes.data()), halfBytes.size(), &options, &error), dust3d_baked_mesh_destroy);
    require(half && dust3d_baked_mesh_texture(half.get(), DUST3D_TEXTURE_EMISSIVE).pixels[0] == 127,
        "Procedural half-strength white emission must match the Qt byte truncation");
    const auto invalid = dust3d_baked_mesh_texture(owner.get(), uint32_t(99));
    require(!invalid.pixels && invalid.byte_count == 0, "Invalid map requests must return empty views without indexing storage");
    require(!dust3d_baked_mesh_texture(owner.get(), std::numeric_limits<uint32_t>::max()).pixels, "All unknown selector bit patterns must be safely rejected");
    options.maximum_bake_payload_bytes = dust3d_baked_mesh_payload_bytes(owner.get()) - 1;
    require(!dust3d_baked_mesh_load(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size(), &options, &error) && error.status == DUST3D_STATUS_LIMIT_EXCEEDED && *error.message, "Optional map budget failures must propagate the baker cause through the C boundary");
    options.maximum_bake_payload_bytes = 0;
    options.maximum_uv_payload_bytes = 256;
    require(!dust3d_baked_mesh_load(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size(), &options, &error) && error.status == DUST3D_STATUS_LIMIT_EXCEEDED && *error.message, "Independent UV workspace limits must not publish a partial baked owner");
    options.maximum_uv_payload_bytes = 0;
    std::fill(pixels.begin(), pixels.end(), 0);
    require(color.pixels[0] == 17 && color.pixels[3] == 128, "Returned owners must not retain source pixel storage");
    options.images = nullptr;
    options.image_count = 0;
    require(!dust3d_baked_mesh_load(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size(), &options, &error) && error.status == DUST3D_STATUS_UNSUPPORTED_ASSET && std::strstr(error.message, "image"), "Missing decoded assets must fail explicitly rather than baking a fallback color");
    options.images = &image;
    options.image_count = 1;
    const auto legacyBytes = model("metallic='0,500000' roughness='0,250000' emissive='2,000000'");
    std::unique_ptr<Dust3DMesh, decltype(&dust3d_mesh_destroy)> legacyPreview(dust3d_mesh_load(reinterpret_cast<const uint8_t*>(legacyBytes.data()), legacyBytes.size(), &error), dust3d_mesh_destroy);
    require(legacyPreview != nullptr, "Legacy comma-decimal archive must remain accepted by ordinary generation");
    Owner legacy(dust3d_baked_mesh_load(reinterpret_cast<const uint8_t*>(legacyBytes.data()), legacyBytes.size(), &options, &error), dust3d_baked_mesh_destroy);
    require(legacy && dust3d_baked_mesh_texture(legacy.get(), DUST3D_TEXTURE_METALLIC).pixels[0] == 128 && dust3d_baked_mesh_texture(legacy.get(), DUST3D_TEXTURE_ROUGHNESS).pixels[0] == 64, "Baking must preserve legacy comma-decimal material values accepted by normal generation");
    for (const auto* value : { "-1", "2", "nan", ".5junk", "0,5junk", "0,5.2" }) {
        const auto malformed = model(std::string("metallic='") + value + "'");
        require(!dust3d_baked_mesh_load(reinterpret_cast<const uint8_t*>(malformed.data()), malformed.size(), &options, &error) && error.status == DUST3D_STATUS_UNSUPPORTED_ASSET && std::strstr(error.message, "metallic"), "Unsupported material scalars must preserve their specific cause at the C boundary");
    }
}
void ownedCapsule(const std::string& directory)
{
    std::ifstream file(directory + "/Capsule.ds3", std::ios::binary);
    require(file.good(), "Baked owner fixture must exist");
    const std::vector<uint8_t> bytes { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
    Dust3DBakeOptions options {};
    options.struct_size = sizeof(options);
    options.color_size = options.normal_size = options.material_size = 8;
    Dust3DError error {};
    using Owner = std::unique_ptr<Dust3DBakedMesh, decltype(&dust3d_baked_mesh_destroy)>;
    Owner owner(dust3d_baked_mesh_load(bytes.data(), bytes.size(), &options, &error), dust3d_baked_mesh_destroy);
    if (!owner)
        throw std::runtime_error(error.message);
    const auto* uvs = dust3d_baked_mesh_uvs(owner.get());
    const auto color = dust3d_baked_mesh_texture(owner.get(), DUST3D_TEXTURE_COLOR);
    require(uvs && dust3d_uv_mesh_triangle_count(uvs) == 80 && dust3d_baked_mesh_tangent_count(owner.get()) == 80, "Real capsule geometry, UVs and tangents must align");
    require(color.width == 8 && color.height == 8 && color.row_stride == 32 && color.byte_count == 256 && color.pixels, "Baked capsule must expose actual bounded RGBA8 pixels");
    require(!dust3d_baked_mesh_texture(owner.get(), DUST3D_TEXTURE_NORMAL).pixels, "Plain capsule must not invent normal maps");
    const std::vector<uint8_t> copied(color.pixels, color.pixels + color.byte_count);
    options.maximum_bake_payload_bytes = dust3d_baked_mesh_payload_bytes(owner.get());
    require(options.maximum_bake_payload_bytes == 256 + 80 * sizeof(Dust3DTextureTangent), "Final payload counts maps and aligned C tangent records exactly");
    Owner exact(dust3d_baked_mesh_load(bytes.data(), bytes.size(), &options, &error), dust3d_baked_mesh_destroy);
    require(exact && std::equal(copied.begin(), copied.end(), color.pixels), "Exact-budget later loads must preserve earlier borrowed pixels");
    --options.maximum_bake_payload_bytes;
    require(!dust3d_baked_mesh_load(bytes.data(), bytes.size(), &options, &error) && error.status == DUST3D_STATUS_LIMIT_EXCEEDED, "One-byte insufficient bake budgets must not publish partial owners");
}
void cancellationWithoutEngineLock()
{
    Dust3DError error {};
    std::unique_ptr<Dust3DCancellationToken, decltype(&dust3d_cancellation_token_destroy)> token(dust3d_cancellation_token_create(&error), dust3d_cancellation_token_destroy);
    require(token && error.status == DUST3D_STATUS_OK, "Cancellation token allocation must succeed");
    dust3d::native::checkCancellation(nullptr);
    dust3d::native::checkCancellation(token.get());
    std::unique_lock<std::mutex> engineLock(dust3d::native::engineMutex());
    auto worker = std::async(std::launch::async, [&] {
        dust3d_cancellation_token_cancel(token.get());
        return dust3d_cancellation_token_is_cancelled(token.get());
    });
    const auto state = worker.wait_for(std::chrono::seconds(2));
    // Always release the lock and join before asserting, including a regression
    // where cancellation incorrectly waits on generation's mutex.
    engineLock.unlock();
    const auto cancelled = worker.get();
    require(state == std::future_status::ready && cancelled, "Cancellation/query must complete while the engine mutex remains held");
    bool rejected = false;
    try {
        dust3d::native::checkCancellation(token.get());
    } catch (const dust3d::native::DocumentError& cause) {
        rejected = cause.status() == DUST3D_STATUS_CANCELLED && std::strcmp(cause.what(), "The native operation was cancelled.") == 0;
    }
    require(rejected, "Cancelled native operations must preserve their typed status and cause");
}
}
int main(int argc, char** argv)
{
    try {
        cancellationWithoutEngineLock();
        nullBakedOwners();
        invalidBakeInputs();
        visualResolutionBoundaries();
        archivedImageEncodingSelectsTheRaster();
        analyticMaterialsImagesAndTangents();
        proceduralSnapshotInputs();
        actualFilteredTriangles();
        publicImagesAndMaterials();
        require(argc == 2, "Baked mesh tests require the fixture directory");
        ownedCapsule(argv[1]);
        std::cout << "Baked mesh boundary contracts passed\n";
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}
