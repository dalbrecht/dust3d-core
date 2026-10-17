#include <cmath>
#include <dust3d/mesh/solid_mesh_boolean_operation.h>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
struct Box {
    std::vector<dust3d::Vector3> vertices;
    std::vector<std::vector<size_t>> triangles {
        { 0, 2, 1 }, { 0, 3, 2 }, { 4, 5, 6 }, { 4, 6, 7 },
        { 0, 1, 5 }, { 0, 5, 4 }, { 3, 7, 6 }, { 3, 6, 2 },
        { 0, 4, 7 }, { 0, 7, 3 }, { 1, 2, 6 }, { 1, 6, 5 }
    };
};

Box box(size_t axis, double minimum, double maximum)
{
    Box result;
    for (const auto& corner : std::vector<std::vector<int>> {
             { 0, 0, 0 }, { 1, 0, 0 }, { 1, 1, 0 }, { 0, 1, 0 },
             { 0, 0, 1 }, { 1, 0, 1 }, { 1, 1, 1 }, { 0, 1, 1 } }) {
        double position[3] {};
        for (size_t coordinate = 0; coordinate < 3; ++coordinate)
            position[coordinate] = coordinate == axis ? (corner[coordinate] ? maximum : minimum) : corner[coordinate];
        result.vertices.emplace_back(position[0], position[1], position[2]);
    }
    return result;
}

void verifyThinSolidAcrossZero(size_t axis)
{
    auto first = box(axis, -0.000009, 0.000009);
    auto second = box(axis, 0.1, 0.2);
    dust3d::SolidMesh firstMesh;
    dust3d::SolidMesh secondMesh;
    firstMesh.setVertices(&first.vertices);
    firstMesh.setTriangles(&first.triangles);
    firstMesh.prepare();
    secondMesh.setVertices(&second.vertices);
    secondMesh.setTriangles(&second.triangles);
    secondMesh.prepare();
    dust3d::SolidMeshBooleanOperation operation(&firstMesh, &secondMesh);
    if (!operation.combine())
        throw std::runtime_error("Disjoint thin-solid union failed");
    std::vector<std::vector<size_t>> triangles;
    operation.fetchUnion(triangles);
    if (triangles.size() != 24)
        throw std::runtime_error("Welding discarded triangles from a valid thin solid");
    const auto& vertices = operation.resultVertices();
    double volume = 0;
    for (const auto& triangle : triangles) {
        const auto& a = vertices.at(triangle.at(0));
        const auto& b = vertices.at(triangle.at(1));
        const auto& c = vertices.at(triangle.at(2));
        volume += dust3d::Vector3::dotProduct(a, dust3d::Vector3::crossProduct(b, c)) / 6.0;
    }
    if (std::abs(volume - 0.100018) > 0.000000000001)
        throw std::runtime_error("Thin-solid union volume changed across the zero bucket");
}
}

int main()
{
    try {
        for (size_t axis = 0; axis < 3; ++axis)
            verifyThinSolidAcrossZero(axis);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
