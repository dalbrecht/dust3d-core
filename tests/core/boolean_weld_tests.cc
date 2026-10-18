#include <cmath>
#include <dust3d/mesh/mesh_combiner.h>
#include <dust3d/mesh/mesh_recombiner.h>
#include <dust3d/mesh/mesh_state.h>
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

void verifyBooleanTriangleOwnership(int mode)
{
    const bool difference = mode == 1;
    auto first = box(0, 0, 1);
    auto second = box(0, 0.4, 1.4);
    for (auto& vertex : second.vertices)
        vertex = vertex + dust3d::Vector3(0, 0.2, 0.3);
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
        throw std::runtime_error("Intersecting ownership fixture failed");
    std::vector<std::vector<size_t>> triangles;
    std::vector<dust3d::SolidMeshBooleanOperation::TriangleSource> sources;
    if (mode == 2)
        operation.fetchIntersect(triangles, &sources);
    else if (difference)
        operation.fetchDiff(triangles, &sources);
    else
        operation.fetchUnion(triangles, &sources);
    if (triangles.empty() || sources.size() != triangles.size())
        throw std::runtime_error("Boolean triangle ownership is not aligned");
    size_t firstCount = 0;
    size_t secondCount = 0;
    const auto& vertices = operation.resultVertices();
    for (size_t index = 0; index < triangles.size(); ++index) {
        const auto& source = sources[index];
        const auto& input = source.isFirst ? first : second;
        source.isFirst ? ++firstCount : ++secondCount;
        const auto& original = input.triangles.at(source.triangleIndex);
        const auto& a = input.vertices.at(original[0]);
        const auto& b = input.vertices.at(original[1]);
        const auto& c = input.vertices.at(original[2]);
        auto normal = dust3d::Vector3::crossProduct(b - a, c - a).normalized();
        const auto& triangle = triangles[index];
        for (auto vertex : triangle) {
            if (std::abs(dust3d::Vector3::dotProduct(vertices.at(vertex) - a, normal)) > 1e-7)
                throw std::runtime_error("Boolean face was attributed to a different source plane");
            const auto firstEdge = b - a;
            const auto secondEdge = c - a;
            const auto offset = vertices.at(vertex) - a;
            const auto firstSquared = dust3d::Vector3::dotProduct(firstEdge, firstEdge);
            const auto secondSquared = dust3d::Vector3::dotProduct(secondEdge, secondEdge);
            const auto cross = dust3d::Vector3::dotProduct(firstEdge, secondEdge);
            const auto denominator = firstSquared * secondSquared - cross * cross;
            const auto firstDot = dust3d::Vector3::dotProduct(offset, firstEdge);
            const auto secondDot = dust3d::Vector3::dotProduct(offset, secondEdge);
            const auto u = (secondSquared * firstDot - cross * secondDot) / denominator;
            const auto v = (firstSquared * secondDot - cross * firstDot) / denominator;
            if (u < -1e-7 || v < -1e-7 || u + v > 1 + 1e-7)
                throw std::runtime_error("Boolean source index refers to a different coplanar triangle");
        }
        auto outputNormal = dust3d::Vector3::crossProduct(vertices.at(triangle[1]) - vertices.at(triangle[0]),
            vertices.at(triangle[2]) - vertices.at(triangle[0]))
                                .normalized();
        const bool reversed = difference && !source.isFirst;
        if (source.isReversed != reversed || dust3d::Vector3::dotProduct(normal, outputNormal) * (reversed ? -1 : 1) < 0.999)
            throw std::runtime_error("Boolean ownership lost difference winding");
    }
    if (!firstCount || !secondCount)
        throw std::runtime_error("Ownership fixture did not exercise both input meshes");
    if (mode == 2)
        return;
    dust3d::MeshCombiner::Mesh firstCombined(first.vertices, first.triangles);
    dust3d::MeshCombiner::Mesh secondCombined(second.vertices, second.triangles);
    std::vector<dust3d::MeshCombiner::TriangleSource> combinedSources;
    std::vector<std::pair<dust3d::MeshCombiner::Source, size_t>> vertexSources;
    std::unique_ptr<dust3d::MeshCombiner::Mesh> combined(dust3d::MeshCombiner::combine(firstCombined, secondCombined,
        difference ? dust3d::MeshCombiner::Method::Diff : dust3d::MeshCombiner::Method::Union, &vertexSources, &combinedSources));
    if (!combined || combinedSources.size() != sources.size())
        throw std::runtime_error("MeshCombiner discarded exact triangle identity");
    for (size_t index = 0; index < sources.size(); ++index) {
        const auto& actual = combinedSources[index];
        const auto& expected = sources[index];
        if ((actual.source == dust3d::MeshCombiner::Source::First) != expected.isFirst
            || actual.triangleIndex != expected.triangleIndex || actual.isReversed != expected.isReversed)
            throw std::runtime_error("MeshCombiner changed Boolean triangle identity");
    }
    std::vector<dust3d::Vector3> combinedVertices;
    std::vector<std::vector<size_t>> combinedFaces;
    combined->fetch(combinedVertices, combinedFaces);
    dust3d::MeshRecombiner recombiner;
    recombiner.setVertices(&combinedVertices, &vertexSources);
    recombiner.setFaces(&combinedFaces);
    if (!recombiner.recombine())
        throw std::runtime_error("Ownership fixture recombination failed");
    const auto& regeneratedFaces = recombiner.regeneratedFaces();
    const auto& regeneratedVertices = recombiner.regeneratedVertices();
    const auto& regeneratedSources = recombiner.regeneratedFaceSources();
    if (regeneratedSources.size() != regeneratedFaces.size())
        throw std::runtime_error("Recombiner discarded face identity alignment");
    size_t retainedCount = 0;
    for (size_t index = 0; index < regeneratedFaces.size(); ++index) {
        if (!regeneratedSources[index])
            continue;
        ++retainedCount;
        const auto& original = combinedFaces.at(*regeneratedSources[index]);
        const auto& regenerated = regeneratedFaces[index];
        if (original.size() != regenerated.size())
            throw std::runtime_error("Retained source face changed corner count");
        for (size_t corner = 0; corner < original.size(); ++corner) {
            if ((combinedVertices.at(original[corner]) - regeneratedVertices.at(regenerated[corner])).length() > 1e-9)
                throw std::runtime_error("Recombiner source refers to a different face");
        }
    }
    if (!retainedCount)
        throw std::runtime_error("Recombiner fixture exercised no retained faces");
    if (!firstCount || !secondCount)
        throw std::runtime_error("Ownership fixture did not exercise both input meshes");
}

void verifyCachedMeshTriangleOwnership(bool difference, bool recombine)
{
    auto first = box(0, 0, 1);
    auto second = box(0, 0.4, 1.4);
    for (auto& vertex : second.vertices)
        vertex = vertex + dust3d::Vector3(0, 0.2, 0.3);
    dust3d::MeshState firstState(first.vertices, first.triangles);
    dust3d::MeshState secondState(second.vertices, second.triangles);
    const dust3d::Uuid firstOwner("{11111111-1111-4111-8111-111111111111}");
    const dust3d::Uuid secondOwner("{22222222-2222-4222-8222-222222222222}");
    firstState.triangleComponentIds.assign(first.triangles.size(), firstOwner);
    secondState.triangleComponentIds.assign(second.triangles.size(), secondOwner);
    auto combined = dust3d::MeshState::combine(firstState, secondState,
        difference ? dust3d::MeshCombiner::Method::Diff : dust3d::MeshCombiner::Method::Union, recombine);
    if (!combined || combined->isNull())
        throw std::runtime_error("Cached mesh ownership fixture failed");
    dust3d::MeshState copy(*combined);
    std::vector<dust3d::Vector3> vertices;
    std::vector<std::vector<size_t>> faces;
    copy.fetch(vertices, faces);
    if (copy.triangleComponentIds != combined->triangleComponentIds || copy.triangleComponentIds.size() != faces.size())
        throw std::runtime_error("Cached mesh copy lost triangle ownership");
    size_t firstCount = 0;
    size_t secondCount = 0;
    for (size_t index = 0; index < faces.size(); ++index) {
        const auto& owner = copy.triangleComponentIds[index];
        if (owner.isNull())
            continue;
        const auto& input = owner == firstOwner ? first : second;
        if (owner == firstOwner)
            ++firstCount;
        else if (owner == secondOwner)
            ++secondCount;
        else
            throw std::runtime_error("Cached mesh invented an owner");
        bool isSourcePlane = false;
        for (const auto& inputFace : input.triangles) {
            const auto& a = input.vertices[inputFace[0]];
            auto normal = dust3d::Vector3::crossProduct(input.vertices[inputFace[1]] - a, input.vertices[inputFace[2]] - a).normalized();
            bool matches = true;
            for (auto vertex : faces[index])
                matches = matches && std::abs(dust3d::Vector3::dotProduct(vertices[vertex] - a, normal)) < 1e-7;
            isSourcePlane = isSourcePlane || matches;
        }
        if (!isSourcePlane)
            throw std::runtime_error("Cached output face inherited an unrelated owner");
    }
    if (!firstCount || !secondCount)
        throw std::runtime_error("Cached mesh ownership fixture lost an input");
}

}

int main()
{
    try {
        for (size_t axis = 0; axis < 3; ++axis)
            verifyThinSolidAcrossZero(axis);
        verifyBooleanTriangleOwnership(0);
        verifyBooleanTriangleOwnership(1);
        verifyBooleanTriangleOwnership(2);
        for (bool difference : { false, true }) {
            for (bool recombine : { false, true })
                verifyCachedMeshTriangleOwnership(difference, recombine);
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
