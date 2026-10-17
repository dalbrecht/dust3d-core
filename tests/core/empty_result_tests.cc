#include <Dust3DBridge.h>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
using MeshOwner = std::unique_ptr<Dust3DMesh, decltype(&dust3d_mesh_destroy)>;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

std::string identity(unsigned int value)
{
    std::ostringstream stream;
    stream << '{' << std::setfill('0') << std::setw(8) << value << "-0000-4000-8000-" << std::setw(12) << value << '}';
    return stream.str();
}

// Synthetic isolated diamond loop. Its fill is the document's only triangle source.
std::string loopModel(bool filled)
{
    std::ostringstream stream;
    stream << "<canvas originX='.5' originY='.5' originZ='.5'><nodes>";
    const double positions[4][2] = { { .9, .5 }, { .5, .9 }, { .1, .5 }, { .5, .1 } };
    for (unsigned int index = 0; index < 4; ++index)
        stream << "<node id='" << identity(index + 1) << "' partId='" << identity(100)
               << "' x='" << positions[index][0] << "' y='" << positions[index][1] << "' z='.5' radius='.05'/>";
    stream << "</nodes><edges>";
    for (unsigned int index = 0; index < 4; ++index)
        stream << "<edge id='" << identity(index + 10) << "' partId='" << identity(100)
               << "' from='" << identity(index + 1) << "' to='" << identity((index + 1) % 4 + 1) << "'/>";
    stream << "</edges><parts><part id='" << identity(100) << "' target='StitchingLoop' fillLoopInterior='" << (filled ? "true" : "false")
           << "'/></parts><components><component id='" << identity(300) << "'><component id='" << identity(200)
           << "' linkDataType='partId' linkData='" << identity(100) << "'/></component></components></canvas>";
    return stream.str();
}

// This straight-profile surface previously failed due to the unclamped angle calculation.
std::string straightStitchModel()
{
    std::ostringstream nodes;
    std::ostringstream edges;
    std::ostringstream parts;
    std::ostringstream components;
    const double positions[3][2] = { { .8, .5 }, { .35, .76 }, { .35, .24 } };
    for (unsigned int part = 0; part < 3; ++part) {
        for (unsigned int node = 0; node < 3; ++node) {
            nodes << "<node id='" << identity(1000 + part * 10 + node) << "' partId='" << identity(100 + part)
                  << "' x='" << positions[part][0] << "' y='" << positions[part][1] << "' z='" << node * .5 << "' radius='.05'/>";
            if (node > 0)
                edges << "<edge id='" << identity(2000 + part * 10 + node) << "' partId='" << identity(100 + part)
                      << "' from='" << identity(1000 + part * 10 + node - 1) << "' to='" << identity(1000 + part * 10 + node) << "'/>";
        }
        parts << "<part id='" << identity(100 + part) << "' target='StitchingLine'/>";
        components << "<component id='" << identity(200 + part) << "' linkDataType='partId' linkData='" << identity(100 + part) << "'/>";
    }
    return "<canvas originX='.5' originY='.5' originZ='.5'><nodes>" + nodes.str() + "</nodes><edges>" + edges.str()
        + "</edges><parts>" + parts.str() + "</parts><components><component id='" + identity(300) + "' sideClosed='true'>"
        + components.str() + "</component></components></canvas>";
}

// A negative legacy offset removes the complete wrapping surface, so generation genuinely fails.
std::string failedWrapModel()
{
    return "<canvas originX='.5' originY='.5' originZ='.5'><nodes><node id='{00001000-0000-4000-8000-000000001000}' partId='{00000100-0000-4000-8000-000000000100}' x='.5' y='.5' z='.5' radius='.2'/></nodes><edges/><parts><part id='{00000100-0000-4000-8000-000000000100}'/></parts><components><component id='{00000300-0000-4000-8000-000000000300}' wrap='Cloth' wrapFaces='64' wrapOffset='-1'><component id='{00000200-0000-4000-8000-000000000200}' linkDataType='partId' linkData='{00000100-0000-4000-8000-000000000100}'/></component></components></canvas>";
}

std::string archive(const std::string& xml)
{
    const std::string prefix = "DUST3D 1.0 xml ";
    const std::string header = "<ds3><model name='model.xml' offset='0' size='" + std::to_string(xml.size()) + "'/></ds3>";
    std::ostringstream stream;
    stream << prefix << std::setfill('0') << std::setw(10) << prefix.size() + 12 + header.size() << "\r\n"
           << header << xml;
    return stream.str();
}

MeshOwner load(const std::string& bytes, Dust3DError* error)
{
    return MeshOwner(dust3d_mesh_load(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size(), error), dust3d_mesh_destroy);
}
}

int main()
{
    try {
        Dust3DError error {};
        const auto filledBytes = archive(loopModel(true));
        auto filled = load(filledBytes, &error);
        require(filled && error.status == DUST3D_STATUS_OK, "Filled isolated loop must generate successfully");
        const auto count = dust3d_mesh_vertex_count(filled.get());
        const auto* vertices = dust3d_mesh_vertices(filled.get());
        require(count > 0 && count % 3 == 0 && vertices, "Filled loop must own nonempty triangle data");
        const Dust3DVertex first = vertices[0];
        const auto emptyBytes = archive(loopModel(false));
        auto empty = load(emptyBytes, &error);
        require(!empty && error.status == DUST3D_STATUS_NO_GEOMETRY, "Unfilled isolated loop must report successful no-geometry, not a generation failure");
        require(error.message[0] != '\0', "No-geometry result must remain descriptive for older callers");
        require(!load(emptyBytes, nullptr), "No-geometry accepts an omitted diagnostic owner");
        for (size_t axis = 0; axis < 3; ++axis)
            require(std::isfinite(vertices[0].position[axis]) && vertices[0].position[axis] == first.position[axis], "An empty load must not invalidate another owner's borrowed buffer");
        require(dust3d_mesh_node_count(filled.get()) == 4 && dust3d_mesh_part_count(filled.get()) == 1, "Filled owner source counts must survive empty loads");
        auto invalid = load("not a ds3 archive", &error);
        require(!invalid && error.status == DUST3D_STATUS_INVALID_DOCUMENT, "Malformed input must not become no-geometry");
        auto straight = load(archive(straightStitchModel()), &error);
        require(straight && error.status == DUST3D_STATUS_OK, "Straight-profile generation must not fail through rounded angle input");
        const auto* straightVertices = dust3d_mesh_vertices(straight.get());
        const auto straightCount = dust3d_mesh_vertex_count(straight.get());
        require(straightVertices && straightCount > 0 && straightCount % 3 == 0, "Straight profiles must retain triangle output");
        for (size_t index = 0; index < straightCount; ++index) {
            for (size_t axis = 0; axis < 3; ++axis)
                require(std::isfinite(straightVertices[index].position[axis]) && std::isfinite(straightVertices[index].normal[axis]), "Repaired straight-profile output must remain finite");
        }
        auto failed = load(archive(failedWrapModel()), &error);
        require(!failed && error.status == DUST3D_STATUS_GENERATION_FAILED, "A failed wrap must remain a genuine generation failure");
        require(error.message[0] != '\0', "Generation failure must retain its original diagnostic");
        auto restored = load(filledBytes, &error);
        require(restored && error.status == DUST3D_STATUS_OK && error.message[0] == '\0', "A later success must reset the diagnostic and generate again");
        require(dust3d_mesh_vertex_count(restored.get()) == count, "Refilling must restore triangle counts");
        std::cout << "Successful empty generation and ownership contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
