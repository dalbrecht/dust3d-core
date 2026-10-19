// SPDX-License-Identifier: MIT
#include "EngineInternal.h"
#include "GlbReader.h"
#include <cmath>
#include <cstring>
#include <functional>
#include <iostream>
#include <json/json.hpp>
#include <limits>
#include <stdexcept>

namespace {
void require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}
void appendWord(std::vector<uint8_t>& bytes, uint32_t value)
{
    for (size_t index = 0; index < 4; ++index)
        bytes.push_back(static_cast<uint8_t>(value >> (index * 8)));
}
void appendFloat(std::vector<uint8_t>& bytes, float value)
{
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    appendWord(bytes, bits);
}
std::vector<uint8_t> fixture(std::string json, std::vector<uint8_t> binary)
{
    while (json.size() % 4)
        json += ' ';
    while (binary.size() % 4)
        binary.push_back(0);
    std::vector<uint8_t> bytes;
    appendWord(bytes, 0x46546c67);
    appendWord(bytes, 2);
    appendWord(bytes, static_cast<uint32_t>(28 + json.size() + binary.size()));
    appendWord(bytes, static_cast<uint32_t>(json.size()));
    appendWord(bytes, 0x4e4f534a);
    bytes.insert(bytes.end(), json.begin(), json.end());
    appendWord(bytes, static_cast<uint32_t>(binary.size()));
    appendWord(bytes, 0x004e4942);
    bytes.insert(bytes.end(), binary.begin(), binary.end());
    return bytes;
}
std::string triangleJson()
{
    return R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":36}],"bufferViews":[{"buffer":0,"byteLength":36}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0}}]}]})";
}
std::vector<uint8_t> triangleBinary(bool strided = false)
{
    std::vector<uint8_t> bytes;
    for (const auto& point : std::array<std::array<float, 3>, 3> { { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } } }) {
        for (float value : point)
            appendFloat(bytes, value);
        if (strided)
            appendFloat(bytes, 99);
    }
    return bytes;
}
void replace(std::string& text, const std::string& old, const std::string& replacement)
{
    auto offset = text.find(old);
    require(offset != std::string::npos, "Invalid test mutation");
    text.replace(offset, old.size(), replacement);
}
void rejects(const std::vector<uint8_t>& bytes, Dust3DStatus status)
{
    try {
        dust3d::native::readGlb(bytes.data(), bytes.size());
    } catch (const dust3d::native::DocumentError& error) {
        require(error.status() == status, error.what());
        require(std::strlen(error.what()) > 0, "Missing original diagnostic");
        return;
    }
    throw std::runtime_error("Invalid GLB accepted");
}

struct AttributedFixture {
    nlohmann::json document = nlohmann::json::parse(triangleJson());
    std::vector<uint8_t> binary = triangleBinary();
    size_t addView(const std::vector<uint8_t>& payload)
    {
        while (binary.size() % 4)
            binary.push_back(0);
        const size_t index = document["bufferViews"].size();
        document["bufferViews"].push_back({ { "buffer", 0 }, { "byteOffset", binary.size() }, { "byteLength", payload.size() } });
        binary.insert(binary.end(), payload.begin(), payload.end());
        return index;
    }
    void addFloats(const char* attribute, const char* type, std::initializer_list<float> values)
    {
        std::vector<uint8_t> payload;
        for (float value : values)
            appendFloat(payload, value);
        const size_t index = document["accessors"].size();
        document["accessors"].push_back({ { "bufferView", addView(payload) }, { "componentType", 5126 }, { "count", 3 }, { "type", type } });
        document["meshes"][0]["primitives"][0]["attributes"][attribute] = index;
    }
    std::vector<uint8_t> bytes()
    {
        document["buffers"][0]["byteLength"] = binary.size();
        return fixture(document.dump(), binary);
    }
};
AttributedFixture texturedFixture()
{
    AttributedFixture source;
    source.addFloats("NORMAL", "VEC3", { 0, 0, 1, 0, 0, 1, 0, 0, 1 });
    source.addFloats("TEXCOORD_0", "VEC2", { 0, 0, 1, 0, 0, 1 });
    const auto colorView = source.addView({ 255, 0, 0, 128, 0, 255, 0, 255, 0, 0, 255, 64 });
    const auto colorAccessor = source.document["accessors"].size();
    source.document["accessors"].push_back({ { "bufferView", colorView }, { "componentType", 5121 }, { "count", 3 }, { "type", "VEC4" }, { "normalized", true } });
    source.document["meshes"][0]["primitives"][0]["attributes"]["COLOR_0"] = colorAccessor;
    const std::vector<uint8_t> encodedImage { 1, 2, 3, 4 };
    const auto imageView = source.addView(encodedImage);
    source.document["images"] = nlohmann::json::array({ { { "bufferView", imageView }, { "mimeType", "image/png" } } });
    source.document["textures"] = nlohmann::json::array({ { { "source", 0 }, { "sampler", 0 } } });
    source.document["samplers"] = nlohmann::json::array({ { { "wrapS", 33071 }, { "wrapT", 33648 }, { "magFilter", 9728 }, { "minFilter", 9984 } } });
    source.document["materials"] = nlohmann::json::array({ { { "pbrMetallicRoughness", { { "baseColorTexture", { { "index", 0 } } }, { "metallicFactor", 0.25 }, { "roughnessFactor", 0.75 } } },
        { "alphaMode", "MASK" }, { "alphaCutoff", 0.3 }, { "doubleSided", true }, { "emissiveFactor", { 0.1, 0.2, 0.3 } } } });
    source.document["meshes"][0]["primitives"][0]["material"] = 0;
    return source;
}
void testCoincidentUvTrianglesRetainIndependentCorners()
{
    auto source = texturedFixture();
    const auto original = source.document["meshes"][0]["primitives"][0];
    source.addFloats("TEXCOORD_0", "VEC2", { -2, 3, 4, 5, 6, 7 });
    source.document["meshes"][0]["primitives"].push_back(original);
    const auto bytes = source.bytes();
    const auto result = dust3d::native::readGlb(bytes.data(), bytes.size());
    require(result.model.faces.size() == 2 && result.model.triangleCornerUvs.size() == 2,
        "Coincident GLB triangles lost parallel UV records");
    require(result.model.triangleCornerUvs[0] && result.model.triangleCornerUvs[1], "Textured GLB corners are unmapped");
    require(result.model.triangleCornerUvs[0]->at(0) == dust3d::Vector2(-2, 3)
            && result.model.triangleCornerUvs[1]->at(0) == dust3d::Vector2(0, 0),
        "Quantized position lookup collapsed distinct authored UVs");
}
void testAttributesAndIndependentOwnership()
{
    auto source = texturedFixture();
    const std::vector<uint8_t> encodedImage { 1, 2, 3, 4 };
    auto bytes = source.bytes();
    const auto result = dust3d::native::readGlb(bytes.data(), bytes.size());
    std::fill(bytes.begin(), bytes.end(), 0);
    require(result.model.vertices[1].x() == 1 && result.model.vertexNormals[2].z() == 1, "Geometry borrows input bytes");
    require(result.model.triangleUvs.size() == 1 && result.model.triangleUvs.begin()->second[1].x() == 1, "UVs missing or changed");
    require(result.model.vertexColors.size() == 3 && std::abs(result.model.vertexColors[0].alpha() - 128.0 / 255) < 1e-10, "Normalized color alpha lost");
    require(result.image && result.image->bytes == encodedImage && result.image->mimeType == "image/png", "Encoded image ownership failed");
    require(result.metallic == 0.25 && result.roughness == 0.75, "Material scalars lost");
    require(result.alphaMode == dust3d::native::GlbAlphaMode::Mask && result.alphaCutoff == 0.3 && result.isDoubleSided, "Material visibility properties lost");
    require(result.emissive == std::array<double, 3> { 0.1, 0.2, 0.3 }, "Emissive factor lost");
    require(result.sampler && result.sampler->wrapS == 33071 && result.sampler->wrapT == 33648 && result.sampler->magnification == 9728 && result.sampler->minification == 9984, "Sampler state lost");
    source.document["samplers"][0]["wrapS"] = 123;
    rejects(source.bytes(), DUST3D_STATUS_INVALID_DOCUMENT);
    source.document["samplers"][0]["wrapS"] = 33071;
    source.document["materials"][0]["alphaMode"] = "Unrecognized";
    rejects(source.bytes(), DUST3D_STATUS_INVALID_DOCUMENT);
}
void testAccessorAlignmentAndIndexStride()
{
    AttributedFixture source;
    source.binary.insert(source.binary.begin(), 0);
    source.document["bufferViews"][0]["byteOffset"] = 1;
    rejects(source.bytes(), DUST3D_STATUS_INVALID_DOCUMENT);
    source = AttributedFixture {};
    const auto indexView = source.addView({ 0, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0 });
    source.document["bufferViews"][indexView]["byteStride"] = 4;
    source.document["accessors"].push_back({ { "bufferView", indexView }, { "componentType", 5121 }, { "count", 3 }, { "type", "SCALAR" } });
    source.document["meshes"][0]["primitives"][0]["indices"] = 1;
    rejects(source.bytes(), DUST3D_STATUS_INVALID_DOCUMENT);
}

void testRejectedDocumentMutations()
{
    using Json = nlohmann::json;
    struct Mutation {
        const char* pointer;
        Json value;
        Dust3DStatus status;
    };
    const std::vector<Mutation> mutations {
        { "/asset/version", "1.0", DUST3D_STATUS_UNSUPPORTED_ASSET },
        { "/asset/minVersion", "2.1", DUST3D_STATUS_UNSUPPORTED_ASSET },
        { "/buffers", Json::array(), DUST3D_STATUS_UNSUPPORTED_ASSET },
        { "/buffers/0/byteLength", 0, DUST3D_STATUS_INVALID_DOCUMENT },
        { "/buffers/0/byteLength", 1000, DUST3D_STATUS_INVALID_DOCUMENT },
        { "/bufferViews/0/buffer", 1, DUST3D_STATUS_INVALID_DOCUMENT },
        { "/bufferViews/0/byteOffset", 100, DUST3D_STATUS_INVALID_DOCUMENT },
        { "/bufferViews/0", nullptr, DUST3D_STATUS_INVALID_DOCUMENT },
        { "/accessors/0/type", "VEC2", DUST3D_STATUS_INVALID_DOCUMENT },
        { "/accessors/0/count", 0, DUST3D_STATUS_INVALID_DOCUMENT },
        { "/accessors/0/count", -1, DUST3D_STATUS_INVALID_DOCUMENT },
        { "/accessors/0/count", "three", DUST3D_STATUS_INVALID_DOCUMENT },
        { "/accessors/0/normalized", "yes", DUST3D_STATUS_INVALID_DOCUMENT },
        { "/accessors/0/normalized", true, DUST3D_STATUS_UNSUPPORTED_ASSET },
        { "/accessors/0/componentType", 5120, DUST3D_STATUS_UNSUPPORTED_ASSET },
        { "/meshes", Json::array(), DUST3D_STATUS_UNSUPPORTED_ASSET },
        { "/meshes/0/weights", Json::array(), DUST3D_STATUS_UNSUPPORTED_ASSET },
        { "/meshes/0/primitives", Json::array(), DUST3D_STATUS_INVALID_DOCUMENT },
        { "/meshes/0/primitives/0/mode", 1, DUST3D_STATUS_UNSUPPORTED_ASSET },
        { "/meshes/0/primitives/0/targets", Json::array(), DUST3D_STATUS_UNSUPPORTED_ASSET },
        { "/meshes/0/primitives/0/attributes/JOINTS_0", 0, DUST3D_STATUS_UNSUPPORTED_ASSET },
        { "/meshes/0/primitives/0/attributes/NORMAL", 0, DUST3D_STATUS_INVALID_DOCUMENT },
        { "/scene", 0, DUST3D_STATUS_INVALID_DOCUMENT },
        { "/nodes", Json::array({ { { "mesh", 0 } }, { { "mesh", 0 } } }), DUST3D_STATUS_UNSUPPORTED_ASSET },
        { "/nodes", Json::array({ { { "mesh", 1 } } }), DUST3D_STATUS_UNSUPPORTED_ASSET },
        { "/nodes", Json::array({ { { "mesh", 0 }, { "translation", Json::array({ 0, 0 }) } } }), DUST3D_STATUS_INVALID_DOCUMENT },
        { "/nodes", Json::array({ { { "mesh", 0 }, { "translation", Json::array({ "bad", 0, 0 }) } } }), DUST3D_STATUS_INVALID_DOCUMENT },
        { "/nodes", Json::array({ { { "mesh", 0 }, { "translation", Json::array({ 1e20, 0, 0 }) } } }), DUST3D_STATUS_INVALID_DOCUMENT },
    };
    for (const auto& mutation : mutations) {
        auto document = Json::parse(triangleJson());
        document[Json::json_pointer(mutation.pointer)] = mutation.value;
        try {
            rejects(fixture(document.dump(), triangleBinary()), mutation.status);
        } catch (const std::exception& cause) {
            throw std::runtime_error(std::string(mutation.pointer) + ": " + cause.what());
        }
    }
    auto document = Json::parse(triangleJson());
    document["nodes"] = Json::array({ { { "mesh", 0 } } });
    document["scenes"] = Json::array({ { { "nodes", Json::array({ 0 }) } }, { { "nodes", Json::array({ 0 }) } } });
    rejects(fixture(document.dump(), triangleBinary()), DUST3D_STATUS_UNSUPPORTED_ASSET);
    document["scenes"].erase(document["scenes"].begin() + 1);
    auto bytes = fixture(document.dump(), triangleBinary());
    const auto result = dust3d::native::readGlb(bytes.data(), bytes.size());
    require(result.model.faces.size() == 1, "Single identity scene rejected");
    auto json = triangleJson();
    replace(json, "\"buffer\":0", "\"buffer\":-0");
    bytes = fixture(json, triangleBinary());
    require(dust3d::native::readGlb(bytes.data(), bytes.size()).model.faces.size() == 1, "Nonnegative signed zero rejected");
}
void testMixedPrimitiveColors()
{
    AttributedFixture source;
    source.addFloats("COLOR_0", "VEC3", { 1, 0, 0, 0, 1, 0, 0, 0, 1 });
    source.document["meshes"][0]["primitives"].push_back(source.document["meshes"][0]["primitives"][0]);
    source.document["meshes"][0]["primitives"][1]["attributes"].erase("COLOR_0");
    auto bytes = source.bytes();
    const auto result = dust3d::native::readGlb(bytes.data(), bytes.size());
    require(result.model.vertexColors.size() == 6, "Mixed color arrays lost vertex alignment");
    require(result.model.vertexColors[0].red() == 1 && result.model.vertexColors[0].green() == 0, "Colored primitive changed");
    require(result.model.vertexColors[3].red() == 1 && result.model.vertexColors[3].green() == 1 && result.model.vertexColors[3].blue() == 1, "Uncolored primitive lacks neutral defaults");
}
void testMaterialValidation()
{
    using Json = nlohmann::json;
    AttributedFixture source;
    source.document["materials"] = Json::array({ { { "pbrMetallicRoughness", Json::object() } } });
    source.document["meshes"][0]["primitives"][0]["material"] = 0;
    const auto baseline = source.document;
    const std::vector<std::pair<const char*, Json>> malformed {
        { "/materials/0/alphaCutoff", -1 },
        { "/materials/0/doubleSided", 1 },
        { "/materials/0/emissiveFactor", { 1, 0 } },
        { "/materials/0/pbrMetallicRoughness/metallicFactor", -0.1 },
        { "/materials/0/pbrMetallicRoughness/roughnessFactor", 1.1 },
        { "/materials/0/pbrMetallicRoughness/baseColorFactor", { 1, 0, 0 } },
        { "/materials/0/pbrMetallicRoughness/baseColorFactor", { 1, -0.1, 0, 1 } },
    };
    for (const auto& mutation : malformed) {
        source.document = baseline;
        source.document[Json::json_pointer(mutation.first)] = mutation.second;
        rejects(source.bytes(), DUST3D_STATUS_INVALID_DOCUMENT);
    }
    for (const char* key : { "normalTexture", "occlusionTexture", "emissiveTexture" }) {
        source.document = baseline;
        source.document["materials"][0][key] = Json::object();
        rejects(source.bytes(), DUST3D_STATUS_UNSUPPORTED_ASSET);
    }
    source.document = baseline;
    source.document["materials"][0]["pbrMetallicRoughness"]["metallicRoughnessTexture"] = Json::object();
    rejects(source.bytes(), DUST3D_STATUS_UNSUPPORTED_ASSET);
    for (const char* mode : { "OPAQUE", "BLEND" }) {
        source.document = baseline;
        source.document["materials"][0]["alphaMode"] = mode;
        auto bytes = source.bytes();
        const auto result = dust3d::native::readGlb(bytes.data(), bytes.size());
        require((result.alphaMode == dust3d::native::GlbAlphaMode::Blend) == (std::string(mode) == "BLEND"), "Alpha mode changed");
    }
    source.document["materials"][0].erase("pbrMetallicRoughness");
    auto bytes = source.bytes();
    require(dust3d::native::readGlb(bytes.data(), bytes.size()).roughness == 1, "Default material changed");
}

void testTextureAndAttributeBoundaries()
{
    using Json = nlohmann::json;
    struct Mutation {
        const char* pointer;
        Json value;
        Dust3DStatus status;
    };
    const std::vector<Mutation> mutations {
        { "/materials/0/pbrMetallicRoughness/baseColorTexture/texCoord", 1, DUST3D_STATUS_UNSUPPORTED_ASSET },
        { "/images/0/uri", "https://example.invalid/texture.png", DUST3D_STATUS_UNSUPPORTED_ASSET },
        { "/images/0/mimeType", "application/octet-stream", DUST3D_STATUS_UNSUPPORTED_ASSET },
        { "/samplers/0/magFilter", 0, DUST3D_STATUS_INVALID_DOCUMENT },
        { "/samplers/0/minFilter", 0, DUST3D_STATUS_INVALID_DOCUMENT },
        { "/accessors/1/count", 2, DUST3D_STATUS_INVALID_DOCUMENT },
        { "/accessors/2/count", 2, DUST3D_STATUS_INVALID_DOCUMENT },
        { "/accessors/3/count", 2, DUST3D_STATUS_INVALID_DOCUMENT },
        { "/accessors/3/type", "MAT4", DUST3D_STATUS_INVALID_DOCUMENT },
        { "/accessors/3/normalized", false, DUST3D_STATUS_UNSUPPORTED_ASSET },
    };
    for (const auto& mutation : mutations) {
        auto source = texturedFixture();
        source.document[Json::json_pointer(mutation.pointer)] = mutation.value;
        try {
            rejects(source.bytes(), mutation.status);
        } catch (const std::exception& cause) {
            throw std::runtime_error(std::string(mutation.pointer) + ": " + cause.what());
        }
    }
    auto source = texturedFixture();
    const auto imageView = source.document["images"][0]["bufferView"].get<size_t>();
    source.document["bufferViews"][imageView]["byteStride"] = 4;
    rejects(source.bytes(), DUST3D_STATUS_INVALID_DOCUMENT);
    source = texturedFixture();
    source.document["meshes"][0]["primitives"][0]["attributes"].erase("TEXCOORD_0");
    rejects(source.bytes(), DUST3D_STATUS_INVALID_DOCUMENT);
    source = texturedFixture();
    source.document["meshes"][0]["primitives"].push_back(source.document["meshes"][0]["primitives"][0]);
    const auto coincidentBytes = source.bytes();
    const auto coincident = dust3d::native::readGlb(coincidentBytes.data(), coincidentBytes.size());
    require(coincident.model.triangleCornerUvs.size() == 2
            && coincident.model.triangleCornerUvs[0] == coincident.model.triangleCornerUvs[1],
        "Coincident identical UV triangles must retain both parallel records");
    source.document["meshes"][0]["primitives"][1]["material"] = 1;
    rejects(source.bytes(), DUST3D_STATUS_UNSUPPORTED_ASSET);
    source = texturedFixture();
    source.document["textures"][0].erase("sampler");
    auto bytes = source.bytes();
    const auto result = dust3d::native::readGlb(bytes.data(), bytes.size());
    require(result.sampler && result.sampler->wrapS == 10497 && !result.sampler->magnification, "Unspecified sampler defaults were invented");
    source = AttributedFixture {};
    source.addFloats("COLOR_0", "VEC3", { 1, 0, 0, 0, 1, 0, 0, 0, 1 });
    bytes = source.bytes();
    require(dust3d::native::readGlb(bytes.data(), bytes.size()).model.vertexColors[2].blue() == 1, "Float RGB colors lost");
    source.binary.back() = 0x7f;
    rejects(source.bytes(), DUST3D_STATUS_INVALID_DOCUMENT);
    source = AttributedFixture {};
    source.addFloats("COLOR_0", "VEC3", { 1.1f, 0, 0, 0, 1, 0, 0, 0, 1 });
    rejects(source.bytes(), DUST3D_STATUS_INVALID_DOCUMENT);
    source = AttributedFixture {};
    auto packedColorView = source.addView({ 255, 0, 0, 0, 255, 0, 0, 0, 255 });
    source.document["accessors"].push_back({ { "bufferView", packedColorView }, { "componentType", 5121 }, { "count", 3 }, { "type", "VEC3" }, { "normalized", true } });
    source.document["meshes"][0]["primitives"][0]["attributes"]["COLOR_0"] = 1;
    rejects(source.bytes(), DUST3D_STATUS_INVALID_DOCUMENT);
    source = AttributedFixture {};
    std::vector<uint8_t> colors;
    for (size_t index = 0; index < 12; ++index) {
        colors.push_back(255);
        colors.push_back(255);
    }
    const auto colorView = source.addView(colors);
    source.document["accessors"].push_back({ { "bufferView", colorView }, { "componentType", 5123 }, { "count", 3 }, { "type", "VEC4" }, { "normalized", true } });
    source.document["meshes"][0]["primitives"][0]["attributes"]["COLOR_0"] = 1;
    bytes = source.bytes();
    require(dust3d::native::readGlb(bytes.data(), bytes.size()).model.vertexColors[1].alpha() == 1, "UInt16 RGBA normalization failed");
}
void testIndexWidthsAndInvalidTopology()
{
    for (size_t component : { 5123, 5125 }) {
        AttributedFixture source;
        std::vector<uint8_t> indices;
        for (uint32_t value : { 0, 1, 2 }) {
            if (component == 5123) {
                indices.push_back(static_cast<uint8_t>(value));
                indices.push_back(0);
            } else {
                appendWord(indices, value);
            }
        }
        const auto view = source.addView(indices);
        source.document["accessors"].push_back({ { "bufferView", view }, { "componentType", component }, { "count", 3 }, { "type", "SCALAR" } });
        source.document["meshes"][0]["primitives"][0]["indices"] = 1;
        auto bytes = source.bytes();
        require(dust3d::native::readGlb(bytes.data(), bytes.size()).model.faces[0] == std::vector<size_t> { 0, 1, 2 }, "Index width changed geometry");
        source.document["accessors"][1]["normalized"] = true;
        rejects(source.bytes(), DUST3D_STATUS_INVALID_DOCUMENT);
    }
    AttributedFixture source;
    auto view = source.addView({ 0, 0, 2 });
    source.document["accessors"].push_back({ { "bufferView", view }, { "componentType", 5121 }, { "count", 3 }, { "type", "SCALAR" } });
    source.document["meshes"][0]["primitives"][0]["indices"] = 1;
    rejects(source.bytes(), DUST3D_STATUS_INVALID_DOCUMENT);
    source.document["accessors"][1]["count"] = 2;
    rejects(source.bytes(), DUST3D_STATUS_INVALID_DOCUMENT);
    source = AttributedFixture {};
    view = source.addView({ 0, 1, 2, 0, 2, 1 });
    source.document["accessors"].push_back({ { "bufferView", view }, { "componentType", 5121 }, { "count", 6 }, { "type", "SCALAR" } });
    source.document["meshes"][0]["primitives"][0]["indices"] = 1;
    auto oppositeBytes = source.bytes();
    const auto opposite = dust3d::native::readGlb(oppositeBytes.data(), oppositeBytes.size());
    require(opposite.model.vertexNormals.size() == 6 && opposite.model.vertexNormals[0].z() == 1 && opposite.model.vertexNormals[3].z() == -1, "Opposite flat faces must not cancel normals");
}

void testAggregateAndImageBudgets()
{
    AttributedFixture source;
    source.binary.clear();
    const auto triangle = triangleBinary();
    for (size_t count = 0; count < 41667; ++count)
        source.binary.insert(source.binary.end(), triangle.begin(), triangle.end());
    source.document["bufferViews"][0]["byteLength"] = source.binary.size();
    source.document["accessors"][0]["count"] = 125001;
    source.document["meshes"][0]["primitives"].push_back(source.document["meshes"][0]["primitives"][0]);
    rejects(source.bytes(), DUST3D_STATUS_LIMIT_EXCEEDED);
    source = AttributedFixture {};
    source.addFloats("NORMAL", "VEC3", { 0, 0, 1, 0, 0, 1, 0, 0, 1 });
    std::vector<uint8_t> indices;
    for (size_t count = 0; count < 250001; ++count)
        indices.insert(indices.end(), { 0, 1, 2 });
    auto view = source.addView(indices);
    source.document["accessors"].push_back({ { "bufferView", view }, { "componentType", 5121 }, { "count", indices.size() }, { "type", "SCALAR" } });
    source.document["meshes"][0]["primitives"][0]["indices"] = 2;
    source.document["meshes"][0]["primitives"].push_back(source.document["meshes"][0]["primitives"][0]);
    rejects(source.bytes(), DUST3D_STATUS_LIMIT_EXCEEDED);
    source.document["meshes"][0]["primitives"].erase(source.document["meshes"][0]["primitives"].begin() + 1);
    source.document["meshes"][0]["primitives"][0]["attributes"].erase("NORMAL");
    rejects(source.bytes(), DUST3D_STATUS_LIMIT_EXCEEDED);
    source = texturedFixture();
    view = source.addView(std::vector<uint8_t>(16 * 1024 * 1024 + 1, 0));
    source.document["images"][0]["bufferView"] = view;
    rejects(source.bytes(), DUST3D_STATUS_LIMIT_EXCEEDED);
    source = AttributedFixture {};
    source.document["extras"] = std::vector<int>(100001, 0);
    rejects(source.bytes(), DUST3D_STATUS_LIMIT_EXCEEDED);
}
void testFlatNormalsAndReservedIndices()
{
    AttributedFixture source;
    // Put a second triangle in a perpendicular plane, sharing its first edge.
    for (float value : { 0.0f, 0.0f, 1.0f })
        appendFloat(source.binary, value);
    source.document["bufferViews"][0]["byteLength"] = 48;
    source.document["accessors"][0]["count"] = 4;
    auto view = source.addView({ 0, 1, 2, 0, 3, 1 });
    source.document["accessors"].push_back({ { "bufferView", view }, { "componentType", 5121 }, { "count", 6 }, { "type", "SCALAR" } });
    source.document["meshes"][0]["primitives"][0]["indices"] = 1;
    std::vector<uint8_t> uvBytes;
    for (float value : { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f })
        appendFloat(uvBytes, value);
    auto uvView = source.addView(uvBytes);
    source.document["accessors"].push_back({ { "bufferView", uvView }, { "componentType", 5126 }, { "count", 4 }, { "type", "VEC2" } });
    source.document["meshes"][0]["primitives"][0]["attributes"]["TEXCOORD_0"] = 2;
    const auto colorView = source.addView({ 255, 0, 0, 255, 0, 255, 0, 128, 0, 0, 255, 255, 255, 255, 0, 64 });
    source.document["accessors"].push_back({ { "bufferView", colorView }, { "componentType", 5121 }, { "count", 4 }, { "type", "VEC4" }, { "normalized", true } });
    source.document["meshes"][0]["primitives"][0]["attributes"]["COLOR_0"] = 3;
    auto bytes = source.bytes();
    const auto result = dust3d::native::readGlb(bytes.data(), bytes.size());
    require(result.model.vertices.size() == 6 && result.model.vertexNormals.size() == 6, "Flat normals require independent corners");
    for (size_t i = 0; i < 3; ++i) {
        require(result.model.vertexNormals[i].z() == 1 && result.model.vertexNormals[i].y() == 0, "First face normal was smoothed");
        require(result.model.vertexNormals[i + 3].y() == 1 && result.model.vertexNormals[i + 3].z() == 0, "Second face normal was smoothed");
    }
    require(result.model.triangleUvs.size() == 2 && result.model.vertexColors.size() == 6, "Flat expansion lost UV/color alignment");
    require(result.model.vertexColors[4].red() == 1 && result.model.vertexColors[4].green() == 1 && std::abs(result.model.vertexColors[4].alpha() - 64.0 / 255) < 1e-10, "Flat expansion changed corner color");
    for (size_t component : { 5121, 5123 }) {
        source = AttributedFixture {};
        source.binary.clear();
        const size_t count = component == 5121 ? 256 : 65536;
        for (size_t i = 0; i < count; ++i) {
            appendFloat(source.binary, i == 1 ? 1 : 0);
            appendFloat(source.binary, i > 1 ? 1 : 0);
            appendFloat(source.binary, 0);
        }
        source.document["accessors"][0]["count"] = count;
        source.document["bufferViews"][0]["byteLength"] = source.binary.size();
        std::vector<uint8_t> normals;
        for (size_t i = 0; i < count; ++i) {
            appendFloat(normals, 0);
            appendFloat(normals, 0);
            appendFloat(normals, 1);
        }
        auto normalView = source.addView(normals);
        source.document["accessors"].push_back({ { "bufferView", normalView }, { "componentType", 5126 }, { "count", count }, { "type", "VEC3" } });
        source.document["meshes"][0]["primitives"][0]["attributes"]["NORMAL"] = 1;
        std::vector<uint8_t> indices = component == 5121 ? std::vector<uint8_t> { 0, 1, 255 } : std::vector<uint8_t> { 0, 0, 1, 0, 255, 255 };
        view = source.addView(indices);
        source.document["accessors"].push_back({ { "bufferView", view }, { "componentType", component }, { "count", 3 }, { "type", "SCALAR" } });
        source.document["meshes"][0]["primitives"][0]["indices"] = 2;
        rejects(source.bytes(), DUST3D_STATUS_INVALID_DOCUMENT);
    }
}
void testContainerBoundaries()
{
    auto bytes = fixture(triangleJson(), triangleBinary());
    const auto baseline = bytes;
    for (size_t offset : { 0, 4, 16 }) {
        bytes = baseline;
        bytes[offset] = 0;
        rejects(bytes, DUST3D_STATUS_INVALID_DOCUMENT);
    }
    bytes = baseline;
    bytes.push_back(0);
    bytes[8] = static_cast<uint8_t>(bytes.size());
    bytes[9] = static_cast<uint8_t>(bytes.size() >> 8);
    rejects(bytes, DUST3D_STATUS_INVALID_DOCUMENT);
    const size_t jsonLength = size_t(baseline[12]) | (size_t(baseline[13]) << 8) | (size_t(baseline[14]) << 16) | (size_t(baseline[15]) << 24);
    const size_t binaryTypeOffset = 20 + jsonLength + 4;
    bytes = baseline;
    bytes[binaryTypeOffset] = 0;
    rejects(bytes, DUST3D_STATUS_INVALID_DOCUMENT);
    bytes = baseline;
    bytes[binaryTypeOffset] = 0x4a;
    bytes[binaryTypeOffset + 1] = 0x53;
    bytes[binaryTypeOffset + 2] = 0x4f;
    bytes[binaryTypeOffset + 3] = 0x4e;
    rejects(bytes, DUST3D_STATUS_INVALID_DOCUMENT);
    bytes = fixture("{broken JSON}", triangleBinary());
    rejects(bytes, DUST3D_STATUS_INVALID_DOCUMENT);
    bytes = fixture(std::string(1024 * 1024 + 4, ' '), triangleBinary());
    rejects(bytes, DUST3D_STATUS_LIMIT_EXCEEDED);
    bytes.assign(64 * 1024 * 1024 + 1, 0);
    rejects(bytes, DUST3D_STATUS_LIMIT_EXCEEDED);
}
void testMalformedLayouts()
{
    auto document = nlohmann::json::parse(triangleJson());
    document["bufferViews"][0]["byteOffset"] = 1;
    document["bufferViews"][0]["byteLength"] = 35;
    rejects(fixture(document.dump(), triangleBinary()), DUST3D_STATUS_INVALID_DOCUMENT);
    document = nlohmann::json::parse(triangleJson());
    document["accessors"][0]["sparse"] = nlohmann::json::object();
    rejects(fixture(document.dump(), triangleBinary()), DUST3D_STATUS_UNSUPPORTED_ASSET);
    document = nlohmann::json::parse(triangleJson());
    document["bufferViews"][0]["byteStride"] = 13;
    rejects(fixture(document.dump(), triangleBinary()), DUST3D_STATUS_INVALID_DOCUMENT);
    document = nlohmann::json::parse(triangleJson());
    document["accessors"][0]["byteOffset"] = std::numeric_limits<uint64_t>::max();
    rejects(fixture(document.dump(), triangleBinary()), DUST3D_STATUS_INVALID_DOCUMENT);
    document = nlohmann::json::parse(triangleJson());
    document["meshes"][0]["primitives"][0]["attributes"]["POSITION"] = 100;
    rejects(fixture(document.dump(), triangleBinary()), DUST3D_STATUS_INVALID_DOCUMENT);
    auto bytes = fixture(triangleJson(), triangleBinary());
    bytes[12] = 255;
    rejects(bytes, DUST3D_STATUS_INVALID_DOCUMENT);
}
}
int main()
{
    try {
        testAttributesAndIndependentOwnership();
        testCoincidentUvTrianglesRetainIndependentCorners();
        testMalformedLayouts();
        testAccessorAlignmentAndIndexStride();
        testRejectedDocumentMutations();
        testMaterialValidation();
        testMixedPrimitiveColors();
        testTextureAndAttributeBoundaries();
        testIndexWidthsAndInvalidTopology();
        testFlatNormalsAndReservedIndices();
        testContainerBoundaries();
        testAggregateAndImageBudgets();
        using namespace dust3d::native;
        auto bytes = fixture(triangleJson(), triangleBinary());
        auto result = readGlb(bytes.data(), bytes.size());
        require(result.model.vertices.size() == 3 && result.model.faces.size() == 1, "Triangle geometry missing");
        require(result.model.vertexColors.empty(), "Absent vertex colors must not override part colors");
        require(result.model.faces[0] == std::vector<size_t> { 0, 1, 2 }, "Triangle winding changed");
        require(result.model.vertexNormals.size() == 3 && std::abs(result.model.vertexNormals[0].z() - 1) < 1e-8, "Missing normals were not generated");
        auto json = triangleJson();
        replace(json, "\"byteLength\":36", "\"byteLength\":48");
        replace(json, "\"buffer\":0,\"byteLength\":36", "\"buffer\":0,\"byteLength\":48,\"byteStride\":16");
        bytes = fixture(json, triangleBinary(true));
        result = readGlb(bytes.data(), bytes.size());
        require(result.model.vertices[1].x() == 1 && result.model.vertices[2].y() == 1, "Vertex stride ignored");
        for (size_t length = 0; length < 28; ++length)
            rejects(std::vector<uint8_t>(bytes.begin(), bytes.begin() + length), DUST3D_STATUS_INVALID_DOCUMENT);
        auto damaged = bytes;
        damaged[8] = 0;
        rejects(damaged, DUST3D_STATUS_INVALID_DOCUMENT);
        json = triangleJson();
        replace(json, "\"count\":3", "\"count\":18446744073709551615");
        rejects(fixture(json, triangleBinary()), DUST3D_STATUS_LIMIT_EXCEEDED);
        json = triangleJson();
        replace(json, "\"byteLength\":36}", "\"byteLength\":36,\"uri\":\"file:///secret\"}");
        rejects(fixture(json, triangleBinary()), DUST3D_STATUS_UNSUPPORTED_ASSET);
        json = triangleJson();
        replace(json, "\"asset\":", "\"nodes\":[{\"mesh\":0,\"translation\":[1,0,0]}],\"asset\":");
        rejects(fixture(json, triangleBinary()), DUST3D_STATUS_UNSUPPORTED_ASSET);
        auto nonfinite = triangleBinary();
        std::vector<uint8_t> infinity;
        appendFloat(infinity, std::numeric_limits<float>::infinity());
        std::copy(infinity.begin(), infinity.end(), nonfinite.begin());
        rejects(fixture(triangleJson(), nonfinite), DUST3D_STATUS_INVALID_DOCUMENT);
        json = triangleJson();
        replace(json, "\"asset\":", "\"asset\":{},\"asset\":");
        rejects(fixture(json, triangleBinary()), DUST3D_STATUS_INVALID_DOCUMENT);
        auto document = nlohmann::json::parse(triangleJson());
        document["scene"] = 1;
        document["scenes"] = nlohmann::json::array({ { { "nodes", nlohmann::json::array({ 0 }) } } });
        document["nodes"] = nlohmann::json::array({ { { "mesh", 0 } } });
        rejects(fixture(document.dump(), triangleBinary()), DUST3D_STATUS_INVALID_DOCUMENT);
        document["scene"] = 0;
        document["scenes"][0]["nodes"][0] = 3;
        rejects(fixture(document.dump(), triangleBinary()), DUST3D_STATUS_INVALID_DOCUMENT);
        document = nlohmann::json::parse(triangleJson());
        document["materials"] = nlohmann::json::array({ { { "pbrMetallicRoughness", { { "baseColorFactor", { 0.2, 0.4, 0.8, 1.0 } } } } },
            { { "pbrMetallicRoughness", { { "baseColorFactor", { 1.0, 0.0, 0.0, 0.5 } } } } } });
        document["meshes"][0]["primitives"][0]["material"] = 1;
        bytes = fixture(document.dump(), triangleBinary());
        result = readGlb(bytes.data(), bytes.size());
        require(result.baseColor == std::array<double, 4> { 1, 0, 0, 0.5 }, "Primitive material selection ignored");
        document = nlohmann::json::parse(triangleJson());
        auto indexedBinary = triangleBinary();
        indexedBinary.insert(indexedBinary.end(), { 0, 1, 2 });
        document["buffers"][0]["byteLength"] = indexedBinary.size();
        document["bufferViews"].push_back({ { "buffer", 0 }, { "byteOffset", 36 }, { "byteLength", 3 } });
        document["accessors"].push_back({ { "bufferView", 1 }, { "componentType", 5121 }, { "count", 3 }, { "type", "SCALAR" } });
        document["meshes"][0]["primitives"][0]["indices"] = 1;
        bytes = fixture(document.dump(), indexedBinary);
        result = readGlb(bytes.data(), bytes.size());
        require(result.model.faces[0] == std::vector<size_t> { 0, 1, 2 }, "UInt8 indices changed winding");
        indexedBinary.back() = 3;
        rejects(fixture(document.dump(), indexedBinary), DUST3D_STATUS_INVALID_DOCUMENT);
        json = triangleJson();
        replace(json, "\"asset\":", "\"extensionsRequired\":[\"Unimplemented\"],\"asset\":");
        rejects(fixture(json, triangleBinary()), DUST3D_STATUS_UNSUPPORTED_ASSET);
        json = triangleJson();
        replace(json, "\"asset\":", "\"extras\":" + std::string(70, '[') + "0" + std::string(70, ']') + ",\"asset\":");
        rejects(fixture(json, triangleBinary()), DUST3D_STATUS_LIMIT_EXCEEDED);
        std::cout << "Bounded GLB geometry contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
