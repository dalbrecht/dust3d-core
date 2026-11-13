// SPDX-License-Identifier: MIT
// Positive caller-owned mixed input. No fixture reader/writer, catalog or resource probes.
#include "FbxMeshScene.h"
#include "GenerationPipeline.h"
#include "ImportedModels.h"
#include <Dust3DBridge.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <locale>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace dust3d;
using namespace dust3d::native;
void require(bool condition, const std::string& cause)
{
    if (!condition)
        throw std::runtime_error(cause);
}
void appendWord(std::vector<uint8_t>& bytes, uint32_t value)
{
    for (unsigned int shift : { 0u, 8u, 16u, 24u })
        bytes.push_back(static_cast<uint8_t>((value >> shift) & 0xff));
}
void appendFloat(std::vector<uint8_t>& bytes, float value)
{
    static_assert(sizeof(float) == sizeof(uint32_t));
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    appendWord(bytes, bits);
}
std::vector<uint8_t> blueTriangleGlb()
{
    std::vector<uint8_t> binary;
    for (float value : { -.5f, 0.f, 0.f, .5f, 1.f, 0.f, 0.f, .5f, .5f,
             0.f, 0.f, 1.f, 0.f, .5f, 1.f })
        appendFloat(binary, value);
    require(binary.size() == 60, "unexpected synthetic GLB binary length");
    std::string json = R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":60}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":24}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[-0.5,0,0],"max":[0.5,1,0.5]},{"bufferView":1,"componentType":5126,"count":3,"type":"VEC2"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0,"TEXCOORD_0":1},"material":0}]}],"materials":[{"pbrMetallicRoughness":{"baseColorFactor":[0,0,1,1]}}]})";
    while (json.size() % 4)
        json.push_back(' ');
    std::vector<uint8_t> glb;
    appendWord(glb, 0x46546c67);
    appendWord(glb, 2);
    appendWord(glb, static_cast<uint32_t>(28 + json.size() + binary.size()));
    appendWord(glb, static_cast<uint32_t>(json.size()));
    appendWord(glb, 0x4e4f534a);
    glb.insert(glb.end(), json.begin(), json.end());
    appendWord(glb, static_cast<uint32_t>(binary.size()));
    appendWord(glb, 0x004e4942);
    glb.insert(glb.end(), binary.begin(), binary.end());
    require(glb.size() == 580, "Approved synthetic GLB must remain exactly 580 bytes");
    return glb;
}
std::vector<uint8_t> mixedArchive()
{
    const std::string imported = "{00000100-0000-4000-8000-000000000100}";
    const std::string model = "{00000300-0000-4000-8000-000000000300}";
    const std::string procedural = "{00000101-0000-4000-8000-000000000101}";
    const std::string xml = "<canvas originX='.5' originY='.5' originZ='.5'><nodes>"
                            "<node id='{00001000-0000-4000-8000-000000001000}' partId='"
        + imported + "' x='.5' y='.2' z='.5' radius='.15'/>"
                     "<node id='{00001001-0000-4000-8000-000000001001}' partId='"
        + imported + "' x='.5' y='.8' z='.5' radius='.15'/>"
                     "<node id='{00001002-0000-4000-8000-000000001002}' partId='"
        + procedural + "' x='2' y='.2' z='.5' radius='.15'/>"
                       "<node id='{00001003-0000-4000-8000-000000001003}' partId='"
        + procedural + "' x='2' y='.8' z='.5' radius='.15'/>"
                       "</nodes><edges>"
                       "<edge id='{00002000-0000-4000-8000-000000002000}' partId='"
        + imported + "' from='{00001000-0000-4000-8000-000000001000}' to='{00001001-0000-4000-8000-000000001001}'/>"
                     "<edge id='{00002001-0000-4000-8000-000000002001}' partId='"
        + procedural + "' from='{00001002-0000-4000-8000-000000001002}' to='{00001003-0000-4000-8000-000000001003}'/>"
                       "</edges><parts><part id='"
        + imported + "' target='ImportedModel' importedModelId='" + model + "' interpolated='false'/>"
                                                                            "<part id='"
        + procedural + "' interpolated='false'/></parts><components>"
                       "<component id='{00000200-0000-4000-8000-000000000200}' linkDataType='partId' linkData='"
        + imported + "' combineMode='Uncombined'/>"
                     "<component id='{00000201-0000-4000-8000-000000000201}' linkDataType='partId' linkData='"
        + procedural + "' color='#ff0000' combineMode='Uncombined'/>"
                       "</components></canvas>";
    const auto glb = blueTriangleGlb();
    const std::string prefix = "DUST3D 1.0 xml ";
    const std::string header = "<ds3><model name='model.xml' offset='0' size='" + std::to_string(xml.size())
        + "'/><asset name='models/" + model + ".glb' offset='" + std::to_string(xml.size())
        + "' size='" + std::to_string(glb.size()) + "'/></ds3>";
    std::ostringstream text;
    text << prefix << std::setfill('0') << std::setw(10) << prefix.size() + 12 + header.size() << "\r\n"
         << header << xml;
    const auto envelope = text.str();
    std::vector<uint8_t> archive(envelope.begin(), envelope.end());
    archive.insert(archive.end(), glb.begin(), glb.end());
    require(xml.size() == 1580 && archive.size() == 2336, "Approved mixed archive must retain exact bounded sizes");
    return archive;
}

// Independent bounded FBX7400 consumer. It does not call the encoder's property,
// node, geometry-selection or byte-encoding helpers. Only this test's positive
// <=1MiB output is read; encoded arrays must be uncompressed as the API writes.
struct Consumer {
    const std::vector<uint8_t>& bytes;
    std::map<std::string, std::vector<double>> doubles;
    std::map<std::string, std::vector<int32_t>> integers;
    size_t records = 0;
    void range(size_t offset, size_t count) const
    {
        require(offset <= bytes.size() && count <= bytes.size() - offset, "FBX consumer bounds");
    }
    uint64_t word(size_t offset, size_t width) const
    {
        range(offset, width);
        uint64_t result = 0;
        for (size_t i = 0; i < width; ++i)
            result |= uint64_t(bytes[offset + i]) << (8 * i);
        return result;
    }
    size_t node(size_t offset, size_t parentEnd, size_t depth)
    {
        require(depth <= 16 && ++records <= 512, "Bounded FBX scene tree");
        range(offset, 13);
        const size_t end = word(offset, 4);
        if (!end)
            return offset + 13;
        require(end > offset + 13 && end <= parentEnd, "FBX absolute node end");
        const size_t count = word(offset + 4, 4);
        const size_t propertyBytes = word(offset + 8, 4);
        const size_t nameSize = word(offset + 12, 1);
        range(offset + 13, nameSize);
        const std::string name(bytes.begin() + offset + 13, bytes.begin() + offset + 13 + nameSize);
        size_t cursor = offset + 13 + nameSize;
        range(cursor, propertyBytes);
        const size_t propertyEnd = cursor + propertyBytes;
        require(propertyEnd <= end && count <= 64, "FBX property admission");
        for (size_t p = 0; p < count; ++p) {
            range(cursor, 1);
            const char type = bytes[cursor++];
            if (type == 'd' || type == 'i') {
                const size_t n = word(cursor, 4), encoding = word(cursor + 4, 4), length = word(cursor + 8, 4);
                const size_t width = type == 'd' ? 8 : 4;
                require(n <= 4096 && !encoding && length == n * width, "Bounded raw FBX array");
                cursor += 12;
                range(cursor, length);
                if (type == 'd') {
                    require(!doubles.count(name), "Unique geometry double array");
                    auto& values = doubles[name];
                    for (size_t i = 0; i < n; ++i) {
                        const uint64_t bits = word(cursor + i * 8, 8);
                        double value;
                        std::memcpy(&value, &bits, 8);
                        require(std::isfinite(value), "Finite decoded FBX value");
                        values.push_back(value);
                    }
                } else {
                    require(!integers.count(name), "Unique geometry integer array");
                    auto& values = integers[name];
                    for (size_t i = 0; i < n; ++i) {
                        const uint32_t bits = static_cast<uint32_t>(word(cursor + i * 4, 4));
                        int32_t value;
                        std::memcpy(&value, &bits, 4);
                        values.push_back(value);
                    }
                }
                cursor += length;
            } else if (type == 'S' || type == 'R') {
                const size_t length = word(cursor, 4);
                cursor += 4;
                range(cursor, length);
                cursor += length;
            } else {
                const size_t width = type == 'C' ? 1 : type == 'Y' ? 2
                    : type == 'I' || type == 'F'                   ? 4
                    : type == 'L' || type == 'D'                   ? 8
                                                                   : 0;
                require(width, "Known FBX scalar type");
                range(cursor, width);
                cursor += width;
            }
            require(cursor <= propertyEnd, "FBX property length");
        }
        require(cursor == propertyEnd, "FBX property count");
        while (cursor + 13 <= end && word(cursor, 4))
            cursor = node(cursor, end, depth + 1);
        require(cursor == end || cursor + 13 == end, "FBX child terminator");
        return end;
    }
    void read()
    {
        require(bytes.size() <= 1024 * 1024 && bytes.size() >= 27, "Bounded FBX output");
        const std::string magic("Kaydara FBX Binary  \0\x1a\0", 23);
        require(std::equal(magic.begin(), magic.end(), bytes.begin()) && word(23, 4) == 7400, "FBX7400 header");
        size_t cursor = 27;
        while (word(cursor, 4))
            cursor = node(cursor, bytes.size(), 0);
    }
};
std::unique_ptr<Object> reference(const std::vector<uint8_t>& archive, bool mixed)
{
    std::lock_guard<std::mutex> lock(engineMutex());
    std::string warning;
    ImportedModels imports;
    auto snapshot = readDocument(archive.data(), archive.size(), warning, nullptr, &imports);
    MeshGenerator generator(snapshot.release());
    generator.setImportedModelData(imports.takeGeometry());
    generator.generate();
    require(generator.isSuccessful(), "Reference generation succeeds");
    std::unique_ptr<Object> object(generator.takeObject());
    require(object && !object->triangles.empty() && object->vertices.size() <= 256 && object->triangles.size() <= 256, "Bounded original reference");
    if (mixed) {
        require(std::find(object->triangleModelKeys.begin(), object->triangleModelKeys.end(), "{00000300-0000-4000-8000-000000000300}") != object->triangleModelKeys.end(), "Actual imported source survives");
        require(std::find(object->triangleComponentKeys.begin(), object->triangleComponentKeys.end(), "{00000201-0000-4000-8000-000000000201}") != object->triangleComponentKeys.end(), "Actual procedural source survives");
    }
    return object;
}
void compare(Consumer& consumer, const Object& source)
{
    consumer.read();
    std::vector<double> positions, normals, uvs;
    std::vector<int32_t> indices;
    bool discriminatesFloat = false;
    for (const auto& vertex : source.vertices)
        for (size_t axis = 0; axis < 3; ++axis) {
            positions.push_back(vertex[axis]);
            discriminatesFloat |= double(float(vertex[axis])) != vertex[axis];
        }
    require(discriminatesFloat, "Original doubles differ from float reconstruction");
    for (const auto& face : source.triangles) {
        require(face.size() == 3, "Qt FBX uses triangles, not OBJ polygon arity");
        indices.insert(indices.end(), { int32_t(face[0]), int32_t(face[1]), -int32_t(face[2]) - 1 });
    }
    if (const auto* stream = source.triangleVertexNormals())
        for (const auto& face : *stream) {
            require(face.size() == 3, "Complete original normal corners");
            for (const auto& normal : face)
                for (size_t axis = 0; axis < 3; ++axis)
                    normals.push_back(normal[axis]);
        }
    if (const auto* stream = source.triangleVertexUvs())
        for (const auto& face : *stream) {
            require(face.size() == 3, "Complete original UV corners");
            for (const auto& uv : face) {
                uvs.push_back(uv.x());
                uvs.push_back(1 - uv.y());
            }
        }
    require(consumer.doubles["Vertices"] == positions, "Original double axes/positions preserved");
    require(consumer.integers["PolygonVertexIndex"] == indices, "Original triangle order/winding preserved");
    require(consumer.doubles["Normals"] == normals, "Available original corner normals preserved");
    require(consumer.doubles["UV"] == uvs, "Available original corner UVs preserve Qt V flip");
    require(!consumer.doubles.count("Weights"), "Static geometry has no skin");
}
using Owner = std::unique_ptr<Dust3DFbxExport, decltype(&dust3d_fbx_export_destroy)>;
void geometry(std::vector<uint8_t> archive, bool mixed)
{
    auto original = reference(archive, mixed);
    Dust3DError error {};
    Dust3DFbxExportOptions options {};
    options.struct_size = sizeof(options);
    options.mode = DUST3D_FBX_EXPORT_GEOMETRY;
    options.maximum_geometry_payload_bytes = options.maximum_output_payload_bytes = 1024 * 1024;
    Owner owner(dust3d_fbx_export_load(archive.data(), archive.size(), &options, &error), dust3d_fbx_export_destroy);
    require(owner && error.status == DUST3D_STATUS_OK, std::string("Original FBX geometry export: ") + error.message);
    const size_t size = dust3d_fbx_export_byte_count(owner.get());
    const auto* bytes = dust3d_fbx_export_bytes(owner.get());
    require(bytes && size && size <= 1024 * 1024, "Owned bounded FBX bytes");
    const std::vector<uint8_t> copy(bytes, bytes + size);
    const std::string warning = dust3d_fbx_export_warning(owner.get());
    if (mixed)
        require(!warning.empty(), "Omitted imported materials remain inspectable");
    Consumer consumer { copy };
    compare(consumer, *original);
    Owner later(dust3d_fbx_export_load(archive.data(), archive.size(), &options, &error), dust3d_fbx_export_destroy);
    require(later && error.status == DUST3D_STATUS_OK, "Later export succeeds");
    later.reset();
    original.reset();
    std::vector<uint8_t>().swap(archive);
    require(size == dust3d_fbx_export_byte_count(owner.get()) && std::equal(copy.begin(), copy.end(), dust3d_fbx_export_bytes(owner.get())) && warning == dust3d_fbx_export_warning(owner.get()), "Retained FBX owner outlives inputs and later export");
    owner.reset();
    Consumer retained { copy };
    retained.read();
    std::cout << "FBX vertices=" << consumer.doubles["Vertices"].size() / 3 << " triangles=" << consumer.integers["PolygonVertexIndex"].size() / 3 << " bytes=" << size << '\n';
}
void importedBakedUnsupported()
{
    const auto archive = mixedArchive();
    Dust3DError error {};
    Dust3DFbxExportOptions options {};
    options.struct_size = sizeof(options);
    options.mode = DUST3D_FBX_EXPORT_BAKED;
    options.bake.struct_size = sizeof(options.bake);
    options.bake.atlas_texture_size = options.bake.color_size = options.bake.normal_size = options.bake.material_size = 64;
    Owner owner(dust3d_fbx_export_load(archive.data(), archive.size(), &options, &error), dust3d_fbx_export_destroy);
    require(!owner && error.status == DUST3D_STATUS_UNSUPPORTED_ASSET && *error.message, "Imported baked export must reject explicitly, never downgrade");
}
}
int main(int argc, char** argv)
{
    try {
        require(argc == 3, "Audited scenario and Capsule directory required");
        const std::string scenario = argv[1];
        if (scenario == "mixed")
            geometry(mixedArchive(), true);
        else if (scenario == "capsule") {
            std::ifstream file(std::string(argv[2]) + "/Capsule.ds3", std::ios::binary);
            require(bool(file), "Reviewed Capsule fixture missing");
            const std::vector<uint8_t> bytes { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
            require(bytes.size() <= 1024 * 1024, "Capsule input bound");
            geometry(bytes, false);
        } else if (scenario == "baked-unsupported")
            importedBakedUnsupported();
        else
            throw std::runtime_error("Unknown audited FBX scenario");
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}
