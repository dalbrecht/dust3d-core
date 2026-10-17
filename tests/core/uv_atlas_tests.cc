#include <cmath>
#include <dust3d/base/object.h>
#include <dust3d/uv/uv_atlas_builder.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <native/Engine/GenerationPipeline.h>
#include <stdexcept>

namespace {
void require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}

const dust3d::Uuid componentId("{cccccccc-cccc-4ccc-8ccc-cccccccccccc}");

void populate(dust3d::Object& object, bool withUvs = true)
{
    object.vertices = { { 0, 0, 0 }, { 1, 0, 0 }, { 1, 1, 0 }, { 0, 1, 0 } };
    object.triangles = { { 0, 1, 2 }, { 0, 2, 3 } };
    if (withUvs) {
        object.componentTriangleUvs[componentId][{ dust3d::PositionKey(object.vertices[0]), dust3d::PositionKey(object.vertices[1]), dust3d::PositionKey(object.vertices[2]) }]
            = { dust3d::Vector2(0, 0), dust3d::Vector2(1, 0), dust3d::Vector2(1, 1) };
        object.componentTriangleUvs[componentId][{ dust3d::PositionKey(object.vertices[0]), dust3d::PositionKey(object.vertices[2]), dust3d::PositionKey(object.vertices[3]) }]
            = { dust3d::Vector2(0, 0), dust3d::Vector2(1, 1), dust3d::Vector2(0, 1) };
    }
}

std::map<dust3d::Uuid, dust3d::UvAtlasComponent> components()
{
    dust3d::UvAtlasComponent component;
    component.color = dust3d::Color(1, 0, 0);
    return { { componentId, component } };
}

template <typename Action>
void rejects(Action action, dust3d::UvAtlasErrorCode code)
{
    try {
        action();
    } catch (const dust3d::UvAtlasError& error) {
        require(error.code() == code, "Unexpected UV error classification");
        require(*error.what() != '\0', "UV diagnostics preserve the cause");
        return;
    }
    throw std::runtime_error("Invalid UV input unexpectedly succeeded");
}

void imagelessAtlas()
{
    dust3d::Object object;
    populate(object);
    const auto result = dust3d::UvAtlasBuilder::build(object, components());
    require(result.layouts.size() == 1 && result.layouts[0].sourceId == componentId, "One source component must have one owned layout");
    require(result.gradients.empty(), "Ordinary chart must not create a seam texture");
    require(result.packedTextureSize > 0 && std::isfinite(result.packedTextureSize), "Atlas size must be finite and positive");
    require(result.triangleUvs.size() == 2 && result.triangleUvKnown == std::vector<uint8_t>({ 1, 1 }), "Every mapped triangle needs three corner UVs and an explicit known marker");
    require(result.triangleComponentIds == std::vector<dust3d::Uuid>({ componentId, componentId }), "Source components must follow triangle order");
    for (const auto& triangle : result.triangleUvs) {
        for (const auto& uv : triangle) {
            require(std::isfinite(uv.x()) && std::isfinite(uv.y()) && uv.x() >= 0 && uv.x() <= 1 && uv.y() >= 0 && uv.y() <= 1, "Known unit-square UVs must pack inside the atlas");
        }
    }
    require(object.triangleComponentIds.empty(), "Building an atlas must not mutate its input object");
    auto options = dust3d::UvAtlasOptions {};
    options.maximumPayloadBytes = result.payloadBytes;
    require(dust3d::UvAtlasBuilder::build(object, components(), options).payloadBytes == result.payloadBytes, "Exact output payload budget must succeed");
    options.maximumPayloadBytes -= 1;
    rejects([&] { dust3d::UvAtlasBuilder::build(object, components(), options); }, dust3d::UvAtlasErrorCode::limitExceeded);
}

void missingUvs()
{
    dust3d::Object object;
    populate(object, false);
    const auto result = dust3d::UvAtlasBuilder::build(object, {});
    require(result.layouts.empty() && result.packedTextureSize == 0, "No charts must remain an explicit empty atlas");
    require(result.triangleUvKnown == std::vector<uint8_t>({ 0, 0 }), "Missing UV records must not masquerade as complete mapping");
    require(result.triangleComponentIds.size() == 2 && result.triangleComponentIds[0].isNull(), "Unknown ownership must remain explicit");
}

const dust3d::Uuid secondId("{dddddddd-dddd-4ddd-8ddd-dddddddddddd}");
const dust3d::Uuid imageId("{eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee}");
using TriangleKey = std::array<dust3d::PositionKey, 3>;
using Uvs = std::array<dust3d::Vector2, 3>;

TriangleKey triangleKey(const dust3d::Object& object, size_t index)
{
    const auto& face = object.triangles[index];
    return { dust3d::PositionKey(object.vertices[face[0]]), dust3d::PositionKey(object.vertices[face[1]]), dust3d::PositionKey(object.vertices[face[2]]) };
}

bool sameUvs(const Uvs& first, const Uvs& second)
{
    for (size_t i = 0; i < 3; ++i) {
        if (first[i].x() != second[i].x() || first[i].y() != second[i].y())
            return false;
    }
    return true;
}

void imageAndAreaCharts()
{
    dust3d::Object object;
    populate(object);
    const auto key = triangleKey(object, 1);
    object.componentTriangleUvs[secondId][key] = object.componentTriangleUvs[componentId].at(key);
    object.componentTriangleUvs[componentId].erase(key);
    auto inputs = components();
    inputs[secondId].color = dust3d::Color(0, 0, 1);
    inputs[secondId].image = dust3d::UvAtlasImage { imageId, 64, 128 };
    const auto atlas = dust3d::UvAtlasBuilder::build(object, inputs);
    require(atlas.layouts.size() == 2, "Mixed image and imageless sources need separate charts");
    bool sawImage = false, sawSolid = false;
    for (const auto& layout : atlas.layouts) {
        if (layout.sourceId == secondId) {
            require(layout.id == imageId && layout.color.b() == 1, "Image identity and resolved color must survive packing");
            const auto ratio = layout.width / layout.height;
            require(std::abs(ratio - 0.5) < 0.0001 || std::abs(ratio - 2) < 0.0001, "Packing must preserve the source image aspect ratio under rotation");
            sawImage = true;
        } else {
            require(layout.width * layout.height > 0.25, "Imageless charts must retain area-proportional texel allocation rather than collapsing beside images");
            sawSolid = true;
        }
    }
    require(sawImage && sawSolid, "Both source charts must remain addressable");
    inputs[secondId].image = dust3d::UvAtlasImage { imageId, 8192, 8192 };
    require(dust3d::UvAtlasBuilder::build(object, inputs).layouts.size() == 2, "Maximum bounded image dimensions describe metadata without allocating pixels");
}

void seamCharts()
{
    dust3d::Object object;
    object.vertices = { { 0, 0, 0 }, { 0, 1, 0 }, { 1, 0, 0 }, { 1, 1, 0 }, { -1, 0, 0 }, { 2, 0, 0 } };
    object.triangles = { { 0, 1, 2 }, { 2, 3, 1 } };
    object.componentTriangleUvs[componentId][{ dust3d::PositionKey(object.vertices[0]), dust3d::PositionKey(object.vertices[1]), dust3d::PositionKey(object.vertices[4]) }] = { dust3d::Vector2(0, 0), dust3d::Vector2(1, 0), dust3d::Vector2(0, 1) };
    object.componentTriangleUvs[secondId][{ dust3d::PositionKey(object.vertices[2]), dust3d::PositionKey(object.vertices[3]), dust3d::PositionKey(object.vertices[5]) }] = { dust3d::Vector2(0, 0), dust3d::Vector2(1, 0), dust3d::Vector2(0, 1) };
    auto inputs = components();
    inputs[secondId].color = dust3d::Color(0, 0, 1);
    object.seamTriangleUvs.push_back({ {}, {} });
    object.seamTriangleUvs.push_back({ { triangleKey(object, 0) }, { triangleKey(object, 1) } });
    auto atlas = dust3d::UvAtlasBuilder::build(object, inputs);
    require(atlas.gradients.size() == 1 && atlas.layouts.size() == 3, "Empty seams must not allocate charts; populated seams precede ordinary charts");
    const auto& gradient = atlas.gradients[0];
    require(gradient.layoutIndex == 0 && gradient.resolution == 512, "Gradient textures are addressed by owned layout ordinal");
    require(gradient.largeColor.r() == 1 && gradient.smallColor.b() == 1, "Seam endpoints must retain the correct source colors");
    require(atlas.layouts[0].id.isNull(), "Portable gradient charts must not depend on a global image registry");
    require(atlas.triangleUvKnown == std::vector<uint8_t>({ 1, 1 }), "Both seam sides must have explicit corner mappings");
    require(sameUvs(atlas.triangleUvs[0], atlas.layouts[0].globalUv.at(triangleKey(object, 0))), "Seam triangle UVs must use the gradient chart");
    object.seamTriangleUvs[1].first.clear();
    atlas = dust3d::UvAtlasBuilder::build(object, inputs);
    require(atlas.gradients[0].largeColor.r() == 1 && atlas.gradients[0].smallColor.b() == 1, "Small-side-only seams must reverse endpoint lookup correctly");
    object.seamTriangleUvs[1].first.insert(triangleKey(object, 0));
    object.componentTriangleUvs[componentId][triangleKey(object, 0)] = Uvs {};
    atlas = dust3d::UvAtlasBuilder::build(object, inputs);
    require(sameUvs(atlas.triangleUvs[0], atlas.layouts[0].globalUv.at(triangleKey(object, 0))), "Seam chart precedence must survive conflicting ordinary UV records");
    const auto originalSeamKey = triangleKey(object, 0);
    object.triangles[0] = { 1, 2, 0 };
    object.componentTriangleUvs[componentId][triangleKey(object, 0)] = Uvs {};
    atlas = dust3d::UvAtlasBuilder::build(object, inputs);
    const auto& seamUvs = atlas.layouts[0].globalUv.at(originalSeamKey);
    require(sameUvs(atlas.triangleUvs[0], Uvs { seamUvs[1], seamUvs[2], seamUvs[0] }), "Seam precedence must apply across cyclic keys, ahead of an exact ordinary-chart record");
    object.componentTriangleUvs.clear();
    atlas = dust3d::UvAtlasBuilder::build(object, {});
    require(atlas.layouts.size() == 1 && atlas.gradients[0].largeColor.r() == 200.0 / 255 && atlas.gradients[0].smallColor.b() == 200.0 / 255, "Missing seam color sources must retain the reference gray fallback");
    require(atlas.triangleComponentIds[0].isNull(), "A gradient mapping must not invent source ownership");
}

void ownershipAndMissingCorners()
{
    dust3d::Object object;
    populate(object);
    object.componentTriangleUvs[componentId].erase(triangleKey(object, 1));
    auto atlas = dust3d::UvAtlasBuilder::build(object, components());
    require(atlas.triangleUvKnown == std::vector<uint8_t>({ 1, 0 }), "Neighbour ownership cannot invent missing UV coordinates");
    require(atlas.triangleComponentIds == std::vector<dust3d::Uuid>({ componentId, componentId }), "Shared-edge ownership propagates from a known neighbour");
    object.triangleComponentIds = { secondId };
    atlas = dust3d::UvAtlasBuilder::build(object, components());
    require(atlas.triangleComponentIds == std::vector<dust3d::Uuid>({ secondId, secondId }), "Existing uncombined ownership takes precedence and propagates without rewriting the input");
    const auto originalUvs = atlas.triangleUvs[0];
    object.triangleComponentIds = { secondId, secondId, componentId };
    atlas = dust3d::UvAtlasBuilder::build(object, components());
    require(atlas.triangleComponentIds.size() == 2 && object.triangleComponentIds.size() == 3, "Only triangle-aligned ownership is copied without changing excess input records");
    object.triangleComponentIds.clear();
    object.triangles[0] = { 1, 2, 0 };
    atlas = dust3d::UvAtlasBuilder::build(object, components());
    require(atlas.triangleComponentIds[0] == componentId && atlas.triangleUvKnown[0] == 1, "Cyclic vertex order must retain valid source UVs");
    require(sameUvs(atlas.triangleUvs[0], Uvs { originalUvs[1], originalUvs[2], originalUvs[0] }), "Corner UVs must rotate back to actual triangle order");
    object.triangles[0] = { 0, 2, 1 };
    atlas = dust3d::UvAtlasBuilder::build(object, components());
    require(atlas.triangleUvKnown[0] == 0, "Opposite-winding records must not be conflated with a source chart");
    object.componentTriangleUvs.clear();
    object.brokenTrianglesToComponentIdMap[triangleKey(object, 0)] = componentId;
    atlas = dust3d::UvAtlasBuilder::build(object, components());
    require(atlas.layouts.size() == 1 && atlas.layouts[0].sourceId == componentId, "Broken triangles retain their reference solid-color chart");
    require(atlas.triangleUvKnown[0] == 1 && sameUvs(atlas.triangleUvs[0], atlas.layouts[0].globalUv.at(triangleKey(object, 0))), "Broken-triangle fallback remains an explicit chart mapping");
    const auto retained = atlas.triangleUvs;
    object.vertices.clear();
    object.triangles.clear();
    object.brokenTrianglesToComponentIdMap.clear();
    const auto empty = dust3d::UvAtlasBuilder::build(object, {});
    require(empty.layouts.empty() && empty.triangleUvs.empty() && empty.payloadBytes == 0, "A genuinely empty model has no UV payload");
    require(sameUvs(atlas.triangleUvs[0], retained[0]), "Owned UV buffers survive input destruction and subsequent builds");
}

void incompleteChartInputs()
{
    dust3d::Object object;
    populate(object);
    auto atlas = dust3d::UvAtlasBuilder::build(object, {});
    require(atlas.layouts.empty() && atlas.triangleUvKnown == std::vector<uint8_t>({ 0, 0 }), "Missing resolved visual inputs must not fabricate charts");
    require(atlas.triangleComponentIds[0] == componentId, "Missing visual metadata must not erase known geometric ownership");
    object.componentTriangleUvs.clear();
    object.componentTriangleUvs[componentId][{ dust3d::PositionKey(dust3d::Vector3(10, 10, 10)), dust3d::PositionKey(dust3d::Vector3(11, 10, 10)), dust3d::PositionKey(dust3d::Vector3(10, 11, 10)) }] = { dust3d::Vector2(0, 0), dust3d::Vector2(1, 0), dust3d::Vector2(0, 1) };
    atlas = dust3d::UvAtlasBuilder::build(object, components());
    require(atlas.layouts.size() == 1 && atlas.packedTextureSize > 0, "Unmatched retained chart records use the reference positive fallback size");
    require(atlas.triangleUvKnown == std::vector<uint8_t>({ 0, 0 }), "An unused chart must not invent mappings for unrelated geometry");
    const double minimum = -std::ldexp(1.0, std::numeric_limits<long>::digits) / 100000.0;
    object.vertices[0][0] = minimum;
    dust3d::UvAtlasBuilder::build(object, components());
    object.vertices[0][0] = std::nextafter(minimum, -std::numeric_limits<double>::infinity());
    rejects([&] { dust3d::UvAtlasBuilder::build(object, components()); }, dust3d::UvAtlasErrorCode::invalidInput);
}

void invalidMetadata()
{
    dust3d::Object object;
    populate(object);
    for (double value : { -0.1, 1.1, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity() }) {
        for (size_t channel = 0; channel < 4; ++channel) {
            auto inputs = components();
            inputs[componentId].color[channel] = value;
            rejects([&] { dust3d::UvAtlasBuilder::build(object, inputs); }, dust3d::UvAtlasErrorCode::invalidInput);
        }
    }
    for (const auto& image : { dust3d::UvAtlasImage { {}, 64, 64 }, dust3d::UvAtlasImage { imageId, 0, 64 }, dust3d::UvAtlasImage { imageId, 64, 0 }, dust3d::UvAtlasImage { imageId, 8193, 64 }, dust3d::UvAtlasImage { imageId, 64, 8193 } }) {
        auto inputs = components();
        inputs[componentId].image = image;
        rejects([&] { dust3d::UvAtlasBuilder::build(object, inputs); }, dust3d::UvAtlasErrorCode::invalidInput);
    }
    auto options = dust3d::UvAtlasOptions {};
    options.textureSize = 8193;
    rejects([&] { dust3d::UvAtlasBuilder::build(object, components(), options); }, dust3d::UvAtlasErrorCode::invalidInput);
    options = {};
    options.maximumPayloadBytes = 256 * 1024 * 1024 + 1;
    rejects([&] { dust3d::UvAtlasBuilder::build(object, components(), options); }, dust3d::UvAtlasErrorCode::limitExceeded);
    auto& uv = object.componentTriangleUvs.begin()->second.begin()->second[0][0];
    uv = std::numeric_limits<double>::quiet_NaN();
    rejects([&] { dust3d::UvAtlasBuilder::build(object, components()); }, dust3d::UvAtlasErrorCode::invalidInput);
    uv = std::numeric_limits<double>::max();
    rejects([&] { dust3d::UvAtlasBuilder::build(object, components()); }, dust3d::UvAtlasErrorCode::packingFailed);
}

void generatedFixtures(const std::string& directory)
{
    for (const std::string name : { "Capsule.ds3", "Sword.ds3", "RoundedCrossing.ds3" }) {
        std::ifstream stream(directory + "/" + name, std::ios::binary);
        require(stream.good(), "Generated UV fixture is missing");
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
        std::lock_guard<std::mutex> lock(dust3d::native::engineMutex());
        std::string warning;
        auto snapshot = dust3d::native::readDocument(bytes.data(), bytes.size(), warning);
        dust3d::MeshGenerator generator(snapshot.get());
        snapshot.release();
        generator.generate();
        std::unique_ptr<dust3d::Object> object(generator.takeObject());
        require(generator.isSuccessful() && object && !object->triangles.empty(), "Real model generation must succeed before UV construction");
        std::map<dust3d::Uuid, dust3d::UvAtlasComponent> inputs;
        std::unique_ptr<dust3d::Snapshot> generatedSnapshot(generator.takeSnapshot());
        require(generatedSnapshot != nullptr, "Generated material snapshot must remain owned");
        for (const auto& entry : generatedSnapshot->components) {
            auto& component = inputs[dust3d::Uuid(entry.first)];
            const auto color = entry.second.find("color");
            if (color != entry.second.end())
                component.color = dust3d::Color(color->second);
        }
        const auto atlas = dust3d::UvAtlasBuilder::build(*object, inputs);
        require(atlas.triangleUvs.size() == object->triangles.size() && atlas.triangleUvKnown.size() == object->triangles.size() && atlas.triangleComponentIds.size() == object->triangles.size(), "Real mesh metadata must remain triangle aligned");
        require(!atlas.layouts.empty(), "Procedural reference fixtures must produce UV charts");
        size_t known = 0;
        for (size_t index = 0; index < atlas.triangleUvs.size(); ++index) {
            if (atlas.triangleUvKnown[index])
                ++known;
            for (const auto& uv : atlas.triangleUvs[index])
                require(std::isfinite(uv.x()) && std::isfinite(uv.y()), "Real generated UVs must be finite");
        }
        require(known > 0, "Actual generated charts must map real model triangles");
        for (const auto& gradient : atlas.gradients)
            require(gradient.layoutIndex < atlas.layouts.size(), "Every real seam gradient must refer to an owned chart");
        std::cout << name << ": " << atlas.layouts.size() << " charts, " << known << "/" << atlas.triangleUvs.size() << " known UV triangles, " << atlas.gradients.size() << " gradients\n";
    }
}

void invalidBoundaries()
{
    for (int variant = 0; variant < 4; ++variant) {
        dust3d::Object object;
        populate(object);
        if (variant == 0)
            object.vertices[0][0] = std::numeric_limits<double>::quiet_NaN();
        else if (variant == 1)
            object.vertices[0][0] = std::ldexp(1.0, std::numeric_limits<long>::digits) / 100000.0;
        else if (variant == 2)
            object.triangles[0][0] = object.vertices.size();
        else
            object.triangles[0].resize(2);
        rejects([&] { dust3d::UvAtlasBuilder::build(object, components()); }, dust3d::UvAtlasErrorCode::invalidInput);
    }
    dust3d::Object object;
    populate(object);
    auto options = dust3d::UvAtlasOptions {};
    options.maximumPayloadBytes = 1;
    rejects([&] { dust3d::UvAtlasBuilder::build(object, components(), options); }, dust3d::UvAtlasErrorCode::limitExceeded);
    options = {};
    options.textureSize = 0;
    rejects([&] { dust3d::UvAtlasBuilder::build(object, components(), options); }, dust3d::UvAtlasErrorCode::invalidInput);
}
}

int main(int argc, char* argv[])
{
    try {
        imagelessAtlas();
        missingUvs();
        invalidBoundaries();
        imageAndAreaCharts();
        seamCharts();
        ownershipAndMissingCorners();
        invalidMetadata();
        incompleteChartInputs();
        require(argc == 2, "Expected the independent fixture directory");
        generatedFixtures(argv[1]);
        std::cout << "Portable UV atlas contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
