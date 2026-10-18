// SPDX-License-Identifier: MIT
#include "../../native/Engine/EngineInternal.h"
#include "../../native/Engine/ImportedModels.h"
#include <Dust3DBridge.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
const std::string modelIdentity = "{00000300-0000-4000-8000-000000000300}";
void require(bool value, const std::string& message)
{
    if (!value)
        throw std::runtime_error(message);
}
void appendWord(std::vector<uint8_t>& bytes, uint32_t value)
{
    for (size_t index = 0; index < 4; ++index)
        bytes.push_back(static_cast<uint8_t>(value >> (index * 8)));
}
std::vector<uint8_t> tetrahedron(float tipX = 0)
{
    // Closed, consistently oriented source mesh, independent of the decoder implementation.
    const std::array<float, 12> positions { -.5, 0, 0, .5, 0, 0, tipX, 1, 0, 0, .5, .5 };
    std::vector<uint8_t> binary;
    for (float position : positions) {
        uint32_t bits;
        std::memcpy(&bits, &position, sizeof(bits));
        appendWord(binary, bits);
    }
    for (uint16_t index : { 0, 2, 1, 0, 1, 3, 1, 2, 3, 2, 0, 3 }) {
        binary.push_back(static_cast<uint8_t>(index));
        binary.push_back(0);
    }
    std::string json = R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":72}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":48},{"buffer":0,"byteOffset":48,"byteLength":24}],"accessors":[{"bufferView":0,"componentType":5126,"count":4,"type":"VEC3","min":[-0.5,0,0],"max":[0.5,1,0.5]},{"bufferView":1,"componentType":5123,"count":12,"type":"SCALAR"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1}]}]})";
    while (json.size() % 4)
        json += ' ';
    std::vector<uint8_t> result;
    appendWord(result, 0x46546c67);
    appendWord(result, 2);
    appendWord(result, static_cast<uint32_t>(28 + json.size() + binary.size()));
    appendWord(result, static_cast<uint32_t>(json.size()));
    appendWord(result, 0x4e4f534a);
    result.insert(result.end(), json.begin(), json.end());
    appendWord(result, static_cast<uint32_t>(binary.size()));
    appendWord(result, 0x004e4942);
    result.insert(result.end(), binary.begin(), binary.end());
    return result;
}

std::vector<uint8_t> tetrahedronWithNormals()
{
    const std::array<std::array<float, 3>, 4> positions { { { -.5, 0, 0 }, { .5, 0, 0 }, { 0, 1, 0 }, { 0, .5, .5 } } };
    const std::array<std::array<size_t, 3>, 4> faces { { { 0, 2, 1 }, { 0, 1, 3 }, { 1, 2, 3 }, { 2, 0, 3 } } };
    std::vector<uint8_t> binary, normals;
    auto appendSample = [](std::vector<uint8_t>& bytes, float value) {
        uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        appendWord(bytes, bits);
    };
    for (const auto& face : faces) {
        std::array<double, 3> first, second;
        for (size_t axis = 0; axis < 3; ++axis) {
            first[axis] = positions[face[1]][axis] - positions[face[0]][axis];
            second[axis] = positions[face[2]][axis] - positions[face[0]][axis];
        }
        const std::array<double, 3> normal { first[1] * second[2] - first[2] * second[1], first[2] * second[0] - first[0] * second[2], first[0] * second[1] - first[1] * second[0] };
        const double length = std::sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
        for (size_t vertex : face) {
            for (size_t axis = 0; axis < 3; ++axis) {
                appendSample(binary, positions[vertex][axis]);
                appendSample(normals, static_cast<float>(normal[axis] / length));
            }
        }
    }
    binary.insert(binary.end(), normals.begin(), normals.end());
    std::string json = R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":288}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":144},{"buffer":0,"byteOffset":144,"byteLength":144}],"accessors":[{"bufferView":0,"componentType":5126,"count":12,"type":"VEC3","min":[-0.5,0,0],"max":[0.5,1,0.5]},{"bufferView":1,"componentType":5126,"count":12,"type":"VEC3"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1}}]}]})";
    while (json.size() % 4)
        json += ' ';
    std::vector<uint8_t> result;
    appendWord(result, 0x46546c67);
    appendWord(result, 2);
    appendWord(result, static_cast<uint32_t>(28 + json.size() + binary.size()));
    appendWord(result, static_cast<uint32_t>(json.size()));
    appendWord(result, 0x4e4f534a);
    result.insert(result.end(), json.begin(), json.end());
    appendWord(result, static_cast<uint32_t>(binary.size()));
    appendWord(result, 0x004e4942);
    result.insert(result.end(), binary.begin(), binary.end());
    return result;
}

std::vector<uint8_t> attributedTetrahedron(const std::array<uint8_t, 3>& color, const std::array<float, 3>& normal)
{
    const auto original = tetrahedron();
    std::vector<uint8_t> binary(original.end() - 72, original.end());
    for (size_t vertex = 0; vertex < 4; ++vertex) {
        for (float value : normal) {
            uint32_t bits;
            std::memcpy(&bits, &value, sizeof(bits));
            appendWord(binary, bits);
        }
    }
    for (size_t vertex = 0; vertex < 4; ++vertex)
        binary.insert(binary.end(), { color[0], color[1], color[2], 255 });
    std::string json = R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":136}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":48},{"buffer":0,"byteOffset":48,"byteLength":24},{"buffer":0,"byteOffset":72,"byteLength":48},{"buffer":0,"byteOffset":120,"byteLength":16}],"accessors":[{"bufferView":0,"componentType":5126,"count":4,"type":"VEC3","min":[-0.5,0,0],"max":[0.5,1,0.5]},{"bufferView":1,"componentType":5123,"count":12,"type":"SCALAR"},{"bufferView":2,"componentType":5126,"count":4,"type":"VEC3"},{"bufferView":3,"componentType":5121,"count":4,"type":"VEC4","normalized":true}],"meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":2,"COLOR_0":3},"indices":1}]}]})";
    while (json.size() % 4)
        json += ' ';
    std::vector<uint8_t> result;
    appendWord(result, 0x46546c67);
    appendWord(result, 2);
    appendWord(result, static_cast<uint32_t>(28 + json.size() + binary.size()));
    appendWord(result, static_cast<uint32_t>(json.size()));
    appendWord(result, 0x4e4f534a);
    result.insert(result.end(), json.begin(), json.end());
    appendWord(result, static_cast<uint32_t>(binary.size()));
    appendWord(result, 0x004e4942);
    result.insert(result.end(), binary.begin(), binary.end());
    return result;
}
std::vector<uint8_t> colorSeamTetrahedron()
{
    const std::array<std::array<float, 3>, 4> positions { { { -.5, 0, 0 }, { .5, 0, 0 }, { 0, 1, 0 }, { 0, .5, .5 } } };
    const std::array<std::array<size_t, 3>, 4> faces { { { 0, 2, 1 }, { 0, 1, 3 }, { 1, 2, 3 }, { 2, 0, 3 } } };
    const std::array<std::array<uint8_t, 4>, 4> colors { { { 255, 0, 0, 255 }, { 0, 255, 0, 255 }, { 0, 0, 255, 255 }, { 255, 255, 0, 255 } } };
    std::vector<uint8_t> binary;
    for (const auto& face : faces) {
        for (auto vertex : face) {
            for (float value : positions[vertex]) {
                uint32_t bits;
                std::memcpy(&bits, &value, sizeof(bits));
                appendWord(binary, bits);
            }
        }
    }
    for (const auto& color : colors) {
        for (size_t corner = 0; corner < 3; ++corner)
            binary.insert(binary.end(), color.begin(), color.end());
    }
    std::string json = R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":192}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":144},{"buffer":0,"byteOffset":144,"byteLength":48}],"accessors":[{"bufferView":0,"componentType":5126,"count":12,"type":"VEC3","min":[-0.5,0,0],"max":[0.5,1,0.5]},{"bufferView":1,"componentType":5121,"count":12,"type":"VEC4","normalized":true}],"meshes":[{"primitives":[{"attributes":{"POSITION":0,"COLOR_0":1}}]}]})";
    while (json.size() % 4)
        json += ' ';
    std::vector<uint8_t> result;
    appendWord(result, 0x46546c67);
    appendWord(result, 2);
    appendWord(result, static_cast<uint32_t>(28 + json.size() + binary.size()));
    appendWord(result, static_cast<uint32_t>(json.size()));
    appendWord(result, 0x4e4f534a);
    result.insert(result.end(), json.begin(), json.end());
    appendWord(result, static_cast<uint32_t>(binary.size()));
    appendWord(result, 0x004e4942);
    result.insert(result.end(), binary.begin(), binary.end());
    return result;
}
std::vector<uint8_t> texturedTetrahedron()
{
    const auto original = tetrahedron();
    std::vector<uint8_t> binary(original.end() - 72, original.end());
    for (float uv : { 0.0f, 0.0f, 1.0f, 0.0f, .5f, 1.0f, .5f, .5f }) {
        uint32_t bits;
        std::memcpy(&bits, &uv, sizeof(bits));
        appendWord(binary, bits);
    }
    // Opaque encoded-image marker: geometry loading does not claim pixel decoding.
    binary.insert(binary.end(), { 1, 2, 3, 4 });
    std::string json = R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":108}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":48},{"buffer":0,"byteOffset":48,"byteLength":24},{"buffer":0,"byteOffset":72,"byteLength":32},{"buffer":0,"byteOffset":104,"byteLength":4}],"accessors":[{"bufferView":0,"componentType":5126,"count":4,"type":"VEC3","min":[-0.5,0,0],"max":[0.5,1,0.5]},{"bufferView":1,"componentType":5123,"count":12,"type":"SCALAR"},{"bufferView":2,"componentType":5126,"count":4,"type":"VEC2"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0,"TEXCOORD_0":2},"indices":1,"material":0}]}],"materials":[{"pbrMetallicRoughness":{"baseColorTexture":{"index":0}}}],"textures":[{"source":0}],"images":[{"bufferView":3,"mimeType":"image/png"}]})";
    while (json.size() % 4)
        json += ' ';
    std::vector<uint8_t> result;
    appendWord(result, 0x46546c67);
    appendWord(result, 2);
    appendWord(result, static_cast<uint32_t>(28 + json.size() + binary.size()));
    appendWord(result, static_cast<uint32_t>(json.size()));
    appendWord(result, 0x4e4f534a);
    result.insert(result.end(), json.begin(), json.end());
    appendWord(result, static_cast<uint32_t>(binary.size()));
    appendWord(result, 0x004e4942);
    result.insert(result.end(), binary.begin(), binary.end());
    return result;
}
std::string modelXml(const std::string& width, const std::string& reference = modelIdentity, const std::string& target = "ImportedModel")
{
    const std::string part = "{00000100-0000-4000-8000-000000000100}";
    return "<canvas originX='.5' originY='.5' originZ='.5'><nodes>"
           "<node id='{00001000-0000-4000-8000-000000001000}' partId='"
        + part + "' x='.5' y='.2' z='.5' radius='.15'/>"
                 "<node id='{00001001-0000-4000-8000-000000001001}' partId='"
        + part + "' x='.5' y='.8' z='.5' radius='.15'/></nodes><edges>"
                 "<edge id='{00002000-0000-4000-8000-000000002000}' partId='"
        + part + "' from='{00001000-0000-4000-8000-000000001000}' to='{00001001-0000-4000-8000-000000001001}'/></edges><parts>"
                 "<part id='"
        + part + "' target='" + target + "' importedModelId='" + reference + "' deformWidth='" + width + "'/></parts><components>"
                                                                                                         "<component id='{00000200-0000-4000-8000-000000000200}' linkDataType='partId' linkData='"
        + part + "'/></components></canvas>";
}
using Asset = std::pair<std::string, std::vector<uint8_t>>;
std::vector<uint8_t> archiveWithAssets(const std::string& xml, const std::vector<Asset>& assets)
{
    const std::string prefix = "DUST3D 1.0 xml ";
    std::string header = "<ds3><model name='model.xml' offset='0' size='" + std::to_string(xml.size()) + "'/>";
    size_t offset = xml.size();
    for (const auto& [name, payload] : assets) {
        header += "<asset name='" + name + "' offset='" + std::to_string(offset) + "' size='" + std::to_string(payload.size()) + "'/>";
        offset += payload.size();
    }
    header += "</ds3>";
    std::ostringstream text;
    text << prefix << std::setfill('0') << std::setw(10) << prefix.size() + 12 + header.size() << "\r\n"
         << header << xml;
    const auto envelope = text.str();
    std::vector<uint8_t> result(envelope.begin(), envelope.end());
    for (const auto& [name, payload] : assets)
        result.insert(result.end(), payload.begin(), payload.end());
    return result;
}
std::vector<uint8_t> archive(const std::string& width)
{
    return archiveWithAssets(modelXml(width), { { "models/" + modelIdentity + ".glb", tetrahedron() } });
}
using MeshOwner = std::unique_ptr<Dust3DMesh, decltype(&dust3d_mesh_destroy)>;
MeshOwner load(const std::vector<uint8_t>& bytes)
{
    Dust3DError error {};
    MeshOwner mesh(dust3d_mesh_load(bytes.data(), bytes.size(), &error), dust3d_mesh_destroy);
    require(mesh && error.status == DUST3D_STATUS_OK, std::string("Imported .ds3 generation failed: ") + error.message);
    return mesh;
}
void requiresFailure(const std::vector<uint8_t>& bytes, Dust3DStatus status, const std::string& cause)
{
    Dust3DError error {};
    MeshOwner mesh(dust3d_mesh_load(bytes.data(), bytes.size(), &error), dust3d_mesh_destroy);
    require(!mesh && error.status == status, std::string("Wrong import failure: ") + error.message);
    require(std::string(error.message).find(cause) != std::string::npos, "Original import diagnostic lost");
}
using Positions = std::vector<std::array<int64_t, 3>>;
Positions positions(const Dust3DMesh* mesh)
{
    Positions result;
    const auto* values = dust3d_mesh_vertices(mesh);
    for (size_t index = 0; index < dust3d_mesh_vertex_count(mesh); ++index) {
        std::array<int64_t, 3> point;
        for (size_t axis = 0; axis < 3; ++axis)
            point[axis] = std::llround(double(values[index].position[axis]) * 1000000);
        result.push_back(point);
    }
    std::sort(result.begin(), result.end());
    return result;
}
void requireSurfaceNormals(const Dust3DMesh* mesh)
{
    const auto* vertices = dust3d_mesh_vertices(mesh);
    for (size_t triangle = 0; triangle < dust3d_mesh_vertex_count(mesh); triangle += 3) {
        std::array<double, 3> first, second;
        for (size_t axis = 0; axis < 3; ++axis) {
            first[axis] = double(vertices[triangle + 1].position[axis]) - vertices[triangle].position[axis];
            second[axis] = double(vertices[triangle + 2].position[axis]) - vertices[triangle].position[axis];
        }
        std::array<double, 3> normal { first[1] * second[2] - first[2] * second[1], first[2] * second[0] - first[0] * second[2], first[0] * second[1] - first[1] * second[0] };
        const double length = std::sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
        require(length > 1e-12, "Fixture produced a degenerate exported face");
        for (size_t corner = 0; corner < 3; ++corner) {
            double dot = 0;
            for (size_t axis = 0; axis < 3; ++axis)
                dot += normal[axis] / length * vertices[triangle + corner].normal[axis];
            require(std::abs(dot - 1) < 2e-5, "Imported normal is not perpendicular to its transformed face");
        }
    }
}
void testTransformedImportedNormals()
{
    auto xml = modelXml("2");
    const auto position = xml.find("deformWidth='2'");
    require(position != std::string::npos, "Cannot configure normal fixture");
    xml.insert(position, "deformThickness='1' cutRotation='.5' ");
    for (const auto& payload : { tetrahedron(), tetrahedronWithNormals() }) {
        auto mesh = load(archiveWithAssets(xml, { { "models/" + modelIdentity + ".glb", payload } }));
        requireSurfaceNormals(mesh.get());
    }
}
void testImportedDeformationQuantizationOverflow()
{
    auto xml = modelXml("1000000");
    const std::string radius = "radius='.15'";
    size_t offset = 0;
    while ((offset = xml.find(radius, offset)) != std::string::npos) {
        const std::string replacement = "radius='1000000' deformWidth='1000000'";
        xml.replace(offset, radius.size(), replacement);
        offset += replacement.size();
    }
    const auto bytes = archiveWithAssets(xml, { { "models/" + modelIdentity + ".glb", tetrahedron() } });
    requiresFailure(bytes, DUST3D_STATUS_INVALID_DOCUMENT, "quantization");
    Dust3DError error {};
    Dust3DSourceOptions options { sizeof(Dust3DSourceOptions), 0 };
    std::unique_ptr<Dust3DSourcedMesh, decltype(&dust3d_sourced_mesh_destroy)> sourced(
        dust3d_sourced_mesh_load(bytes.data(), bytes.size(), &options, &error), dust3d_sourced_mesh_destroy);
    require(!sourced && error.status == DUST3D_STATUS_INVALID_DOCUMENT && std::string(error.message).find("quantization") != std::string::npos,
        "Sourced import did not preserve the quantization failure");
    std::unique_ptr<Dust3DGenerationSession, decltype(&dust3d_generation_session_destroy)> session(
        dust3d_generation_session_create(DUST3D_DEFAULT_CACHE_PAYLOAD_BYTES, &error), dust3d_generation_session_destroy);
    const auto valid = archive("1");
    for (unsigned iteration = 0; iteration < 2; ++iteration) {
        MeshOwner accepted(dust3d_generation_session_load(session.get(), valid.data(), valid.size(), &error), dust3d_mesh_destroy);
        require(accepted && error.status == DUST3D_STATUS_OK, "Session did not recover after quantization rejection");
        MeshOwner rejected(dust3d_generation_session_load(session.get(), bytes.data(), bytes.size(), &error), dust3d_mesh_destroy);
        require(!rejected && error.status == DUST3D_STATUS_INVALID_DOCUMENT && std::string(error.message).find("quantization") != std::string::npos,
            "Cached import did not preserve the quantization failure");
    }
}
void testImportedDirectionalNodeScales()
{
    auto xml = modelXml("1");
    const std::string nodeScales = "deformWidth='2' deformThickness='3' ";
    const std::string radiusAttribute = "radius='.15'";
    size_t offset = 0;
    while ((offset = xml.find(radiusAttribute, offset)) != std::string::npos) {
        xml.insert(offset, nodeScales);
        offset += nodeScales.size() + radiusAttribute.size();
    }
    auto original = load(archive("1"));
    auto scaled = load(archiveWithAssets(xml, { { "models/" + modelIdentity + ".glb", tetrahedronWithNormals() } }));
    auto extent = [](const Dust3DMesh* mesh, size_t axis) {
        const auto* vertices = dust3d_mesh_vertices(mesh);
        double minimum = vertices[0].position[axis], maximum = minimum;
        for (size_t index = 0; index < dust3d_mesh_vertex_count(mesh); ++index) {
            minimum = std::min(minimum, double(vertices[index].position[axis]));
            maximum = std::max(maximum, double(vertices[index].position[axis]));
        }
        return maximum - minimum;
    };
    require(std::abs(extent(scaled.get(), 0) - 2 * extent(original.get(), 0)) < 1e-6,
        "Imported node width modifiers were ignored");
    require(std::abs(extent(scaled.get(), 2) - 3 * extent(original.get(), 2)) < 1e-6,
        "Imported node thickness modifiers were ignored");
    requireSurfaceNormals(scaled.get());
}
void testZeroWidthGeometry()
{
    auto mesh = load(archive("0"));
    require(dust3d_mesh_vertex_count(mesh.get()) >= 3, "Zero width erased all flattened faces");
    const auto* vertices = dust3d_mesh_vertices(mesh.get());
    double minimumZ = vertices[0].position[2], maximumZ = minimumZ;
    for (size_t index = 0; index < dust3d_mesh_vertex_count(mesh.get()); ++index) {
        for (size_t axis = 0; axis < 3; ++axis)
            require(std::isfinite(vertices[index].position[axis]) && std::isfinite(vertices[index].normal[axis]), "Zero width produced nonfinite geometry");
        require(std::abs(vertices[index].position[0]) < 1e-6, "Zero width did not flatten the width axis");
        minimumZ = std::min(minimumZ, double(vertices[index].position[2]));
        maximumZ = std::max(maximumZ, double(vertices[index].position[2]));
    }
    require(maximumZ - minimumZ > .1, "Zero width also collapsed the independent thickness axis");
    requireSurfaceNormals(mesh.get());
}
void testRequiredAssetsAndAliases()
{
    const auto xml = modelXml("1");
    requiresFailure(archiveWithAssets(xml, {}), DUST3D_STATUS_INVALID_DOCUMENT, "Missing required");
    requiresFailure(archiveWithAssets(xml, { { "models/" + modelIdentity + ".glb", { 1, 2, 3 } } }), DUST3D_STATUS_INVALID_DOCUMENT, "GLB: truncated");
    requiresFailure(archiveWithAssets(modelXml("1", "not-a-uuid"), {}), DUST3D_STATUS_INVALID_DOCUMENT, "valid importedModelId");
    requiresFailure(archiveWithAssets(modelXml("1", modelIdentity, "FutureImportedModel"), { { "models/" + modelIdentity + ".glb", tetrahedron() } }), DUST3D_STATUS_UNSUPPORTED_ASSET, "Unsupported part target");
    auto badSeparator = modelIdentity;
    badSeparator[9] = '_';
    requiresFailure(archiveWithAssets(modelXml("1", badSeparator), {}), DUST3D_STATUS_INVALID_DOCUMENT, "valid importedModelId");
    const auto bare = modelIdentity.substr(1, 36);
    auto alias = load(archiveWithAssets(modelXml("1", bare), { { "models/" + modelIdentity + ".glb", tetrahedron() } }));
    require(dust3d_mesh_vertex_count(alias.get()) == 12, "Brace alias did not bind");
    requiresFailure(archiveWithAssets(xml, { { "models/" + modelIdentity + ".glb", tetrahedron() }, { "models/" + bare + ".glb", tetrahedron() } }), DUST3D_STATUS_INVALID_DOCUMENT, "ambiguous");
    auto reverseAlias = load(archiveWithAssets(xml, { { "models/" + bare + ".glb", tetrahedron() } }));
    require(dust3d_mesh_vertex_count(reverseAlias.get()) == 12, "Bare asset UUID alias did not bind");
    const std::string lower = "{abcdef00-0000-4000-8000-000000000300}";
    const std::string upper = "{ABCDEF00-0000-4000-8000-000000000300}";
    requiresFailure(archiveWithAssets(modelXml("1", lower), { { "models/" + upper + ".glb", tetrahedron() } }), DUST3D_STATUS_INVALID_DOCUMENT, "Missing required");
    requiresFailure(archiveWithAssets(modelXml("1", "{zzzzzzzz-0000-4000-8000-000000000300}"), {}), DUST3D_STATUS_INVALID_DOCUMENT, "valid importedModelId");
    auto unused = load(archiveWithAssets(modelXml("1", modelIdentity, "Model"), { { "models/opaque-future-model.bin", { 1, 2, 3 } } }));
    require(dust3d_mesh_vertex_count(unused.get()) > 0, "Unused opaque assets prevented procedural generation");
    require(std::string(dust3d_mesh_warning(unused.get())).find("Imported geometry") == std::string::npos, "Unused model reported as rendered");
}
void testOwnedOutputsAndMaterialDiagnostics()
{
    auto bytes = archive("1");
    const auto original = bytes;
    auto mesh = load(bytes);
    require(bytes == original, "Import mutated source archive bytes");
    const auto saved = positions(mesh.get());
    require(std::string(dust3d_mesh_warning(mesh.get())).find("materials, textures and transparency") != std::string::npos, "Incomplete materials were not disclosed");
    Dust3DError error {};
    std::unique_ptr<Dust3DIndexedMesh, decltype(&dust3d_indexed_mesh_destroy)> indexed(dust3d_mesh_create_indexed(mesh.get(), 0, &error), dust3d_indexed_mesh_destroy);
    require(indexed && error.status == DUST3D_STATUS_OK && dust3d_indexed_mesh_index_count(indexed.get()) == 12, "Indexed import copy failed");
    std::fill(bytes.begin(), bytes.end(), 0);
    auto another = load(archive("2"));
    require(positions(mesh.get()) == saved, "Later import or input overwrite invalidated owned output");
    Dust3DSourceOptions sourceOptions {};
    sourceOptions.struct_size = sizeof(sourceOptions);
    std::unique_ptr<Dust3DSourcedMesh, decltype(&dust3d_sourced_mesh_destroy)> sourced(dust3d_sourced_mesh_load(original.data(), original.size(), &sourceOptions, &error), dust3d_sourced_mesh_destroy);
    require(sourced && error.status == DUST3D_STATUS_OK, std::string("Sourced import failed: ") + error.message);
    require(dust3d_sourced_mesh_corner_count(sourced.get()) == 12 && positions(dust3d_sourced_mesh_geometry(sourced.get())) == saved, "Sourced import alignment failed");
    auto textured = load(archiveWithAssets(modelXml("1"), { { "models/" + modelIdentity + ".glb", texturedTetrahedron() }, { "images/opaque-retained.png", { 9 } } }));
    const std::string warnings = dust3d_mesh_warning(textured.get());
    require(warnings.find("Untextured preview") != std::string::npos && warnings.find("Imported geometry preview") != std::string::npos, "Import diagnostics hid the existing image warning");
    Dust3DUvOptions uv {};
    uv.struct_size = sizeof(uv);
    std::unique_ptr<Dust3DUvMesh, decltype(&dust3d_uv_mesh_destroy)> uvMesh(dust3d_uv_mesh_load(original.data(), original.size(), &uv, &error), dust3d_uv_mesh_destroy);
    require(!uvMesh && error.status == DUST3D_STATUS_UNSUPPORTED_ASSET, "UV operation silently omitted imported material state");
    Dust3DBakeOptions bake {};
    bake.struct_size = sizeof(bake);
    std::unique_ptr<Dust3DBakedMesh, decltype(&dust3d_baked_mesh_destroy)> baked(dust3d_baked_mesh_load(original.data(), original.size(), &bake, &error), dust3d_baked_mesh_destroy);
    require(!baked && error.status == DUST3D_STATUS_UNSUPPORTED_ASSET, "Bake operation silently omitted imported material state");
}
void testSessionAssetRevisionAndRecovery()
{
    for (size_t budget : { size_t(0), size_t(1), size_t(DUST3D_DEFAULT_CACHE_PAYLOAD_BYTES) }) {
        Dust3DError error {};
        std::unique_ptr<Dust3DGenerationSession, decltype(&dust3d_generation_session_destroy)> session(dust3d_generation_session_create(budget, &error), dust3d_generation_session_destroy);
        require(session && error.status == DUST3D_STATUS_OK, "Cannot create import session");
        auto firstBytes = archive("1");
        MeshOwner first(dust3d_generation_session_load(session.get(), firstBytes.data(), firstBytes.size(), &error), dust3d_mesh_destroy);
        require(first && error.status == DUST3D_STATUS_OK, "First session import failed");
        const auto original = positions(first.get());
        MeshOwner repeated(dust3d_generation_session_load(session.get(), firstBytes.data(), firstBytes.size(), &error), dust3d_mesh_destroy);
        require(repeated && error.status == DUST3D_STATUS_OK, "Repeated session import failed");
        Dust3DGenerationStats stats {};
        require(dust3d_generation_session_get_stats(session.get(), &stats, sizeof(stats), &error) == DUST3D_STATUS_OK, "Cannot inspect import cache stats");
        if (budget == DUST3D_DEFAULT_CACHE_PAYLOAD_BYTES)
            require(stats.cache_outcome == DUST3D_CACHE_RETAINED && stats.reused_components > 0, "Unchanged imports did not reuse bounded generated components");
        if (budget == 1)
            require(stats.cache_status == DUST3D_STATUS_LIMIT_EXCEEDED && stats.retained_payload_bytes == 0, "Optional cache pressure was not isolated");

        auto changed = archiveWithAssets(modelXml("1"), { { "models/" + modelIdentity + ".glb", tetrahedron(.25f) } });
        MeshOwner second(dust3d_generation_session_load(session.get(), changed.data(), changed.size(), &error), dust3d_mesh_destroy);
        require(second && error.status == DUST3D_STATUS_OK, "Changed session import failed");
        auto fresh = load(changed);
        require(positions(second.get()) != original && positions(second.get()) == positions(fresh.get()), "Same-UUID asset edit reused stale imported geometry");
        require(positions(first.get()) == original, "Session reload invalidated prior output");
        const auto broken = archiveWithAssets(modelXml("1"), { { "models/" + modelIdentity + ".glb", { 0 } } });
        MeshOwner rejected(dust3d_generation_session_load(session.get(), broken.data(), broken.size(), &error), dust3d_mesh_destroy);
        require(!rejected && error.status == DUST3D_STATUS_INVALID_DOCUMENT && std::string(error.message).find("GLB: truncated") != std::string::npos, "Required import failure degraded into cache success");
        MeshOwner recovered(dust3d_generation_session_load(session.get(), firstBytes.data(), firstBytes.size(), &error), dust3d_mesh_destroy);
        require(recovered && error.status == DUST3D_STATUS_OK && positions(recovered.get()) == original, "Import session did not recover after failure");
        session.reset();
        require(positions(first.get()) == original, "Session destruction invalidated imported output");
    }
}

std::string serialIdentity(unsigned value)
{
    std::ostringstream text;
    text << '{' << std::hex << std::setfill('0') << std::setw(8) << value << "-0000-4000-8000-" << std::setw(12) << value << '}';
    return text.str();
}
void testSharedAssetParts()
{
    std::string nodes, edges, parts, components;
    for (unsigned index = 0; index < 2; ++index) {
        const auto part = serialIdentity(100 + index);
        const auto first = serialIdentity(1000 + index * 2);
        const auto second = serialIdentity(1001 + index * 2);
        const auto x = std::to_string(.5 + index);
        const auto reference = index == 0 ? modelIdentity : modelIdentity.substr(1, 36);
        nodes += "<node id='" + first + "' partId='" + part + "' x='" + x + "' y='.2' z='.5' radius='.15'/>";
        nodes += "<node id='" + second + "' partId='" + part + "' x='" + x + "' y='.8' z='.5' radius='.15'/>";
        edges += "<edge id='" + serialIdentity(2000 + index) + "' partId='" + part + "' from='" + first + "' to='" + second + "'/>";
        parts += "<part id='" + part + "' target='ImportedModel' importedModelId='" + reference + "'/>";
        components += "<component id='" + serialIdentity(200 + index) + "' linkDataType='partId' linkData='" + part + "'/>";
    }
    const auto xml = "<canvas originX='.5' originY='.5' originZ='.5'><nodes>" + nodes + "</nodes><edges>" + edges + "</edges><parts>" + parts + "</parts><components>" + components + "</components></canvas>";
    auto mesh = load(archiveWithAssets(xml, { { "models/" + modelIdentity + ".glb", tetrahedron() } }));
    require(dust3d_mesh_vertex_count(mesh.get()) == 24 && dust3d_mesh_node_count(mesh.get()) == 4 && dust3d_mesh_part_count(mesh.get()) == 2, "Shared model asset did not generate both distinct parts");
}
void testImportedColorSeams()
{
    const auto bytes = archiveWithAssets(modelXml("1"), { { "models/" + modelIdentity + ".glb", colorSeamTetrahedron() } });
    auto assertColors = [](const Dust3DMesh* mesh) {
        require(dust3d_mesh_vertex_count(mesh) == 12, "Color seam fixture lost triangles");
        std::array<size_t, 4> counts {};
        const auto* vertices = dust3d_mesh_vertices(mesh);
        for (size_t index = 0; index < 12; ++index) {
            const auto& color = vertices[index].color;
            const unsigned palette = (color[0] > .99f ? 1 : 0) + (color[1] > .99f ? 2 : 0) + (color[2] > .99f ? 4 : 0);
            require(palette == 1 || palette == 2 || palette == 4 || palette == 3, "Color seam introduced an unrelated color");
            ++counts[palette == 1 ? 0 : palette == 2 ? 1
                    : palette == 4                   ? 2
                                                     : 3];
        }
        require(counts == std::array<size_t, 4> { 3, 3, 3, 3 }, "Imported color seam collapsed distinct coincident corners");
    };
    auto mesh = load(bytes);
    assertColors(mesh.get());
    std::string warning;
    dust3d::native::ImportedModels imports;
    auto snapshot = dust3d::native::readDocument(bytes.data(), bytes.size(), warning, nullptr, &imports);
    const auto group = serialIdentity(900);
    snapshot->components[group] = { { "id", group }, { "children", snapshot->rootComponent["children"] } };
    snapshot->rootComponent["children"] = group;
    dust3d::MeshGenerator generator(snapshot.release());
    generator.setImportedModelData(imports.takeGeometry());
    generator.generate();
    require(generator.isSuccessful(), "Color seam preview generation failed");
    for (const auto& component : { dust3d::Uuid("{00000200-0000-4000-8000-000000000200}"), dust3d::Uuid(group) }) {
        const auto& preview = generator.generatedComponentPreviews().at(component);
        std::array<size_t, 4> counts {};
        for (const auto& triangle : preview.triangles) {
            for (auto vertex : triangle) {
                const auto& color = std::get<0>(preview.vertexProperties.at(vertex));
                const unsigned palette = (color.r() > .99 ? 1 : 0) + (color.g() > .99 ? 2 : 0) + (color.b() > .99 ? 4 : 0);
                require(palette == 1 || palette == 2 || palette == 4 || palette == 3, "Preview seam introduced an unrelated color");
                ++counts[palette == 1 ? 0 : palette == 2 ? 1
                        : palette == 4                   ? 2
                                                         : 3];
            }
        }
        require(counts == std::array<size_t, 4> { 3, 3, 3, 3 }, "Leaf/group preview collapsed source color seams");
    }
    Dust3DError error {};
    std::unique_ptr<Dust3DGenerationSession, decltype(&dust3d_generation_session_destroy)> session(
        dust3d_generation_session_create(DUST3D_DEFAULT_CACHE_PAYLOAD_BYTES, &error), dust3d_generation_session_destroy);
    for (unsigned iteration = 0; iteration < 2; ++iteration) {
        MeshOwner cached(dust3d_generation_session_load(session.get(), bytes.data(), bytes.size(), &error), dust3d_mesh_destroy);
        require(cached && error.status == DUST3D_STATUS_OK, "Cached seam generation failed");
        assertColors(cached.get());
    }
}
void testSharedImportedPartFallbackColors(bool hasFirstColor)
{
    auto createArchive = [&](const std::string& secondColor) {
        auto xml = modelXml("1");
        const std::string part = "{00000100-0000-4000-8000-000000000100}";
        const auto first = "<component id='" + serialIdentity(200) + "' linkDataType='partId' linkData='" + part
            + "'" + (hasFirstColor ? " color='#ff0000'" : "") + "/>";
        const auto second = "<component id='" + serialIdentity(201) + "' linkDataType='partId' linkData='" + part
            + "' combineMode='Uncombined' color='" + secondColor + "'/>";
        const auto start = xml.find("<components>");
        const auto end = xml.find("</components>", start);
        xml.replace(start, end + std::string("</components>").size() - start, "<components>" + first + second + "</components>");
        return archiveWithAssets(xml, { { "models/" + modelIdentity + ".glb", tetrahedron() } });
    };
    auto assertColors = [&](const Dust3DMesh* mesh, bool isSecondGreen) {
        require(dust3d_mesh_vertex_count(mesh) == 24, "Shared imported part lost a component");
        std::array<size_t, 2> counts {};
        const auto* vertices = dust3d_mesh_vertices(mesh);
        for (size_t index = 0; index < 24; ++index) {
            const auto& color = vertices[index].color;
            if (color[0] > .99f && (hasFirstColor ? color[1] < .01f && color[2] < .01f : color[1] > .99f && color[2] > .99f))
                ++counts[0];
            else if (color[0] < .01f && color[isSecondGreen ? 1 : 2] > .99f && color[isSecondGreen ? 2 : 1] < .01f)
                ++counts[1];
            else
                throw std::runtime_error("Shared imported part introduced an unrelated fallback color");
        }
        require(counts == std::array<size_t, 2> { 12, 12 }, "Shared imported part overwrote component fallback colors");
    };
    const auto bytes = createArchive("#0000ff");
    assertColors(load(bytes).get(), false);
    Dust3DError error {};
    std::unique_ptr<Dust3DGenerationSession, decltype(&dust3d_generation_session_destroy)> session(
        dust3d_generation_session_create(DUST3D_DEFAULT_CACHE_PAYLOAD_BYTES, &error), dust3d_generation_session_destroy);
    for (unsigned iteration = 0; iteration < 3; ++iteration) {
        const auto edited = iteration == 2 ? createArchive("#00ff00") : bytes;
        MeshOwner cached(dust3d_generation_session_load(session.get(), edited.data(), edited.size(), &error), dust3d_mesh_destroy);
        require(cached && error.status == DUST3D_STATUS_OK, "Shared imported part session failed");
        assertColors(cached.get(), iteration == 2);
    }
    std::string warning;
    dust3d::native::ImportedModels imports;
    auto snapshot = dust3d::native::readDocument(bytes.data(), bytes.size(), warning, nullptr, &imports);
    const auto group = serialIdentity(900);
    snapshot->components[group] = { { "id", group }, { "children", snapshot->rootComponent["children"] } };
    snapshot->rootComponent["children"] = group;
    dust3d::MeshGenerator generator(snapshot.release());
    generator.setImportedModelData(imports.takeGeometry());
    generator.generate();
    require(generator.isSuccessful(), "Shared imported part group preview failed");
    for (unsigned identity : { 200, 201, 900 }) {
        const auto& preview = generator.generatedComponentPreviews().at(dust3d::Uuid(serialIdentity(identity)));
        std::array<size_t, 2> counts {};
        for (const auto& triangle : preview.triangles) {
            for (auto vertex : triangle) {
                const auto color = preview.vertexProperties.empty() ? preview.color : std::get<0>(preview.vertexProperties.at(vertex));
                if (color.r() > .99 && (hasFirstColor ? color.g() < .01 && color.b() < .01 : color.g() > .99 && color.b() > .99))
                    ++counts[0];
                else if (color.r() < .01 && color.g() < .01 && color.b() > .99)
                    ++counts[1];
                else
                    throw std::runtime_error("Shared imported preview introduced an unrelated fallback color");
            }
        }
        require(counts == (identity == 900 ? std::array<size_t, 2> { 12, 12 } : identity == 200 ? std::array<size_t, 2> { 12, 0 }
                                                                                                : std::array<size_t, 2> { 0, 12 }),
            "Shared imported part overwrote leaf/group preview fallback colors");
    }
}
void testCoincidentImportedAttributeIsolation(bool hasSecondColor, bool hasComponentAliases = false)
{
    std::string nodes, edges, parts, components;
    std::vector<Asset> assets;
    for (unsigned index = 0; index < 2; ++index) {
        const auto part = serialIdentity(100 + index);
        const auto model = serialIdentity(300 + index);
        const auto first = serialIdentity(1000 + index * 2);
        const auto second = serialIdentity(1001 + index * 2);
        nodes += "<node id='" + first + "' partId='" + part + "' x='.5' y='.2' z='.5' radius='.15'/>";
        nodes += "<node id='" + second + "' partId='" + part + "' x='.5' y='.8' z='.5' radius='.15'/>";
        edges += "<edge id='" + serialIdentity(2000 + index) + "' partId='" + part + "' from='" + first + "' to='" + second + "'/>";
        parts += "<part id='" + part + "' target='ImportedModel' importedModelId='" + model + "'/>";
        const auto componentId = hasComponentAliases ? (index ? serialIdentity(200).substr(1, 36) : serialIdentity(200)) : serialIdentity(200 + index);
        components += "<component id='" + componentId + "' linkDataType='partId' linkData='" + part + "'" + (index ? " combineMode='Uncombined' color='#0000ff'" : "") + "/>";
        assets.emplace_back("models/" + model + ".glb", index ? (hasSecondColor ? attributedTetrahedron({ 0, 255, 0 }, { 0, 0, 1 }) : tetrahedron()) : attributedTetrahedron({ 255, 0, 0 }, { 1, 0, 0 }));
    }
    const auto xml = "<canvas originX='.5' originY='.5' originZ='.5'><nodes>" + nodes + "</nodes><edges>" + edges + "</edges><parts>" + parts + "</parts><components>" + components + "</components></canvas>";
    const auto bytes = archiveWithAssets(xml, assets);
    auto mesh = load(bytes);
    require(dust3d_mesh_vertex_count(mesh.get()) == 24, "Coincident uncombined import lost geometry");
    const auto* vertices = dust3d_mesh_vertices(mesh.get());
    size_t red = 0, green = 0;
    for (size_t index = 0; index < 24; ++index) {
        const auto& vertex = vertices[index];
        if (vertex.color[0] > .99f && vertex.color[1] < .01f) {
            ++red;
            require(vertex.normal[0] > .99f, "Another part overwrote the red import normal");
        } else if (vertex.color[hasSecondColor ? 1 : 2] > .99f && vertex.color[0] < .01f) {
            ++green;
            if (hasSecondColor)
                require(vertex.normal[2] < -.99f, "Another part overwrote the green import normal");
        } else {
            throw std::runtime_error("Imported vertex color was lost");
        }
    }
    require(red == 12 && green == 12, "Coincident parts overwrote each other's imported attributes");
    Dust3DSourceOptions sourceOptions { sizeof(Dust3DSourceOptions), 0 };
    Dust3DError sourceError {};
    std::unique_ptr<Dust3DSourcedMesh, decltype(&dust3d_sourced_mesh_destroy)> sourced(
        dust3d_sourced_mesh_load(bytes.data(), bytes.size(), &sourceOptions, &sourceError), dust3d_sourced_mesh_destroy);
    require(sourced && sourceError.status == DUST3D_STATUS_OK && dust3d_sourced_mesh_corner_count(sourced.get()) == 24,
        "Coincident sourced import failed");
    auto assertSourceOwnership = [](const Dust3DSourcedMesh* source) {
        const auto* sourceCorners = dust3d_sourced_mesh_corners(source);
        const auto* sourceInfluences = dust3d_sourced_mesh_influences(source);
        const auto* sourcedVertices = dust3d_mesh_vertices(dust3d_sourced_mesh_geometry(source));
        size_t firstPartHints = 0, secondPartHints = 0;
        for (size_t index = 0; index < 24; ++index) {
            const auto& corner = sourceCorners[index];
            require(corner.influence_count == 1, "Imported corner must retain its nearest-node association");
            const auto identity = sourceInfluences[corner.first_influence].source_index;
            if (corner.kind == DUST3D_SOURCE_UNKNOWN) {
                require(identity == SIZE_MAX, "Generated nearest-node identities must remain explicitly unknown");
                continue;
            }
            require(corner.kind == DUST3D_SOURCE_NODE_HINT, "Unexpected imported source kind");
            sourcedVertices[index].color[0] > .99f ? ++firstPartHints : ++secondPartHints;
            const auto expectedPart = serialIdentity(sourcedVertices[index].color[0] > .99f ? 100 : 101);
            require(dust3d_sourced_mesh_part_id(source, identity) == expectedPart,
                "Coincident imported source hint refers to another part");
        }
        require(firstPartHints > 0 && secondPartHints > 0, "Source regression must exercise both original parts");
    };
    assertSourceOwnership(sourced.get());
    std::string warning;
    dust3d::native::ImportedModels imports;
    auto snapshot = dust3d::native::readDocument(bytes.data(), bytes.size(), warning, nullptr, &imports);
    const auto group = serialIdentity(900);
    snapshot->components[group] = { { "id", group }, { "children", snapshot->rootComponent["children"] } };
    snapshot->rootComponent["children"] = group;
    dust3d::MeshGenerator generator(snapshot.release());
    generator.setImportedModelData(imports.takeGeometry());
    generator.generate();
    require(generator.isSuccessful(), "Coincident group preview generation failed");
    const auto& preview = generator.generatedComponentPreviews().at(dust3d::Uuid(group));
    size_t previewRed = 0, previewGreen = 0;
    for (const auto& triangle : preview.triangles) {
        for (auto vertex : triangle) {
            const auto& color = std::get<0>(preview.vertexProperties.at(vertex));
            previewRed += color.r() > .99 && color.g() < .01;
            previewGreen += (hasSecondColor ? color.g() : color.b()) > .99 && color.r() < .01;
        }
    }
    require(previewRed == 12 && previewGreen == 12, "Group preview merged coincident imported colors");
    Dust3DError error {};
    std::unique_ptr<Dust3DGenerationSession, decltype(&dust3d_generation_session_destroy)> session(
        dust3d_generation_session_create(DUST3D_DEFAULT_CACHE_PAYLOAD_BYTES, &error), dust3d_generation_session_destroy);
    for (unsigned iteration = 0; iteration < 2; ++iteration) {
        MeshOwner cached(dust3d_generation_session_load(session.get(), bytes.data(), bytes.size(), &error), dust3d_mesh_destroy);
        require(cached && error.status == DUST3D_STATUS_OK && dust3d_mesh_vertex_count(cached.get()) == 24,
            "Coincident import session failed");
        std::unique_ptr<Dust3DSourcedMesh, decltype(&dust3d_sourced_mesh_destroy)> cachedSources(
            dust3d_generation_session_load_sourced(session.get(), bytes.data(), bytes.size(), &sourceOptions, &error),
            dust3d_sourced_mesh_destroy);
        require(cachedSources && error.status == DUST3D_STATUS_OK && dust3d_sourced_mesh_corner_count(cachedSources.get()) == 24,
            "Coincident cached sourced import failed");
        assertSourceOwnership(cachedSources.get());
        const auto* cachedVertices = dust3d_mesh_vertices(cached.get());
        const auto* originalVertices = dust3d_mesh_vertices(mesh.get());
        for (size_t index = 0; index < 24; ++index) {
            for (size_t axis = 0; axis < 3; ++axis)
                require(cachedVertices[index].color[axis] == originalVertices[index].color[axis]
                        && cachedVertices[index].normal[axis] == originalVertices[index].normal[axis],
                    "Cached mesh copy lost imported attribute ownership");
        }
    }
}
std::vector<uint8_t> repeatedTriangles()
{
    constexpr size_t vertexCount = 125001;
    std::vector<uint8_t> binary;
    for (size_t index = 0; index < vertexCount; ++index) {
        const std::array<float, 3> point = index % 3 == 0 ? std::array<float, 3> { -.5, 0, 0 } : index % 3 == 1 ? std::array<float, 3> { .5, 0, 0 }
                                                                                                                : std::array<float, 3> { 0, 1, 0 };
        for (float position : point) {
            uint32_t bits;
            std::memcpy(&bits, &position, sizeof(bits));
            appendWord(binary, bits);
        }
    }
    std::string json = "{\"asset\":{\"version\":\"2.0\"},\"buffers\":[{\"byteLength\":" + std::to_string(binary.size())
        + "}],\"bufferViews\":[{\"buffer\":0,\"byteLength\":" + std::to_string(binary.size())
        + "}],\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":" + std::to_string(vertexCount)
        + ",\"type\":\"VEC3\",\"min\":[-0.5,0,0],\"max\":[0.5,1,0]}],\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0}}]}]}";
    while (json.size() % 4)
        json += ' ';
    std::vector<uint8_t> result;
    appendWord(result, 0x46546c67);
    appendWord(result, 2);
    appendWord(result, static_cast<uint32_t>(28 + json.size() + binary.size()));
    appendWord(result, static_cast<uint32_t>(json.size()));
    appendWord(result, 0x4e4f534a);
    result.insert(result.end(), json.begin(), json.end());
    appendWord(result, static_cast<uint32_t>(binary.size()));
    appendWord(result, 0x004e4942);
    result.insert(result.end(), binary.begin(), binary.end());
    return result;
}
void testAggregateImportedPayloadLimit()
{
    std::string nodes, edges, parts, components;
    std::vector<Asset> assets;
    const auto payload = repeatedTriangles();
    for (unsigned index = 0; index < 8; ++index) {
        const auto part = serialIdentity(100 + index);
        const auto model = serialIdentity(300 + index);
        const auto first = serialIdentity(1000 + index * 2);
        const auto second = serialIdentity(1001 + index * 2);
        nodes += "<node id='" + first + "' partId='" + part + "' x='.5' y='.2' z='.5' radius='.15'/>";
        nodes += "<node id='" + second + "' partId='" + part + "' x='.5' y='.8' z='.5' radius='.15'/>";
        edges += "<edge id='" + serialIdentity(2000 + index) + "' partId='" + part + "' from='" + first + "' to='" + second + "'/>";
        parts += "<part id='" + part + "' target='ImportedModel' importedModelId='" + model + "'/>";
        components += "<component id='" + serialIdentity(200 + index) + "' linkDataType='partId' linkData='" + part + "'/>";
        assets.emplace_back("models/" + model + ".glb", payload);
    }
    const auto xml = "<canvas originX='.5' originY='.5' originZ='.5'><nodes>" + nodes + "</nodes><edges>" + edges + "</edges><parts>" + parts + "</parts><components>" + components + "</components></canvas>";
    requiresFailure(archiveWithAssets(xml, assets), DUST3D_STATUS_LIMIT_EXCEEDED, "64 MiB aggregate");
}
double generatedWidth(const std::string& deformation)
{
    const auto bytes = archive(deformation);
    Dust3DError error {};
    std::unique_ptr<Dust3DMesh, decltype(&dust3d_mesh_destroy)> mesh(dust3d_mesh_load(bytes.data(), bytes.size(), &error), dust3d_mesh_destroy);
    require(mesh && error.status == DUST3D_STATUS_OK, std::string("Imported .ds3 generation failed: ") + error.message);
    require(dust3d_mesh_vertex_count(mesh.get()) == 12, "The imported tetrahedron must retain four triangles");
    const auto* vertices = dust3d_mesh_vertices(mesh.get());
    double minimum = vertices[0].position[0], maximum = minimum;
    for (size_t index = 0; index < dust3d_mesh_vertex_count(mesh.get()); ++index) {
        for (size_t axis = 0; axis < 3; ++axis)
            require(std::isfinite(vertices[index].position[axis]) && std::isfinite(vertices[index].normal[axis]), "Nonfinite imported geometry");
        minimum = std::min(minimum, double(vertices[index].position[0]));
        maximum = std::max(maximum, double(vertices[index].position[0]));
    }
    return maximum - minimum;
}
}
int main()
{
    try {
        const double original = generatedWidth("1");
        const double widened = generatedWidth("2");
        require(original > 0 && std::abs(widened - original * 2) < 1e-6, "Imported spine deformation was not applied");
        testTransformedImportedNormals();
        testImportedDeformationQuantizationOverflow();
        testImportedDirectionalNodeScales();
        testZeroWidthGeometry();
        testRequiredAssetsAndAliases();
        testOwnedOutputsAndMaterialDiagnostics();
        testSessionAssetRevisionAndRecovery();
        testSharedAssetParts();
        testSharedImportedPartFallbackColors(true);
        testSharedImportedPartFallbackColors(false);
        testImportedColorSeams();
        testCoincidentImportedAttributeIsolation(true);
        testCoincidentImportedAttributeIsolation(false);
        testCoincidentImportedAttributeIsolation(true, true);
        testAggregateImportedPayloadLimit();
        std::cout << "Imported archive generation contracts passed\n";
        return 0;
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}
