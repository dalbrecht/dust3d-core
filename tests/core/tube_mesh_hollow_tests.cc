// SPDX-License-Identifier: MIT
#include <algorithm>
#include <cmath>
#include <dust3d/base/position_key.h>
#include <dust3d/mesh/section_remesher.h>
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

void tiltedConcaveCapHasConformingTriangleEdges()
{
    // The first cap of a saved linked L-profile tilted by 14 degrees.
    const std::vector<dust3d::Vector3> ring {
        { -.10000000149011612, -.33895155011488937, -.44610383549011451 },
        { -.10000000149011612, -.24192190170288086, -.47029572725296021 },
        { 0, -.24192190170288086, -.47029572725296021 },
        { 0, -.14489225329087235, -.49448761901580590 },
        { .10000000149011612, -.14489225329087235, -.49448761901580590 },
        { .10000000149011612, -.33895155011488937, -.44610383549011451 },
    };
    dust3d::SectionRemesher remesher(ring, 0, 1);
    remesher.remesh();
    const auto& triangles = remesher.generatedFaces();
    require(triangles.size() == ring.size() - 2, "Concave cap triangulation must retain the collinear interior diagonal corner");
    require(remesher.generatedFaceUvs().size() == triangles.size(), "Every cap triangle retains UVs");
    std::map<std::pair<size_t, size_t>, int> edges;
    for (const auto& triangle : triangles) {
        const auto& a = ring.at(triangle.at(0));
        const auto& b = ring.at(triangle.at(1));
        const auto& c = ring.at(triangle.at(2));
        require(dust3d::Vector3::crossProduct(b - a, c - a).lengthSquared() > 1e-12, "Cap triangles have positive area");
        for (size_t i = 0; i < 3; ++i) {
            const auto from = triangle[i];
            const auto to = triangle[(i + 1) % 3];
            ++edges[{ from, to }];
            --edges[{ to, from }];
        }
    }
    for (size_t i = 0; i < ring.size(); ++i) {
        const size_t next = (i + 1) % ring.size();
        --edges[{ i, next }];
        ++edges[{ next, i }];
    }
    for (const auto& edge : edges)
        require(edge.second == 0, "Cap interior edges pair exactly and the boundary follows the authored ring");
}

void customHollowProfileContainsCavity(const std::vector<dust3d::Vector2>& profile, double thickness)
{
    dust3d::TubeMeshBuilder::BuildParameters parameters;
    parameters.cutFace = profile;
    parameters.interpolationEnabled = false;
    parameters.hollowThickness = thickness;
    dust3d::TubeMeshBuilder builder(parameters, { { { 0, 0, 0 }, 1, {} }, { { 0, 0, 2 }, 1, {} } }, false);
    builder.build();
    const auto& vertices = builder.generatedVertices();
    const auto count = profile.size();
    require(vertices.size() == count * 4, "Custom hollow profile must retain both walls");
    const auto side = [](const auto& a, const auto& b, const auto& point) {
        return (b.x() - a.x()) * (point.y() - a.y()) - (b.y() - a.y()) * (point.x() - a.x());
    };
    for (size_t i = 0; i < count; ++i) {
        const auto& point = vertices[count * 2 + i];
        const auto& next = vertices[count * 2 + (i + 1) % count];
        bool inside = false;
        for (size_t j = 0; j < count; ++j) {
            const auto& a = vertices[j];
            const auto& b = vertices[(j + 1) % count];
            const auto cross = side(a, b, point);
            require(!(cross * side(a, b, next) < 0 && side(point, next, a) * side(point, next, b) < 0),
                "Inner edge must not cross the outer contour");
            const auto dot = (point.x() - a.x()) * (point.x() - b.x()) + (point.y() - a.y()) * (point.y() - b.y());
            require(std::abs(cross) > 1e-12 || dot > 0, "Inner vertex must not lie on the outer contour");
            if ((a.y() > point.y()) != (b.y() > point.y())
                && point.x() < a.x() + (point.y() - a.y()) * (b.x() - a.x()) / (b.y() - a.y()))
                inside = !inside;
        }
        require(inside, "Custom profile must strictly contain each inner vertex");
    }
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
    if (thickness > .999999) {
        const auto& firstInner = vertices.at(8);
        const auto& secondInner = vertices.at(9);
        require((firstInner - secondInner).lengthSquared() > 0, "Near-one inner corners must remain distinct in double precision");
        require(dust3d::PositionKey(firstInner) == dust3d::PositionKey(secondInner),
            "Near-one fixture must exercise a legacy position-key collision");
    }
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
        tiltedConcaveCapHasConformingTriangleEdges();
        customHollowProfileContainsCavity({ { -1, -1 }, { 1, -1 }, { -1, 1 } }, .5);
        const std::vector<dust3d::Vector2> concave { { -1, -1 }, { 1, -1 }, { 1, 1 }, { .5, 1 },
            { .5, -.5 }, { -.5, -.5 }, { -.5, 1 }, { -1, 1 } };
        customHollowProfileContainsCavity(concave, .25);
        customHollowProfileContainsCavity(concave, .99);
        customHollowProfileContainsCavity({ concave.rbegin(), concave.rend() }, .25);
        hollowTubeHasConsistentTopology(false);
        hollowTubeHasConsistentTopology(true);
        hollowTubeHasConsistentTopology(false, 0.99999999);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
