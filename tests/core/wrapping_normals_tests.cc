#include <Dust3DBridge.h>
#include <array>
#include <cmath>
#include <dust3d/base/vector3.h>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using MeshOwner = std::unique_ptr<Dust3DMesh, decltype(&dust3d_mesh_destroy)>;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void angles()
{
    const double pi = std::acos(-1.0);
    // This normalized self-dot rounds to 1.0000000000000002 on the reproducing host.
    const dust3d::Vector3 reproducer(1.0 / 7, 1.0 / 11, 2.0 / 13);
    require(std::isfinite(dust3d::Vector3::angleBetween(reproducer, reproducer)), "Parallel finite vectors must have a finite angle");
    for (int x = 1; x <= 8; ++x) {
        for (int y = 1; y <= 8; ++y) {
            for (int z = 1; z <= 8; ++z) {
                const dust3d::Vector3 value(x / 7.0, y / 11.0, z / 13.0);
                const auto parallel = dust3d::Vector3::angleBetween(value, value);
                const auto opposite = dust3d::Vector3::angleBetween(value, value * -1);
                require(std::isfinite(parallel) && std::abs(parallel) < 1e-7, "Parallel angle must remain zero within rounding tolerance");
                require(std::isfinite(opposite) && std::abs(opposite - pi) < 1e-7, "Opposite angle must remain pi within rounding tolerance");
                const auto nearby = dust3d::Vector3::angleBetween(value, value + dust3d::Vector3(1e-12, -1e-12, 1e-12));
                require(std::isfinite(nearby) && nearby >= 0 && nearby <= pi, "Near-parallel angle must remain finite and bounded");
            }
        }
    }
    require(std::abs(dust3d::Vector3::angleBetween({ 1, 0, 0 }, { 0, 1, 0 }) - pi / 2) < 1e-12, "Orthogonal angle changed");
    require(std::abs(dust3d::Vector3::angleBetween({}, {}) - pi / 2) < 1e-12, "Preserve inherited zero-vector behavior");
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

size_t components(const std::map<size_t, std::set<size_t>>& graph)
{
    std::set<size_t> visited;
    size_t count = 0;
    for (const auto& entry : graph) {
        if (!visited.insert(entry.first).second)
            continue;
        ++count;
        std::vector<size_t> stack { entry.first };
        while (!stack.empty()) {
            const auto vertex = stack.back();
            stack.pop_back();
            for (const auto neighbor : graph.at(vertex)) {
                if (visited.insert(neighbor).second)
                    stack.push_back(neighbor);
            }
        }
    }
    return count;
}

void verifyOpeningTopology(const std::vector<Dust3DVertex>& vertices)
{
    using Position = std::array<float, 3>;
    struct EdgeUse {
        size_t count = 0;
        int orientation = 0;
    };
    std::map<Position, size_t> positions;
    std::map<std::pair<size_t, size_t>, EdgeUse> edges;
    std::map<size_t, std::set<size_t>> graph;
    for (size_t index = 0; index < vertices.size(); index += 3) {
        dust3d::Vector3 points[3];
        size_t identities[3];
        for (size_t corner = 0; corner < 3; ++corner) {
            const auto& value = vertices[index + corner];
            points[corner] = { value.position[0], value.position[1], value.position[2] };
            const Position position { value.position[0], value.position[1], value.position[2] };
            identities[corner] = positions.emplace(position, positions.size()).first->second;
        }
        const auto areaVector = dust3d::Vector3::crossProduct(points[1] - points[0], points[2] - points[0]);
        require(areaVector.lengthSquared() > 0, "An opening triangle must have positive area at exported precision");
        for (size_t corner = 0; corner < 3; ++corner) {
            const auto from = identities[corner];
            const auto to = identities[(corner + 1) % 3];
            const bool forward = from < to;
            auto& edge = edges[forward ? std::make_pair(from, to) : std::make_pair(to, from)];
            ++edge.count;
            edge.orientation += forward ? 1 : -1;
            graph[from].insert(to);
            graph[to].insert(from);
        }
    }
    std::map<size_t, std::set<size_t>> boundary;
    for (const auto& entry : edges) {
        require(entry.second.count <= 2, "Opening edges must not be nonmanifold");
        if (entry.second.count == 2) {
            require(entry.second.orientation == 0, "Shared opening edges must have opposing winding");
        } else {
            boundary[entry.first.first].insert(entry.first.second);
            boundary[entry.first.second].insert(entry.first.first);
        }
    }
    for (const auto& entry : boundary)
        require(entry.second.size() == 2, "Opening rim must have exactly two boundary neighbors");
    require(components(boundary) == 1, "The cloth hem must retain one boundary loop");
    require(components(graph) == 2, "Keep-children must retain exactly the body and garment components");
}

void opening(const std::string& property, const std::string& value, int faces)
{
    const std::string part = "{00000100-0000-4000-8000-000000000100}";
    const auto xml = "<canvas originX='.5' originY='.5' originZ='.5'><nodes><node id='{00001000-0000-4000-8000-000000001000}' partId='" + part
        + "' x='.5' y='.5' z='.5' radius='.2'/></nodes><edges/><parts><part id='" + part
        + "'/></parts><components><component id='{00000300-0000-4000-8000-000000000300}' wrap='Cloth' wrapFaces='" + std::to_string(faces)
        + "' " + property + "='" + value + "'><component id='{00000200-0000-4000-8000-000000000200}' linkDataType='partId' linkData='" + part
        + "'/></component></components></canvas>";
    const auto bytes = archive(xml);
    Dust3DError error {};
    MeshOwner mesh(dust3d_mesh_load(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size(), &error), dust3d_mesh_destroy);
    require(mesh && error.status == DUST3D_STATUS_OK, property + "=" + value + " at " + std::to_string(faces) + " faces: " + error.message);
    require(dust3d_mesh_node_count(mesh.get()) == 1 && dust3d_mesh_part_count(mesh.get()) == 1, "Opening changed source graph counts");
    const auto count = dust3d_mesh_vertex_count(mesh.get());
    const auto* data = dust3d_mesh_vertices(mesh.get());
    require(count > 0 && count % 3 == 0 && data, "Opening must produce triangles");
    const std::vector<Dust3DVertex> vertices(data, data + count);
    mesh.reset();
    for (const auto& vertex : vertices) {
        double normalSquared = 0;
        for (size_t axis = 0; axis < 3; ++axis) {
            require(std::isfinite(vertex.position[axis]) && std::isfinite(vertex.normal[axis]), "Opening geometry must remain finite after owner destruction");
            normalSquared += static_cast<double>(vertex.normal[axis]) * vertex.normal[axis];
        }
        require(std::abs(std::sqrt(normalSquared) - 1) < 1e-4, "Opening normal must have unit length");
        for (const auto channel : vertex.color)
            require(std::isfinite(channel) && channel >= 0 && channel <= 1, "Opening color must remain finite and bounded");
    }
    verifyOpeningTopology(vertices);
}

void precision()
{
    auto model = [](const std::string& center) {
        return "<canvas originX='0' originY='0' originZ='0'><nodes><node id='{00001000-0000-4000-8000-000000001000}' partId='{00000100-0000-4000-8000-000000000100}' x='" + center + "' y='" + center + "' z='" + center + "' radius='.001'/></nodes><edges/><parts><part id='{00000100-0000-4000-8000-000000000100}'/></parts><components><component id='{00000200-0000-4000-8000-000000000200}' linkDataType='partId' linkData='{00000100-0000-4000-8000-000000000100}'/></components></canvas>";
    };
    Dust3DError error {};
    const auto farBytes = archive(model("1000000"));
    MeshOwner collapsed(dust3d_mesh_load(reinterpret_cast<const uint8_t*>(farBytes.data()), farBytes.size(), &error), dust3d_mesh_destroy);
    require(!collapsed && error.status == DUST3D_STATUS_GENERATION_FAILED, "An entirely collapsed exported mesh must fail generation; status=" + std::to_string(error.status) + " vertices=" + std::to_string(dust3d_mesh_vertex_count(collapsed.get())));
    require(std::string(error.message).find("precision") != std::string::npos, "Precision failure must retain its specific cause");
    const auto nearBytes = archive(model("0"));
    MeshOwner near(dust3d_mesh_load(reinterpret_cast<const uint8_t*>(nearBytes.data()), nearBytes.size(), &error), dust3d_mesh_destroy);
    require(near && error.status == DUST3D_STATUS_OK && error.message[0] == '\0', "Small representable geometry must remain supported and clear the previous error");
    const auto* data = dust3d_mesh_vertices(near.get());
    const auto count = dust3d_mesh_vertex_count(near.get());
    require(data && count > 0 && count % 3 == 0, "Representable small geometry must retain triangles");
    for (size_t index = 0; index < count; index += 3) {
        dust3d::Vector3 points[3];
        for (size_t corner = 0; corner < 3; ++corner)
            points[corner] = { data[index + corner].position[0], data[index + corner].position[1], data[index + corner].position[2] };
        require(dust3d::Vector3::crossProduct(points[1] - points[0], points[2] - points[0]).lengthSquared() > 0, "Small supported triangles must have positive exported area");
    }
}

void openings()
{
    for (const auto* property : { "wrapOpenTop", "wrapOpenBottom" }) {
        opening(property, ".225", 64);
        opening(property, ".225", 256);
        opening(property, ".1", 64);
    }
}
}

int main(int argc, char** argv)
{
    try {
        require(argc == 2, "Expected angles, openings or precision scenario");
        const std::string scenario = argv[1];
        if (scenario == "angles")
            angles();
        else if (scenario == "openings")
            openings();
        else if (scenario == "precision")
            precision();
        else
            throw std::runtime_error("Unknown wrapping normals scenario");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
