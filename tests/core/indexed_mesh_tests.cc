#include "EngineInternal.h"
#include <Dust3DBridge.h>
#include <array>
#include <cmath>
#include <cstring>
#include <future>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using MeshOwner = std::unique_ptr<Dust3DMesh, decltype(&dust3d_mesh_destroy)>;
using IndexedOwner = std::unique_ptr<Dust3DIndexedMesh, decltype(&dust3d_indexed_mesh_destroy)>;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

std::string archive()
{
    // Independent MIT synthetic planar loop: two coplanar triangles share four vertices.
    const std::string part = "{00000100-0000-4000-8000-000000000100}";
    const std::array<std::string, 4> nodes {
        "{00001000-0000-4000-8000-000000001000}", "{00001001-0000-4000-8000-000000001001}",
        "{00001002-0000-4000-8000-000000001002}", "{00001003-0000-4000-8000-000000001003}"
    };
    const double positions[4][2] = { { .9, .5 }, { .5, .9 }, { .1, .5 }, { .5, .1 } };
    std::ostringstream xml;
    xml << "<canvas originX='.5' originY='.5' originZ='.5'><nodes>";
    for (size_t index = 0; index < 4; ++index)
        xml << "<node id='" << nodes[index] << "' partId='" << part << "' x='" << positions[index][0] << "' y='" << positions[index][1] << "' z='.5' radius='.05'/>";
    xml << "</nodes><edges>";
    for (size_t index = 0; index < 4; ++index)
        xml << "<edge id='{0000200" << index << "-0000-4000-8000-00000000200" << index << "}' partId='" << part << "' from='" << nodes[index] << "' to='" << nodes[(index + 1) % 4] << "'/>";
    xml << "</edges><parts><part id='" << part << "' target='StitchingLoop' fillLoopInterior='true'/></parts><components><component id='{00000300-0000-4000-8000-000000000300}' color='#3366cc' wrapPattern='Spots'><component id='{00000200-0000-4000-8000-000000000200}' linkDataType='partId' linkData='" << part << "'/></component></components></canvas>";
    const auto model = xml.str();
    const std::string prefix = "DUST3D 1.0 xml ";
    const std::string header = "<ds3><model name='model.xml' offset='0' size='" + std::to_string(model.size()) + "'/></ds3>";
    std::ostringstream bytes;
    bytes << prefix << std::setfill('0') << std::setw(10) << prefix.size() + 12 + header.size() << "\r\n"
          << header << model;
    return bytes.str();
}

MeshOwner load()
{
    const auto bytes = archive();
    Dust3DError error {};
    MeshOwner mesh(dust3d_mesh_load(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size(), &error), dust3d_mesh_destroy);
    require(mesh && error.status == DUST3D_STATUS_OK, std::string("Indexed source fixture failed: ") + error.message);
    return mesh;
}

IndexedOwner convert(const Dust3DMesh* mesh, size_t maximumBytes = 0)
{
    Dust3DError error {};
    IndexedOwner indexed(dust3d_mesh_create_indexed(mesh, maximumBytes, &error), dust3d_indexed_mesh_destroy);
    require(indexed && error.status == DUST3D_STATUS_OK && error.message[0] == '\0', std::string("Indexed conversion failed: ") + error.message);
    return indexed;
}

bool equalVertex(const Dust3DVertex& first, const Dust3DVertex& second)
{
    for (size_t axis = 0; axis < 3; ++axis) {
        if (first.position[axis] != second.position[axis] || first.normal[axis] != second.normal[axis])
            return false;
    }
    for (size_t channel = 0; channel < 4; ++channel) {
        if (first.color[channel] != second.color[channel])
            return false;
    }
    return true;
}

void verify(const Dust3DMesh* source, const Dust3DIndexedMesh* indexed)
{
    const auto count = dust3d_mesh_vertex_count(source);
    const auto* expanded = dust3d_mesh_vertices(source);
    const auto vertexCount = dust3d_indexed_mesh_vertex_count(indexed);
    const auto* vertices = dust3d_indexed_mesh_vertices(indexed);
    const auto indexCount = dust3d_indexed_mesh_index_count(indexed);
    const auto* indices = dust3d_indexed_mesh_indices(indexed);
    require(expanded && vertices && indices && count > 0 && count % 3 == 0, "Indexed buffers must be nonempty triangles");
    require(indexCount == count && vertexCount <= count && vertexCount > 0, "Indexed conversion changed triangle count or added vertices");
    for (size_t index = 0; index < count; ++index) {
        require(indices[index] < vertexCount, "Indexed buffer references a missing vertex");
        require(equalVertex(expanded[index], vertices[indices[index]]), "Reconstructing indices must preserve every vertex attribute value");
    }
    require(dust3d_indexed_mesh_node_count(indexed) == dust3d_mesh_node_count(source), "Indexed conversion changed node count");
    require(dust3d_indexed_mesh_part_count(indexed) == dust3d_mesh_part_count(source), "Indexed conversion changed part count");
    require(std::strcmp(dust3d_indexed_mesh_warning(indexed), dust3d_mesh_warning(source)) == 0, "Indexed conversion lost the source warning");
}

void nullHandles()
{
    Dust3DError error {};
    require(!dust3d_mesh_create_indexed(nullptr, 0, &error) && error.status == DUST3D_STATUS_INVALID_DOCUMENT && error.message[0] != '\0', "Null indexed source must reject descriptively");
    require(!dust3d_mesh_create_indexed(nullptr, 0, nullptr), "Omitted error owner must be safe");
    require(!dust3d_indexed_mesh_vertices(nullptr) && !dust3d_indexed_mesh_indices(nullptr), "Null buffers must be null");
    require(dust3d_indexed_mesh_vertex_count(nullptr) == 0 && dust3d_indexed_mesh_index_count(nullptr) == 0, "Null buffer counts must be zero");
    require(dust3d_indexed_mesh_node_count(nullptr) == 0 && dust3d_indexed_mesh_part_count(nullptr) == 0, "Null source counts must be zero");
    require(dust3d_indexed_mesh_warning(nullptr)[0] == '\0', "Null warning must be empty");
    dust3d_indexed_mesh_destroy(nullptr);
}

void reconstruction()
{
    auto source = load();
    auto indexed = convert(source.get());
    verify(source.get(), indexed.get());
    require(dust3d_indexed_mesh_vertex_count(indexed.get()) < dust3d_mesh_vertex_count(source.get()), "Planar fixture must reuse shared full-attribute vertices");
    require(dust3d_indexed_mesh_node_count(indexed.get()) == 4 && dust3d_indexed_mesh_part_count(indexed.get()) == 1, "Fixture source counts changed");
    require(dust3d_indexed_mesh_warning(indexed.get())[0] != '\0', "Fixture must exercise warning copying");
    auto repeated = convert(source.get());
    const auto count = dust3d_indexed_mesh_index_count(indexed.get());
    for (size_t index = 0; index < count; ++index)
        require(dust3d_indexed_mesh_indices(indexed.get())[index] == dust3d_indexed_mesh_indices(repeated.get())[index], "Repeated conversion of identical input must keep stable indices");
}

void ownership()
{
    auto source = load();
    auto indexed = convert(source.get());
    const auto count = dust3d_indexed_mesh_index_count(indexed.get());
    const auto vertexCount = dust3d_indexed_mesh_vertex_count(indexed.get());
    const auto* vertices = dust3d_indexed_mesh_vertices(indexed.get());
    const auto* indices = dust3d_indexed_mesh_indices(indexed.get());
    const std::vector<Dust3DVertex> copiedVertices(vertices, vertices + vertexCount);
    const std::vector<uint32_t> copiedIndices(indices, indices + count);
    const std::string warning = dust3d_indexed_mesh_warning(indexed.get());
    source.reset();
    auto unrelated = load();
    auto second = convert(unrelated.get());
    second.reset();
    for (size_t index = 0; index < vertexCount; ++index)
        require(equalVertex(vertices[index], copiedVertices[index]), "Indexed vertices must outlive the source owner and unrelated conversions");
    for (size_t index = 0; index < count; ++index)
        require(indices[index] == copiedIndices[index], "Indexed indices must outlive the source owner");
    require(warning == dust3d_indexed_mesh_warning(indexed.get()), "Indexed warning must outlive the source owner");
    require(dust3d_indexed_mesh_node_count(indexed.get()) == 4 && dust3d_indexed_mesh_part_count(indexed.get()) == 1, "Indexed source counts must outlive the source owner");
}

void budget()
{
    auto source = load();
    auto baseline = convert(source.get());
    const auto bytes = dust3d_indexed_mesh_vertex_count(baseline.get()) * sizeof(Dust3DVertex)
        + dust3d_indexed_mesh_index_count(baseline.get()) * sizeof(uint32_t);
    verify(source.get(), convert(source.get(), bytes).get());
    for (const auto limit : { size_t { 1 }, bytes - 1 }) {
        Dust3DError error {};
        IndexedOwner rejected(dust3d_mesh_create_indexed(source.get(), limit, &error), dust3d_indexed_mesh_destroy);
        require(!rejected && error.status == DUST3D_STATUS_LIMIT_EXCEEDED && error.message[0] != '\0', "Insufficient payload budget must reject without partial ownership");
        require(!dust3d_mesh_create_indexed(source.get(), limit, nullptr), "Budget failure must accept an omitted error owner");
    }
    verify(source.get(), convert(source.get()).get());
}

Dust3DVertex vertex(float x, float y, float z, float normalX = 0, float normalZ = 1, float red = 1, float blue = 0)
{
    return { { x, y, z }, { normalX, 0, normalZ }, { red, 0, blue, 1 } };
}

void attributeSeams()
{
    Dust3DMesh source;
    source.nodeCount = 3;
    source.partCount = 2;
    source.warning = "Synthetic attribute seam fixture";
    const auto first = vertex(0, 0, 0);
    const auto second = vertex(1, 0, 0);
    const auto third = vertex(0, 1, 0);
    source.vertices = {
        first, second, third,
        vertex(0, 0, 0, 1, 0), vertex(0, 1, 0, 1, 0), vertex(0, 0, 1, 1, 0),
        vertex(0, 0, 0, 0, 1, 0, 1), vertex(1, 0, 0, 0, 1, 0, 1), vertex(0, 1, 0, 0, 1, 0, 1),
        first, second, third
    };
    auto indexed = dust3d::native::buildIndexedMesh(source, 0);
    verify(&source, indexed.get());
    require(dust3d_indexed_mesh_vertex_count(indexed.get()) == 9, "Hard-normal and color seams must retain distinct full-attribute vertices");
    const auto* indices = dust3d_indexed_mesh_indices(indexed.get());
    require(indices[0] != indices[3] && indices[0] != indices[6], "Shared positions cannot merge different normal/color values");
    require(indices[0] == indices[9] && indices[1] == indices[10] && indices[2] == indices[11], "Identical complete vertices must be reused");
    source.vertices[9].position[0] = -0.0f;
    verify(&source, dust3d::native::buildIndexedMesh(source, 0).get());
}

void concurrentConversions()
{
    auto source = load();
    std::vector<std::future<void>> conversions;
    for (size_t worker = 0; worker < 4; ++worker) {
        conversions.push_back(std::async(std::launch::async, [mesh = source.get()] {
            for (size_t iteration = 0; iteration < 50; ++iteration)
                verify(mesh, convert(mesh).get());
        }));
    }
    auto unrelated = load();
    verify(unrelated.get(), convert(unrelated.get()).get());
    for (auto& conversion : conversions)
        conversion.get();
}

void invalidAttributes()
{
    for (int attribute = 0; attribute < 3; ++attribute) {
        Dust3DMesh source;
        source.vertices = { vertex(0, 0, 0), vertex(1, 0, 0), vertex(0, 1, 0) };
        if (attribute == 0)
            source.vertices[0].position[0] = std::numeric_limits<float>::quiet_NaN();
        else if (attribute == 1)
            source.vertices[0].normal[1] = std::numeric_limits<float>::infinity();
        else
            source.vertices[0].color[2] = std::numeric_limits<float>::quiet_NaN();
        bool rejected = false;
        try {
            dust3d::native::buildIndexedMesh(source, 0);
        } catch (const dust3d::native::DocumentError& error) {
            rejected = error.status() == DUST3D_STATUS_GENERATION_FAILED && error.what()[0] != '\0';
        }
        require(rejected, "Nonfinite converter attributes must fail before ordered-key comparison");
    }
    for (const size_t count : { 0, 2 }) {
        Dust3DMesh incomplete;
        incomplete.vertices.resize(count);
        bool rejected = false;
        try {
            dust3d::native::buildIndexedMesh(incomplete, 0);
        } catch (const dust3d::native::DocumentError& error) {
            rejected = error.status() == DUST3D_STATUS_INVALID_DOCUMENT;
        }
        require(rejected, "Empty or incomplete converter triangles must reject");
    }
}
}

int main(int argc, char** argv)
{
    try {
        require(argc == 2, "Expected indexed mesh scenario");
        const std::string scenario = argv[1];
        if (scenario == "null_handles")
            nullHandles();
        else if (scenario == "reconstruction")
            reconstruction();
        else if (scenario == "ownership")
            ownership();
        else if (scenario == "budget")
            budget();
        else if (scenario == "attribute_seams")
            attributeSeams();
        else if (scenario == "invalid_attributes")
            invalidAttributes();
        else if (scenario == "concurrent")
            concurrentConversions();
        else
            throw std::runtime_error("Unknown indexed mesh scenario");
        std::cout << "Indexed mesh contract passed: " << scenario << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
