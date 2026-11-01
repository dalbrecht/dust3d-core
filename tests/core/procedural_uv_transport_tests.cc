// SPDX-License-Identifier: MIT
#include <cmath>
#include <dust3d/base/object.h>
#include <dust3d/mesh/mesh_generator.h>
#include <dust3d/mesh/mesh_state.h>
#include <iomanip>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
namespace {
const char* ownerA = "{00000000-0000-4000-8000-000000000001}";
const char* ownerB = "{00000000-0000-4000-8000-000000000002}";
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
void near(double actual, double expected)
{
    require(std::abs(actual - expected) < 1e-8, "Corner UV no longer follows its source geometry");
}
std::unique_ptr<dust3d::MeshState> box(double offset, const char* owner)
{
    std::vector<dust3d::Vector3> vertices;
    for (const auto& point : std::vector<std::array<int, 3>> { { 0, 0, 0 }, { 1, 0, 0 }, { 1, 1, 0 }, { 0, 1, 0 }, { 0, 0, 1 }, { 1, 0, 1 }, { 1, 1, 1 }, { 0, 1, 1 } })
        vertices.emplace_back(point[0] + offset, point[1] + (offset ? .2 : 0), point[2] + (offset ? .3 : 0));
    const std::vector<std::vector<size_t>> faces { { 0, 2, 1 }, { 0, 3, 2 }, { 4, 5, 6 }, { 4, 6, 7 }, { 0, 1, 5 }, { 0, 5, 4 }, { 3, 7, 6 }, { 3, 6, 2 }, { 0, 4, 7 }, { 0, 7, 3 }, { 1, 2, 6 }, { 1, 6, 5 } };
    auto result = std::make_unique<dust3d::MeshState>(vertices, faces);
    for (const auto& face : faces) {
        dust3d::ProceduralTriangleUv record;
        record.componentId = dust3d::Uuid(owner);
        for (size_t corner = 0; corner < 3; ++corner)
            record.uv[corner] = dust3d::Vector2(vertices[face[corner]].x() / 2, vertices[face[corner]].y() / 2);
        result->triangleProceduralUvs.emplace_back(record);
    }
    return result;
}
void bridgeOrdinalsAndGradientTransport()
{
    auto first = box(0, ownerA);
    auto second = box(.4, ownerB);
    size_t totalBridges = 0;
    for (const auto method : { dust3d::MeshCombiner::Method::Union, dust3d::MeshCombiner::Method::Diff }) {
        std::vector<std::pair<dust3d::MeshCombiner::Source, size_t>> vertexSources;
        std::unique_ptr<dust3d::MeshCombiner::Mesh> combined(dust3d::MeshCombiner::combine(*first->mesh, *second->mesh, method, &vertexSources));
        require(combined && !combined->isNull(), "Ordinary bridge fixture Boolean failed");
        std::vector<dust3d::Vector3> vertices;
        std::vector<std::vector<size_t>> faces;
        combined->fetch(vertices, faces);
        dust3d::MeshRecombiner recombiner;
        recombiner.setVertices(&vertices, &vertexSources);
        recombiner.setFaces(&faces);
        require(recombiner.recombine(), "Ordinary bridge fixture recombination failed");
        const auto& groups = recombiner.generatedBridgingFaceIndices();
        const auto& oldTriangles = recombiner.generatedBridgingTriangles();
        const auto& boundaries = recombiner.generatedBridgingBoundaryVertices();
        require(groups.size() == oldTriangles.size() && groups.size() == boundaries.size(), "Bridge group metadata lost alignment");
        std::set<size_t> used;
        for (size_t group = 0; group < groups.size(); ++group) {
            for (bool large : { true, false }) {
                const auto& indices = large ? groups[group].first : groups[group].second;
                const auto& positions = large ? oldTriangles[group].first : oldTriangles[group].second;
                const auto& boundary = large ? boundaries[group].first : boundaries[group].second;
                require(indices.size() == positions.size(), "Bridge fan ordinals lost triangle alignment");
                for (const auto vertex : boundary)
                    require(vertex < vertices.size(), "Bridge boundary no longer refers to input vertices");
                for (size_t index = 0; index < indices.size(); ++index) {
                    const auto ordinal = indices[index];
                    require(used.insert(ordinal).second, "Bridge face belongs to two generated charts");
                    require(!recombiner.regeneratedFaceSources().at(ordinal), "Generated bridge invented an original face identity");
                    const auto& face = recombiner.regeneratedFaces().at(ordinal);
                    for (size_t corner = 0; corner < 3; ++corner)
                        near((recombiner.regeneratedVertices().at(face.at(corner)) - positions[index][corner]).length(), 0);
                }
            }
        }
        auto state = dust3d::MeshState::combine(*first, *second, method, true);
        require(state && !state->isNull(), "Procedural bridge fixture failed");
        size_t gradients = 0;
        for (size_t index = 0; index < state->triangleProceduralUvs.size(); ++index) {
            const auto& record = state->triangleProceduralUvs[index];
            if (record && record->gradient) {
                require(used.count(index) != 0, "Generated gradient chart assigned to an unrelated triangle");
                require(record->componentId.isNull() && !record->gradient->chartId.isNull(), "Bridge mapping invented a single component owner");
                ++gradients;
            }
        }
        require(gradients == used.size(), "An exact generated bridge ordinal lost its gradient chart");
        totalBridges += gradients;
    }
    require(totalBridges > 0, "Gradient ordinal regression exercised no generated bridges");
    const dust3d::ProceduralUvGradient gradient { dust3d::Uuid("{00000000-0000-4000-8000-000000000099}"), dust3d::Uuid(ownerA), dust3d::Uuid(ownerB) };
    for (auto& record : first->triangleProceduralUvs) {
        record->componentId = dust3d::Uuid();
        record->gradient = gradient;
    }
    auto preserved = dust3d::MeshState::combine(*first, *second, dust3d::MeshCombiner::Method::Union, false);
    size_t inherited = 0;
    for (const auto& record : preserved->triangleProceduralUvs) {
        if (record && record->gradient) {
            require(record->gradient->chartId == gradient.chartId && record->gradient->largeComponentId == gradient.largeComponentId
                    && record->gradient->smallComponentId == gradient.smallComponentId,
                "Boolean interpolation discarded gradient chart identity/endpoints");
            ++inherited;
        }
    }
    require(inherited > 0, "Gradient interpolation regression exercised no inherited records");
}

void copyAndBoolean()
{
    auto first = box(0, ownerA);
    dust3d::MeshState copied(*first);
    require(copied.triangleProceduralUvs.size() == 12, "Copy lost indexed procedural UVs");
    first->triangleProceduralUvs[0]->uv[0].setX(99);
    require(copied.triangleProceduralUvs[0]->uv[0].x() != 99, "Copy did not own its corner values");
    auto second = box(.4, ownerB);
    for (const auto method : { dust3d::MeshCombiner::Method::Union, dust3d::MeshCombiner::Method::Diff }) {
        auto combined = dust3d::MeshState::combine(copied, *second, method, false);
        require(combined && !combined->isNull(), "Ordinary Boolean failed");
        std::vector<dust3d::Vector3> vertices;
        std::vector<std::vector<size_t>> faces;
        combined->fetch(vertices, faces);
        require(combined->triangleProceduralUvs.size() == faces.size(), "Boolean lost triangle alignment");
        for (size_t index = 0; index < faces.size(); ++index) {
            const auto& record = combined->triangleProceduralUvs[index];
            require(record.has_value(), "Source Boolean surface lost procedural UVs");
            require(record->componentId == dust3d::Uuid(ownerA) || record->componentId == dust3d::Uuid(ownerB), "Boolean lost source chart owner");
            for (size_t corner = 0; corner < 3; ++corner) {
                near(record->uv[corner].x(), vertices[faces[index][corner]].x() / 2);
                near(record->uv[corner].y(), vertices[faces[index][corner]].y() / 2);
            }
        }
    }
}
std::unique_ptr<dust3d::Object> tube(bool mirrored, dust3d::MeshGenerator::GeneratedCacheContext* cache = nullptr)
{
    const std::string part = "{00000000-0000-4000-8000-000000000010}";
    const std::string a = "{00000000-0000-4000-8000-000000000011}";
    const std::string b = "{00000000-0000-4000-8000-000000000012}";
    auto snapshot = std::make_unique<dust3d::Snapshot>();
    snapshot->canvas = { { "originX", ".5" }, { "originY", ".5" }, { "originZ", ".5" } };
    snapshot->parts[part] = { { "id", part }, { "interpolated", "false" }, { "rounded", "false" }, { "xMirrored", mirrored ? "true" : "false" } };
    snapshot->nodes[a] = { { "id", a }, { "partId", part }, { "x", "0.3" }, { "y", "0" }, { "z", "0" }, { "radius", "0.2" } };
    snapshot->nodes[b] = { { "id", b }, { "partId", part }, { "x", "0.3" }, { "y", "1" }, { "z", "0" }, { "radius", "0.3" } };
    snapshot->edges["{00000000-0000-4000-8000-000000000013}"] = { { "partId", part }, { "from", a }, { "to", b } };
    snapshot->components[ownerA] = { { "id", ownerA }, { "linkDataType", "partId" }, { "linkData", part }, { "combineMode", "Uncombined" } };
    snapshot->rootComponent["children"] = ownerA;
    dust3d::MeshGenerator generator(snapshot.release());
    if (cache)
        generator.setGeneratedCacheContext(cache);
    generator.generate();
    require(generator.isSuccessful(), "Tube fixture generation failed");
    auto object = std::unique_ptr<dust3d::Object>(generator.takeObject());
    require(object->triangleComponentKeys.size() == object->triangles.size(), "Procedural source keys must align without imported inputs");
    for (size_t index = 0; index < object->triangles.size(); ++index) {
        require(object->triangleProceduralUvs[index].has_value(), "Tube mapping missing");
        require(dust3d::Uuid(object->triangleComponentKeys[index]) == object->triangleProceduralUvs[index]->componentId, "Source key and chart owner disagree");
        require(object->triangleComponentKeys[index].front() == '{', "Original braced source key was normalized away");
    }
    for (const auto& entry : generator.generatedComponentPreviews()) {
        const auto& preview = entry.second;
        require(preview.triangleProceduralUvs.size() == preview.triangles.size(), "Part/group preview lost procedural channel alignment");
        for (const auto& record : preview.triangleProceduralUvs)
            require(record.has_value(), "Tube preview lost procedural corner ownership");
    }
    return object;
}
void unknownBooleanSources()
{
    auto first = box(0, ownerA);
    auto second = box(.4, ownerB);
    second->triangleProceduralUvs.clear();
    auto result = dust3d::MeshState::combine(*first, *second, dust3d::MeshCombiner::Method::Union, false);
    require(result && !result->isNull(), "Unknown-source Boolean failed");
    std::vector<dust3d::Vector3> vertices;
    std::vector<std::vector<size_t>> triangles;
    result->fetch(vertices, triangles);
    require(result->triangleProceduralUvs.size() == triangles.size(), "Unknown-source channel lost alignment");
    size_t known = 0, unknown = 0;
    for (const auto& record : result->triangleProceduralUvs) {
        if (record) {
            require(record->componentId == dust3d::Uuid(ownerA), "Unknown source acquired another chart owner");
            ++known;
        } else
            ++unknown;
    }
    require(known && unknown, "Missing source must remain explicitly unknown");
}

std::string identity(unsigned value)
{
    std::ostringstream text;
    text << '{' << std::setfill('0') << std::setw(8) << value << "-0000-4000-8000-" << std::setw(12) << value << '}';
    return text.str();
}
void stitchingOwnership()
{
    auto snapshot = std::make_unique<dust3d::Snapshot>();
    snapshot->canvas = { { "originX", ".5" }, { "originY", ".5" }, { "originZ", ".5" } };
    const auto group = identity(300);
    std::string children;
    for (unsigned partIndex = 0; partIndex < 2; ++partIndex) {
        const auto part = identity(100 + partIndex);
        const auto component = identity(200 + partIndex);
        snapshot->parts[part] = { { "id", part }, { "target", "StitchingLoop" }, { "fillLoopInterior", "true" } };
        snapshot->components[component] = { { "id", component }, { "linkDataType", "partId" }, { "linkData", part } };
        children += (children.empty() ? "" : ",") + component;
        for (unsigned corner = 0; corner < 4; ++corner) {
            const double angle = std::acos(-1.0) * corner / 2;
            const double radius = partIndex ? .16 : .4;
            const auto node = identity(1000 + partIndex * 10 + corner);
            snapshot->nodes[node] = { { "id", node }, { "partId", part }, { "x", std::to_string(.5 + std::cos(angle) * radius) }, { "y", std::to_string(.5 + std::sin(angle) * radius) }, { "z", partIndex ? ".6" : ".5" }, { "radius", ".05" } };
            const auto edge = identity(2000 + partIndex * 10 + corner);
            snapshot->edges[edge] = { { "id", edge }, { "partId", part }, { "from", node }, { "to", identity(1000 + partIndex * 10 + (corner + 1) % 4) } };
        }
    }
    snapshot->components[group] = { { "id", group }, { "children", children }, { "color", "#3366cc" } };
    snapshot->rootComponent["children"] = group;
    dust3d::MeshGenerator generator(snapshot.release());
    generator.generate();
    require(generator.isSuccessful(), "Ordinary stitching loop generation failed");
    const auto object = std::unique_ptr<dust3d::Object>(generator.takeObject());
    require(object && !object->triangles.empty(), "Expected stitching geometry");
    require(object->triangleProceduralUvs.size() == object->triangles.size(), "Stitch UVs lost triangle alignment");
    require(object->triangleComponentKeys.size() == object->triangles.size(), "Stitch source keys lost alignment");
    for (size_t index = 0; index < object->triangles.size(); ++index) {
        const auto& record = object->triangleProceduralUvs[index];
        require(record.has_value(), "Stitch face lost its indexed chart");
        require(record->componentId == dust3d::Uuid(identity(200)) || record->componentId == dust3d::Uuid(identity(201)), "Stitch chart has no original loop owner");
        require(object->triangleComponentKeys[index] == record->componentId.toString(), "Stitch source key and chart owner disagree");
    }
}

void generationAndMirror()
{
    dust3d::MeshGenerator::GeneratedCacheContext cache;
    const auto original = tube(false, &cache);
    require(original && !original->triangles.empty(), "Expected tube geometry");
    require(original->triangleProceduralUvs.size() == original->triangles.size(), "Tube lost indexed UVs");
    for (const auto& uv : original->triangleProceduralUvs)
        require(uv && uv->componentId == dust3d::Uuid(ownerA), "Tube lost its chart owner");
    const auto mirror = tube(true);
    require(mirror->triangles.size() == original->triangles.size() * 2, "Expected uncombined mirrored geometry");
    require(mirror->triangleProceduralUvs.size() == mirror->triangles.size(), "Mirror lost UV alignment");
    size_t mirroredCorners = 0;
    for (size_t index = 0; index < mirror->triangles.size(); ++index) {
        const auto& uv = mirror->triangleProceduralUvs[index];
        require(uv.has_value(), "Mirror lost source UVs");
        if (uv->componentId == dust3d::Uuid(ownerA))
            continue;
        for (size_t corner = 0; corner < 3; ++corner) {
            const auto& position = mirror->vertices[mirror->triangles[index][corner]];
            bool matches = false;
            for (size_t face = 0; face < original->triangles.size(); ++face) {
                for (size_t sourceCorner = 0; sourceCorner < 3; ++sourceCorner) {
                    const auto& source = original->vertices[original->triangles[face][sourceCorner]];
                    const auto& expected = original->triangleProceduralUvs[face]->uv[sourceCorner];
                    matches |= std::abs(position.x() + source.x()) < 1e-8 && std::abs(position.y() - source.y()) < 1e-8 && std::abs(position.z() - source.z()) < 1e-8
                        && std::abs(uv->uv[corner].x() - expected.x()) < 1e-8 && std::abs(uv->uv[corner].y() - expected.y()) < 1e-8;
                }
            }
            require(matches, "Mirror UV does not follow the reversed source corner");
            ++mirroredCorners;
        }
    }
    require(mirroredCorners > 0, "Mirror regression was not exercised");
    const auto warm = tube(false, &cache);
    require(warm->triangleProceduralUvs.size() == original->triangleProceduralUvs.size(), "Warm generation lost UV channel");
    for (size_t index = 0; index < original->triangles.size(); ++index) {
        require(warm->triangleProceduralUvs[index].has_value(), "Warm generation lost source mapping");
        require(warm->triangleProceduralUvs[index]->componentId == original->triangleProceduralUvs[index]->componentId, "Warm generation changed chart owner");
        for (size_t corner = 0; corner < 3; ++corner) {
            near(warm->triangleProceduralUvs[index]->uv[corner].x(), original->triangleProceduralUvs[index]->uv[corner].x());
            near(warm->triangleProceduralUvs[index]->uv[corner].y(), original->triangleProceduralUvs[index]->uv[corner].y());
        }
    }
}
}
int main()
{
    try {
        bridgeOrdinalsAndGradientTransport();
        copyAndBoolean();
        unknownBooleanSources();
        generationAndMirror();
        stitchingOwnership();
        std::cout << "Procedural UV transport contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
