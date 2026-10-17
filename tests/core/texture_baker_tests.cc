// SPDX-License-Identifier: MIT
#include <cmath>
#include <dust3d/base/object.h>
#include <dust3d/uv/texture_baker.h>
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
const dust3d::Uuid imageId("{eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee}");
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
        atlas.triangleComponentIds = { componentId, componentId };
        dust3d::UvMapPacker::Layout layout;
        layout.sourceId = componentId;
        layout.color = dust3d::Color(0.2, 0.4, 0.8);
        layout.width = layout.height = 1;
        for (size_t index = 0; index < object.triangles.size(); ++index) {
            const auto& triangle = object.triangles[index];
            layout.globalUv[{ dust3d::PositionKey(object.vertices[triangle[0]]), dust3d::PositionKey(object.vertices[triangle[1]]), dust3d::PositionKey(object.vertices[triangle[2]]) }] = atlas.triangleUvs[index];
        }
        atlas.layouts.push_back(layout);
        options.colorSize = 8;
        options.normalSize = 8;
        options.materialSize = 8;
    }
    dust3d::TextureBakeResult bake(const dust3d::TextureBakeInputs& inputs = {}) const
    {
        return dust3d::TextureBaker::bake(object, atlas, inputs, options);
    }
};
template <typename Action>
void rejects(Action action, dust3d::TextureBakeErrorCode code)
{
    try {
        action();
    } catch (const dust3d::TextureBakeError& error) {
        require(error.code() == code && *error.what(), "Typed bake error must preserve its cause");
        return;
    }
    throw std::runtime_error("Invalid bake unexpectedly succeeded");
}
void solidAndOwnership()
{
    Fixture fixture;
    auto result = fixture.bake();
    require(result.color.width == 8 && result.color.height == 8 && result.color.pixels.size() == 256, "Color output dimensions and storage must agree");
    for (size_t index = 0; index < result.color.pixels.size(); index += 4) {
        require(result.color.pixels[index] == 51 && result.color.pixels[index + 1] == 102 && result.color.pixels[index + 2] == 204 && result.color.pixels[index + 3] == 255, "Solid chart must bake its encoded color");
    }
    require(!result.normal && !result.metallic && !result.roughness && !result.emissive, "Absent material features must not allocate maps");
    require(result.tangents.size() == 2, "Tangents must follow original triangle order");
    require(result.payloadBytes >= result.color.pixels.size(), "Payload accounting must include output pixels");
    fixture.atlas.layouts[0].color = dust3d::Color(1, 0, 0);
    require(result.color.pixels[0] == 51 && fixture.bake().color.pixels[0] == 255, "Bake outputs must own pixels independently of inputs and later calls");
}
void materialsAndTransparency()
{
    Fixture fixture;
    fixture.atlas.layouts[0].color = dust3d::Color(0, 1, 0, 0);
    require(fixture.bake().color.pixels[3] == 0, "Transparent green is covered data rather than an empty sentinel");
    fixture.atlas.layouts[0].color = dust3d::Color(1, 0.5, 0);
    dust3d::TextureBakeInputs inputs;
    inputs.materials[componentId] = { 0.5, 0.25, 2 };
    const auto result = fixture.bake(inputs);
    require(result.metallic && result.roughness && result.emissive, "Active material features need owned maps");
    require(result.metallic->pixels[0] == 128 && result.roughness->pixels[0] == 64, "Scalar maps must round reference values");
    require(result.emissive->pixels[0] == 255 && result.emissive->pixels[1] == 128 && result.emissive->pixels[2] == 0, "RGBA8 emission clips strength at one");
}
void imageOrientation()
{
    Fixture fixture;
    fixture.options.colorSize = 2;
    fixture.atlas.layouts[0].id = imageId;
    std::vector<uint8_t> pixels = { 255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 0, 255 };
    dust3d::TextureBakeInputs inputs;
    inputs.images[imageId] = { 2, 2, 8, pixels.size(), pixels.data() };
    require(fixture.bake(inputs).color.pixels == pixels, "Image chart must preserve pixels without solid color modulation");
    fixture.atlas.layouts[0].flipped = true;
    const std::vector<uint8_t> transposed = { 255, 0, 0, 255, 0, 0, 255, 255, 0, 255, 0, 255, 255, 255, 0, 255 };
    require(fixture.bake(inputs).color.pixels == transposed, "Flipped chart transposes UV image axes");
    inputs.images[imageId].rowStride = 7;
    rejects([&] { fixture.bake(inputs); }, dust3d::TextureBakeErrorCode::invalidInput);
}
void surfaceDetails()
{
    Fixture fixture;
    fixture.options.colorSize = 64;
    fixture.options.normalSize = 64;
    dust3d::TextureBakeInputs inputs;
    auto& surface = inputs.surfaces[componentId];
    dust3d::SurfacePattern::Settings pattern;
    pattern.type = dust3d::SurfacePattern::Type::Spots;
    pattern.baseColor = dust3d::Color(1, 1, 1);
    pattern.patternColor = dust3d::Color(0, 0, 0);
    pattern.scale = 0.1;
    surface.pattern = pattern;
    const auto patterned = fixture.bake(inputs);
    auto minimum = uint8_t(255);
    auto maximum = uint8_t(0);
    for (size_t index = 0; index < patterned.color.pixels.size(); index += 4) {
        minimum = std::min(minimum, patterned.color.pixels[index]);
        maximum = std::max(maximum, patterned.color.pixels[index]);
    }
    require(maximum > minimum + 64, "World-space pattern must produce spatial color variation");
    dust3d::ClothFolds::Fold fold;
    fold.points = { { 0.5, 0, 0 }, { 0.5, 1, 0 } };
    fold.normals = { { 0, 0, 1 }, { 0, 0, 1 } };
    fold.width = 0.2;
    fold.depth = 0.04;
    surface.folds = { fold };
    surface.wrinkleStrength = 1;
    fixture.options.maximumSpatialGridEntries = 1;
    rejects([&] { fixture.bake(inputs); }, dust3d::TextureBakeErrorCode::limitExceeded);
    fixture.options.maximumSpatialGridEntries = 1000000;
    const auto folded = fixture.bake(inputs);
    require(folded.normal.has_value(), "Active cloth folds must generate a normal map");
    size_t nonflat = 0;
    for (size_t index = 0; index < folded.normal->pixels.size(); index += 4) {
        if (folded.normal->pixels[index] != 128 || folded.normal->pixels[index + 1] != 128)
            ++nonflat;
    }
    require(nonflat > 0, "Fold normal map must contain actual evaluated surface detail");
}
void malformedSurfaceInputs()
{
    Fixture fixture;
    dust3d::TextureBakeInputs inputs;
    auto& surface = inputs.surfaces[componentId];
    surface.wrinkleStrength = std::numeric_limits<double>::quiet_NaN();
    rejects([&] { fixture.bake(inputs); }, dust3d::TextureBakeErrorCode::invalidInput);
    surface.wrinkleStrength = 0;
    surface.pattern = dust3d::SurfacePattern::Settings {};
    surface.pattern->scale = 0;
    rejects([&] { fixture.bake(inputs); }, dust3d::TextureBakeErrorCode::invalidInput);
    surface.pattern.reset();
    dust3d::UvAtlasGradient gradient;
    gradient.layoutIndex = 2;
    fixture.atlas.gradients = { gradient };
    rejects([&] { fixture.bake(); }, dust3d::TextureBakeErrorCode::invalidInput);
    fixture.atlas.gradients.clear();
    fixture.object.vertices[0] = { 1e30, 0, 0 };
    rejects([&] { fixture.bake(); }, dust3d::TextureBakeErrorCode::invalidInput);
}
void unsafeProceduralCoordinates()
{
    Fixture fixture;
    dust3d::TextureBakeInputs inputs;
    auto& surface = inputs.surfaces[componentId];
    surface.pattern = dust3d::SurfacePattern::Settings {};
    surface.pattern->type = dust3d::SurfacePattern::Type::Spots;
    surface.pattern->scale = 1e-4;
    for (auto& vertex : fixture.object.vertices)
        vertex[0] += 1e12;
    fixture.atlas.layouts[0].globalUv.clear();
    for (size_t index = 0; index < fixture.object.triangles.size(); ++index) {
        const auto& triangle = fixture.object.triangles[index];
        fixture.atlas.layouts[0].globalUv[{ dust3d::PositionKey(fixture.object.vertices[triangle[0]]), dust3d::PositionKey(fixture.object.vertices[triangle[1]]), dust3d::PositionKey(fixture.object.vertices[triangle[2]]) }] = fixture.atlas.triangleUvs[index];
    }
    rejects([&] { fixture.bake(inputs); }, dust3d::TextureBakeErrorCode::invalidInput);
}
void unsafeFoldGrid()
{
    Fixture fixture;
    dust3d::TextureBakeInputs inputs;
    auto& surface = inputs.surfaces[componentId];
    surface.wrinkleStrength = 1;
    dust3d::ClothFolds::Fold fold;
    fold.points = { { 1e12, 0, 0 }, { 1e12, 1, 0 } };
    fold.normals = { { 0, 0, 1 }, { 0, 0, 1 } };
    fold.width = 0.1;
    fold.depth = 0.01;
    surface.folds = { fold };
    rejects([&] { fixture.bake(inputs); }, dust3d::TextureBakeErrorCode::invalidInput);
}
void gradientQuantization()
{
    Fixture fixture;
    fixture.options.colorSize = 4;
    dust3d::UvAtlasGradient gradient;
    gradient.layoutIndex = 0;
    gradient.largeColor = dust3d::Color(1, 0, 0);
    gradient.smallColor = dust3d::Color(0, 0, 1);
    fixture.atlas.gradients = { gradient };
    const auto result = fixture.bake();
    require(result.color.pixels[0] == 223 && result.color.pixels[2] == 31, "Seam gradient samples a quantized 512-wide source with truncated channel interpolation");
    fixture.atlas.layouts[0].flipped = true;
    const auto transposed = fixture.bake();
    require(transposed.color.pixels[0] == transposed.color.pixels[12] && transposed.color.pixels[0] != transposed.color.pixels[48], "Flipped seam gradient transposes its source axis");
}
void invalidInputFamilies()
{
    for (int variant = 0; variant < 15; ++variant) {
        Fixture fixture;
        dust3d::TextureBakeInputs inputs;
        switch (variant) {
        case 0:
            fixture.atlas.layouts[0].color[0] = -0.1;
            break;
        case 1:
            fixture.options.maximumSpatialGridEntries = 0;
            break;
        case 2:
            fixture.options.maximumPayloadBytes = 256 * 1024 * 1024 + 1;
            break;
        case 3:
            fixture.object.triangles[0].resize(2);
            break;
        case 4:
            fixture.object.triangles[0][0] = fixture.object.vertices.size();
            break;
        case 5:
            fixture.atlas.triangleUvKnown[0] = 2;
            break;
        case 6:
            fixture.atlas.triangleUvs[0][0][0] = std::numeric_limits<double>::quiet_NaN();
            break;
        case 7:
            fixture.atlas.layouts[0].width = 2;
            break;
        case 8:
            fixture.atlas.layouts[0].globalUv.begin()->second[0][0] = 2;
            break;
        case 9:
            inputs.images[imageId] = {};
            break;
        case 10:
            inputs.materials[componentId].metallic = std::numeric_limits<double>::infinity();
            break;
        case 11:
            inputs.materials[componentId].roughness = 1.1;
            break;
        case 12:
            inputs.materials[componentId].emissive = -0.1;
            break;
        case 13:
            fixture.atlas.layouts[0].id = imageId;
            break;
        case 14:
            fixture.options.colorSize = 8193;
            break;
        }
        rejects([&] { fixture.bake(inputs); }, dust3d::TextureBakeErrorCode::invalidInput);
    }
    Fixture fixture;
    const std::vector<uint8_t> pixels(16, 255);
    dust3d::TextureBakeInputs inputs;
    inputs.images[imageId] = { 2, 2, 8, 15, pixels.data() };
    rejects([&] { fixture.bake(inputs); }, dust3d::TextureBakeErrorCode::invalidInput);
    inputs.images[imageId] = { 2, 2, std::numeric_limits<size_t>::max(), pixels.size(), pixels.data() };
    rejects([&] { fixture.bake(inputs); }, dust3d::TextureBakeErrorCode::invalidInput);
}
void invalidFoldFamilies()
{
    for (int variant = 0; variant < 4; ++variant) {
        Fixture fixture;
        dust3d::TextureBakeInputs inputs;
        auto& surface = inputs.surfaces[componentId];
        surface.wrinkleStrength = 1;
        dust3d::ClothFolds::Fold fold;
        fold.points = { { 0.5, 0, 0 }, { 0.5, 1, 0 } };
        fold.normals = { { 0, 0, 1 }, { 0, 0, 1 } };
        fold.width = 0.1;
        fold.depth = 0.01;
        if (variant == 0)
            fold.normals.pop_back();
        if (variant == 1)
            fold.points[0][0] = std::numeric_limits<double>::quiet_NaN();
        if (variant == 2)
            fold.normals[0] = { 0, 0, 0 };
        if (variant == 3)
            fixture.object.vertices[0] = { 0.2, 0, 0 };
        surface.folds = { fold };
        rejects([&] { fixture.bake(inputs); }, dust3d::TextureBakeErrorCode::invalidInput);
    }
}
void generatedFixtures(const std::string& directory)
{
    for (const std::string name : { "Capsule.ds3", "Sword.ds3", "RoundedCrossing.ds3" }) {
        std::ifstream stream(directory + "/" + name, std::ios::binary);
        require(stream.good(), "Texture fixture must exist");
        const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
        std::lock_guard<std::mutex> lock(dust3d::native::engineMutex());
        std::string warning;
        auto snapshot = dust3d::native::readDocument(bytes.data(), bytes.size(), warning);
        dust3d::MeshGenerator generator(snapshot.get());
        snapshot.release();
        generator.generate();
        std::unique_ptr<dust3d::Object> object(generator.takeObject());
        std::unique_ptr<dust3d::Snapshot> generated(generator.takeSnapshot());
        require(generator.isSuccessful() && object && generated, "Texture fixture generation must succeed");
        std::map<dust3d::Uuid, dust3d::UvAtlasComponent> components;
        dust3d::TextureBakeInputs inputs;
        for (const auto& entry : generated->components) {
            const dust3d::Uuid id(entry.first);
            const auto color = entry.second.find("color");
            if (color != entry.second.end())
                components[id].color = dust3d::Color(color->second);
            else
                components[id] = {};
            inputs.materials[id] = { 0.5, 0.25, 0.1 };
        }
        const auto atlas = dust3d::UvAtlasBuilder::build(*object, components);
        dust3d::TextureBakeOptions options;
        options.colorSize = options.normalSize = options.materialSize = 64;
        const auto first = dust3d::TextureBaker::bake(*object, atlas, inputs, options);
        const auto second = dust3d::TextureBaker::bake(*object, atlas, inputs, options);
        require(first.color.pixels == second.color.pixels && first.payloadBytes == second.payloadBytes, "Repeated real model bakes must be deterministic");
        require(first.tangents.size() == object->triangles.size(), "Real tangents must stay triangle aligned");
        size_t known = 0;
        for (size_t index = 0; index < first.tangents.size(); ++index) {
            const auto& tangent = first.tangents[index];
            if (!atlas.triangleUvKnown[index])
                require(!tangent.known, "Unmapped real geometry must not acquire fabricated tangents");
            if (tangent.known) {
                ++known;
                require(std::abs(tangent.direction.length() - 1) < 1e-9 && std::abs(tangent.handedness) == 1, "Real tangent frames must be finite and normalized");
            }
        }
        require(known > 0, "Real mapped geometry must resolve usable tangent frames");
        std::cout << name << ": " << known << " tangent frames, " << first.payloadBytes << " bake bytes\n";
    }
}
void boundaries()
{
    Fixture fixture;
    const auto payload = fixture.bake().payloadBytes;
    fixture.options.maximumPayloadBytes = payload;
    require(fixture.bake().payloadBytes == payload, "Exact payload budget must succeed");
    fixture.options.maximumPayloadBytes = payload - 1;
    rejects([&] { fixture.bake(); }, dust3d::TextureBakeErrorCode::limitExceeded);
    fixture.options.maximumPayloadBytes = 0;
    std::atomic_bool cancelled(true);
    fixture.options.cancellation = &cancelled;
    rejects([&] { fixture.bake(); }, dust3d::TextureBakeErrorCode::cancelled);
    fixture.options.cancellation = nullptr;
    fixture.options.colorSize = 0;
    rejects([&] { fixture.bake(); }, dust3d::TextureBakeErrorCode::invalidInput);
    fixture.options.colorSize = 8;
    fixture.atlas.triangleUvKnown.pop_back();
    rejects([&] { fixture.bake(); }, dust3d::TextureBakeErrorCode::invalidInput);
    fixture.atlas.triangleUvKnown.push_back(1);
    fixture.object.vertices[0] = { std::numeric_limits<double>::infinity(), 0, 0 };
    rejects([&] { fixture.bake(); }, dust3d::TextureBakeErrorCode::invalidInput);
}
}
int main(int argc, char* argv[])
{
    try {
        solidAndOwnership();
        materialsAndTransparency();
        imageOrientation();
        boundaries();
        surfaceDetails();
        malformedSurfaceInputs();
        gradientQuantization();
        unsafeProceduralCoordinates();
        unsafeFoldGrid();
        invalidInputFamilies();
        invalidFoldFamilies();
        require(argc == 2, "Expected fixture directory");
        generatedFixtures(argv[1]);
        std::cout << "Texture bake contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
