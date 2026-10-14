#include <Dust3DBridge.h>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
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

MeshOwner load(const std::string& path)
{
    std::ifstream stream(path, std::ios::binary);
    require(stream.good(), "Cannot read fixture: " + path);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    Dust3DError error {};
    MeshOwner mesh(dust3d_mesh_load(bytes.data(), bytes.size(), &error), dust3d_mesh_destroy);
    require(mesh != nullptr, std::string("Fixture generation failed: ") + error.message);
    require(error.status == DUST3D_STATUS_OK, "Successful load reported an error");
    return mesh;
}

void verify(const Dust3DMesh* mesh, size_t nodes, size_t parts)
{
    require(dust3d_mesh_node_count(mesh) == nodes, "Source node count mismatch");
    require(dust3d_mesh_part_count(mesh) == parts, "Source part count mismatch");
    const size_t count = dust3d_mesh_vertex_count(mesh);
    require(count > 0 && count % 3 == 0, "Not a nonempty triangle buffer");
    const auto* vertices = dust3d_mesh_vertices(mesh);
    require(vertices != nullptr, "Missing vertex buffer");
    bool hasColor = false;
    for (size_t index = 0; index < count; ++index) {
        const auto& vertex = vertices[index];
        float normalLengthSquared = 0;
        for (size_t axis = 0; axis < 3; ++axis) {
            require(std::isfinite(vertex.position[axis]), "Nonfinite position");
            require(std::isfinite(vertex.normal[axis]), "Nonfinite normal");
            normalLengthSquared += vertex.normal[axis] * vertex.normal[axis];
        }
        require(std::abs(normalLengthSquared - 1.0f) < 0.001f, "Normal is not normalized");
        for (float channel : vertex.color)
            require(std::isfinite(channel) && channel >= 0 && channel <= 1, "Invalid color channel");
        hasColor |= vertex.color[0] != vertex.color[1] || vertex.color[1] != vertex.color[2];
    }
    require(hasColor, "Fixture component colors were lost");
}
}

int main(int argc, char** argv)
{
    try {
        require(argc == 3, "Expected scenario and fixture directory");
        const std::string scenario(argv[1]);
        const std::string fixtures(argv[2]);
        if (scenario == "null_handles") {
            require(dust3d_mesh_vertices(nullptr) == nullptr, "Null mesh exposes vertices");
            require(dust3d_mesh_vertex_count(nullptr) == 0, "Null vertex count");
            require(dust3d_mesh_node_count(nullptr) == 0, "Null node count");
            require(dust3d_mesh_part_count(nullptr) == 0, "Null part count");
            require(std::strlen(dust3d_mesh_warning(nullptr)) == 0, "Null warning");
            dust3d_mesh_destroy(nullptr);
        } else if (scenario == "invalid_archive") {
            const uint8_t bytes[] = { 'n', 'o', 't', 'd', 's', '3' };
            Dust3DError error {};
            MeshOwner mesh(dust3d_mesh_load(bytes, sizeof(bytes), &error), dust3d_mesh_destroy);
            require(!mesh, "Invalid archive accepted");
            require(error.status == DUST3D_STATUS_INVALID_DOCUMENT, "Wrong invalid archive status");
            require(error.message[0] != '\0', "Diagnostic was lost");
            require(std::memchr(error.message, '\0', sizeof(error.message)) != nullptr, "Unterminated diagnostic");
        } else if (scenario == "input_limit") {
            const uint8_t byte = 0;
            Dust3DError error {};
            MeshOwner mesh(dust3d_mesh_load(&byte, 256ULL * 1024 * 1024 + 1, &error), dust3d_mesh_destroy);
            require(!mesh && error.status == DUST3D_STATUS_LIMIT_EXCEEDED, "Limit must reject before reading input");
        } else if (scenario == "capsule") {
            auto mesh = load(fixtures + "/Capsule.ds3");
            verify(mesh.get(), 3, 1);
        } else if (scenario == "sword") {
            auto mesh = load(fixtures + "/Sword.ds3");
            verify(mesh.get(), 18, 5);
        } else if (scenario == "ownership") {
            auto first = load(fixtures + "/Capsule.ds3");
            const size_t count = dust3d_mesh_vertex_count(first.get());
            const auto* pointer = dust3d_mesh_vertices(first.get());
            std::vector<Dust3DVertex> original(pointer, pointer + count);
            auto second = load(fixtures + "/Sword.ds3");
            second.reset();
            require(dust3d_mesh_vertices(first.get()) == pointer, "Another load invalidated owner buffer");
            require(dust3d_mesh_vertex_count(first.get()) == count, "Another load changed owner count");
            require(std::memcmp(pointer, original.data(), count * sizeof(Dust3DVertex)) == 0, "Another load changed owner contents");
        } else {
            throw std::runtime_error("Unknown scenario: " + scenario);
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
