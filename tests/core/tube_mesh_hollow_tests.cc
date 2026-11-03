// SPDX-License-Identifier: MIT
#include <algorithm>
#include <cmath>
#include <dust3d/mesh/tube_mesh_builder.h>
#include <iostream>
#include <map>
#include <stdexcept>
#include <utility>

namespace {
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void hollowTubeHasConsistentTopology(bool circular, double thickness = .5)
{
    dust3d::TubeMeshBuilder::BuildParameters parameters;
    parameters.cutFace = { { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 } };
    parameters.interpolationEnabled = false;
    parameters.hollowThickness = thickness;
    const dust3d::Uuid first("00000000-0000-0000-0000-000000000001");
    const dust3d::Uuid second("00000000-0000-0000-0000-000000000002");
    std::vector<dust3d::MeshNode> nodes { { { 0, 0, 0 }, 1, first }, { { 0, 0, 2 }, 1, second } };
    if (circular)
        nodes.push_back({ { 3, 0, 1 }, 1, first });
    dust3d::TubeMeshBuilder builder(parameters, std::move(nodes), circular);
    builder.build();
    const auto& vertices = builder.generatedVertices();
    const auto& sources = builder.generatedVertexSources();
    const auto& faces = builder.generatedFaces();
    const auto& uvs = builder.generatedFaceUvs();
    require(vertices.size() == (circular ? 24 : 16), "Hollow tube retains an inner and outer ring at each end");
    require(vertices.size() >= 8 && sources.size() == vertices.size(), "Hollow tube has owned vertex sources");
    require(!faces.empty() && faces.size() == uvs.size(), "Each hollow face has UVs");
    require(faces.size() == (circular ? 24 : 16), "Closed centerlines generate walls without terminal rims");
    std::map<std::pair<size_t, size_t>, size_t> edges;
    std::map<std::pair<size_t, size_t>, int> winding;
    for (size_t faceIndex = 0; faceIndex < faces.size(); ++faceIndex) {
        const auto& face = faces[faceIndex];
        require(face.size() >= 3 && face.size() == uvs[faceIndex].size(), "Each face has matching UV corners");
        for (size_t corner = 0; corner < face.size(); ++corner) {
            const auto current = face[corner];
            const auto next = face[(corner + 1) % face.size()];
            require(current < vertices.size() && next < vertices.size() && current != next, "Faces contain valid edges");
            const auto& uv = uvs[faceIndex][corner];
            require(std::isfinite(uv.x()) && std::isfinite(uv.y()), "UV corners are finite");
            const double band = static_cast<double>(faceIndex / (circular ? 12 : 4)) / 4.0;
            require(uv.y() >= band && uv.y() <= band + .25, "Outer, inner and rim faces use separate UV bands");
            ++edges[std::minmax(current, next)];
            winding[std::minmax(current, next)] += current < next ? 1 : -1;
        }
    }
    for (const auto& edge : edges) {
        require(edge.second == 2, "Hollow tube has no boundary or nonmanifold edges");
        require(winding.at(edge.first) == 0, "Adjacent hollow faces wind in opposite edge directions");
    }
    for (size_t index = 0; index < vertices.size(); ++index) {
        const auto& vertex = vertices[index];
        require(std::isfinite(vertex.x()) && std::isfinite(vertex.y()) && std::isfinite(vertex.z()), "Vertices are finite");
        require(sources[index] == first || sources[index] == second, "Vertices retain a source node");
    }
}
}

int main()
{
    try {
        hollowTubeHasConsistentTopology(false);
        hollowTubeHasConsistentTopology(true);
        hollowTubeHasConsistentTopology(false, 0.99999999);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
