// SPDX-License-Identifier: MIT
#include <cmath>
#include <dust3d/base/object.h>
#include <dust3d/mesh/mesh_generator.h>
#include <dust3d/rig/rig_generator.h>
#include <dust3d/uv/texture_baker.h>
#include <dust3d/uv/uv_atlas_builder.h>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <native/Engine/ObjDocument.h>
#include <set>
#include <sstream>
#include <stdexcept>

namespace {
const std::string partA = "{00000000-0000-4000-8000-000000000010}";
const std::string nodeA = "{00000000-0000-4000-8000-000000000011}";
const std::string nodeB = "{00000000-0000-4000-8000-000000000012}";
const std::string ownerA = "{00000000-0000-4000-8000-000000000001}";
const std::string partB = "{00000000-0000-4000-8000-000000000020}";
const std::string nodeC = "{00000000-0000-4000-8000-000000000021}";
const std::string nodeD = "{00000000-0000-4000-8000-000000000022}";
const std::string ownerB = "{00000000-0000-4000-8000-000000000002}";
void require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}
void addPart(dust3d::Snapshot& snapshot, const std::string& part, const std::string& a,
    const std::string& b, const std::string& component)
{
    snapshot.parts[part] = { { "id", part }, { "interpolated", "false" }, { "rounded", "false" } };
    snapshot.nodes[a] = { { "id", a }, { "partId", part }, { "x", ".3" }, { "y", "0" }, { "z", "0" }, { "radius", ".2" } };
    snapshot.nodes[b] = { { "id", b }, { "partId", part }, { "x", ".3" }, { "y", "1" }, { "z", "0" }, { "radius", ".3" } };
    snapshot.edges[part] = { { "partId", part }, { "from", a }, { "to", b } };
    snapshot.components[component] = { { "id", component }, { "linkDataType", "partId" }, { "linkData", part }, { "combineMode", "Uncombined" } };
}
std::unique_ptr<dust3d::Snapshot> snapshot(bool coincident, bool mirrored)
{
    auto result = std::make_unique<dust3d::Snapshot>();
    result->canvas = { { "originX", ".5" }, { "originY", ".5" }, { "originZ", ".5" } };
    addPart(*result, partA, nodeA, nodeB, ownerA);
    result->rootComponent["children"] = ownerA;
    result->parts[partA]["xMirrored"] = mirrored ? "true" : "false";
    if (coincident) {
        addPart(*result, partB, nodeC, nodeD, ownerB);
        result->rootComponent["children"] += ',' + ownerB;
    }
    return result;
}
std::unique_ptr<dust3d::Object> generate(bool coincident, bool mirrored,
    dust3d::MeshGenerator::GeneratedCacheContext* cache = nullptr)
{
    auto source = snapshot(coincident, mirrored);
    dust3d::MeshGenerator generator(source.release());
    if (cache)
        generator.setGeneratedCacheContext(cache);
    generator.generate();
    require(generator.isSuccessful(), "Synthetic tube generation failed");
    return std::unique_ptr<dust3d::Object>(generator.takeObject());
}
void explicitZeroHollowMatchesDefault()
{
    const auto expected = generate(false, false);
    auto source = snapshot(false, false);
    source->parts[partA]["hollowThickness"] = "0";
    dust3d::MeshGenerator generator(source.release());
    generator.generate();
    require(generator.isSuccessful(), "Explicit zero hollow thickness must produce a solid tube");
    const std::unique_ptr<dust3d::Object> actual(generator.takeObject());
    require(actual && expected->vertices.size() == actual->vertices.size() && expected->triangleAndQuads == actual->triangleAndQuads,
        "Explicit zero hollow thickness must preserve the default topology");
    for (size_t vertex = 0; vertex < actual->vertices.size(); ++vertex)
        for (size_t axis = 0; axis < 3; ++axis)
            require(actual->vertices[vertex][axis] == expected->vertices[vertex][axis],
                "Explicit zero hollow thickness must preserve vertex positions exactly");
}
void intersectingHollowBooleanRetainsIndexedRecords(const char* mode)
{
    auto source = snapshot(true, false);
    source->parts[partA]["hollowThickness"] = ".5";
    source->components[ownerA]["combineMode"] = "Normal";
    source->components[ownerB]["combineMode"] = mode;
    source->nodes[nodeC]["x"] = ".49";
    source->nodes[nodeD]["x"] = ".49";
    source->nodes[nodeC]["y"] = ".13";
    source->nodes[nodeD]["y"] = "1.13";
    source->nodes[nodeC]["z"] = ".07";
    source->nodes[nodeD]["z"] = ".07";
    dust3d::MeshGenerator generator(source.release());
    generator.generate();
    require(generator.isSuccessful(), "Hollow and solid tube Boolean failed");
    const std::unique_ptr<dust3d::Object> object(generator.takeObject());
    require(object && !object->triangles.empty(), "Hollow Boolean must produce geometry");
    require(object->triangles.size() != 32, "Boolean output must differ from the uncut hollow tube");
    require(object->triangleSourceCorners.size() == object->triangles.size()
            && object->triangleProceduralUvs.size() == object->triangles.size(),
        "Hollow Boolean must retain indexed record alignment");
    std::set<dust3d::Uuid> nodes;
    for (const auto& triangle : object->triangleSourceCorners)
        for (size_t corner = 0; corner < 3; ++corner)
            for (const auto& influence : triangle[corner].influences())
                nodes.insert(influence.first);
    require(nodes.count(dust3d::Uuid(nodeA)) && nodes.count(dust3d::Uuid(nodeB)),
        "Boolean output must retain both hollow tube source nodes");
    require(nodes.count(dust3d::Uuid(nodeC)) || nodes.count(dust3d::Uuid(nodeD)),
        "Boolean output must retain provenance from the intersecting tube");
}
void mirroredHollowRetainsIndexedCorners()
{
    auto source = snapshot(false, true);
    source->parts[partA]["hollowThickness"] = ".5";
    dust3d::MeshGenerator generator(source.release());
    generator.generate();
    require(generator.isSuccessful(), "Mirrored hollow tube generation failed");
    const std::unique_ptr<dust3d::Object> object(generator.takeObject());
    require(object && object->triangles.size() == 64, "Mirroring must retain both shells and rims");
    require(object->triangleSourceCorners.size() == 64 && object->triangleProceduralUvs.size() == 64,
        "Mirroring must retain triangle-aligned hollow records");
    for (size_t face = 0; face < object->triangles.size(); ++face) {
        require(object->triangleProceduralUvs[face].has_value(), "Mirrored hollow face must retain UVs");
        for (size_t corner = 0; corner < 3; ++corner)
            require(object->triangleSourceCorners[face][corner].dominantNodeHint().has_value(),
                "Mirrored hollow corner must retain source ownership");
    }
}
void nearCollapseRetainsIndexedTriangles()
{
    auto source = snapshot(false, false);
    source->parts[partA]["hollowThickness"] = "0.99999999";
    dust3d::MeshGenerator generator(source.release());
    generator.generate();
    require(generator.isSuccessful(), "Near-collapse tube generation failed");
    const std::unique_ptr<dust3d::Object> object(generator.takeObject());
    require(object && object->triangles.size() == 32, "Near-collapse tube must retain both walls and rims");
    require(object->triangleSourceCorners.size() == 32 && object->triangleProceduralUvs.size() == 32,
        "Near-collapse tube must retain indexed corner records");
    const dust3d::Uuid owner(ownerA);
    const auto atlas = dust3d::UvAtlasBuilder::build(*object, { { owner, { dust3d::Color(1, 0, 0), std::nullopt } } });
    require(atlas.triangleUvKnown == std::vector<uint8_t>(32, 1), "Near-collapse atlas must map every triangle");
    dust3d::TextureBakeOptions options;
    options.colorSize = options.normalSize = 32;
    options.materialSize = 8;
    const auto bake = dust3d::TextureBaker::bake(*object, atlas, {}, options);
    require(bake.color.pixels.size() == 32 * 32 * 4 && bake.tangents.size() == 32,
        "Near-collapse bake must retain its color map and triangle-aligned tangent records");
    dust3d::RigGenerator rig;
    require(rig.computeVertexBoneBindings(object.get(),
                { { dust3d::Uuid(nodeA), { "First" } }, { dust3d::Uuid(nodeB), { "Second" } } }),
        "Near-collapse tube must bind to bones");
    for (size_t face = 0; face < object->triangles.size(); ++face) {
        const auto& uv = object->triangleProceduralUvs[face];
        require(uv && uv->componentId == dust3d::Uuid(ownerA), "Near-collapse UV must retain its owner");
        for (size_t corner = 0; corner < 3; ++corner) {
            const auto node = object->triangleSourceCorners[face][corner].dominantNodeHint();
            require(node == dust3d::Uuid(nodeA) || node == dust3d::Uuid(nodeB),
                "Near-collapse corner must retain its node");
            const auto& position = object->vertices[object->triangles[face][corner]];
            const auto legacyNode = object->positionToNodeIdMap.find(dust3d::PositionKey(position));
            require(legacyNode != object->positionToNodeIdMap.end() && node == legacyNode->second,
                "Legacy rig lookup must agree with the indexed source for this tube");
            const auto& binding = object->vertexBone1.at(object->triangles[face][corner]);
            require(binding.first == (node == dust3d::Uuid(nodeA) ? "First" : "Second") && binding.second == 1.0f,
                "Hollow corner must bind to the bone of its indexed source node");
            require(std::isfinite(uv->uv[corner].x()) && std::isfinite(uv->uv[corner].y()),
                "Near-collapse UV must remain finite");
        }
    }
}
void hollowTubeRetainsIndexedSources()
{
    const auto solid = generate(false, false);
    auto source = snapshot(false, false);
    source->parts[partA]["hollowThickness"] = ".5";
    dust3d::MeshGenerator generator(source.release());
    generator.generate();
    require(generator.isSuccessful(), "Synthetic hollow tube generation failed");
    const std::unique_ptr<dust3d::Object> hollow(generator.takeObject());
    require(hollow && hollow->triangles.size() > solid->triangles.size(), "Snapshot hollow thickness must add inner and rim faces");
    require(hollow->triangleSourceCorners.size() == hollow->triangles.size(), "Hollow source corners must align with triangles");
    require(hollow->triangleProceduralUvs.size() == hollow->triangles.size(), "Hollow UV records must align with triangles");
    for (const auto& record : hollow->triangleProceduralUvs) {
        require(record && record->componentId == dust3d::Uuid(ownerA), "Hollow UV record must retain its component owner");
        for (const auto& uv : record->uv)
            require(std::isfinite(uv.x()) && std::isfinite(uv.y()), "Hollow UV corners must be finite");
        const auto first = record->uv[1] - record->uv[0];
        const auto second = record->uv[2] - record->uv[0];
        require(std::abs(first.x() * second.y() - first.y() * second.x()) > 1e-12, "Hollow UV triangle must have area");
    }
    const dust3d::Uuid owner(ownerA);
    const auto atlas = dust3d::UvAtlasBuilder::build(*hollow, { { owner, { dust3d::Color(1, 0, 0), std::nullopt } } });
    require(atlas.layouts.size() == 1, "Hollow tube must pack into its component chart");
    const auto& layout = atlas.layouts.front();
    require(atlas.triangleUvs.size() == hollow->triangles.size(), "Packed hollow UVs must align with triangles");
    for (size_t triangle = 0; triangle < hollow->triangles.size(); ++triangle) {
        require(atlas.triangleUvKnown.at(triangle), "Every hollow face must have a packed UV");
        const auto& input = hollow->triangleProceduralUvs[triangle]->uv;
        for (size_t corner = 0; corner < 3; ++corner) {
            const auto localV = input[corner].y();
            const auto packed = atlas.triangleUvs[triangle][corner];
            const auto coordinate = layout.flipped ? packed.x() : packed.y();
            const auto origin = layout.flipped ? layout.left : layout.top;
            require(std::abs(coordinate - (origin + localV * layout.height)) < 1e-5,
                "Atlas must preserve each hollow corner's band coordinate");
        }
    }
    dust3d::TextureBakeOptions options;
    options.colorSize = options.normalSize = 32;
    options.materialSize = 8;
    const auto plain = dust3d::TextureBaker::bake(*hollow, atlas, {}, options);
    dust3d::TextureBakeInputs inputs;
    dust3d::SurfacePattern::Settings pattern;
    pattern.type = dust3d::SurfacePattern::Type::Spots;
    pattern.baseColor = dust3d::Color(1, 1, 1);
    pattern.patternColor = dust3d::Color(0, 0, 0);
    pattern.scale = .1;
    inputs.surfaces[owner].pattern = pattern;
    const auto patterned = dust3d::TextureBaker::bake(*hollow, atlas, inputs, options);
    require(patterned.rasterTriangleVisits > 0 && patterned.color.pixels != plain.color.pixels,
        "Hollow indexed geometry must rasterize a procedural surface pattern");
    require(patterned.tangents.size() == hollow->triangles.size(),
        "Hollow baked tangents must align with triangles");
    const auto obj = dust3d::native::encodeObjDocument(*hollow);
    std::istringstream records(std::string(obj.bytes().begin(), obj.bytes().end()));
    size_t vertex = 0;
    size_t face = 0;
    for (std::string line; std::getline(records, line);) {
        std::istringstream record(line);
        std::string kind;
        record >> kind;
        if (kind == "v") {
            double x, y, z;
            require(bool(record >> x >> y >> z), "Hollow OBJ vertex must contain three coordinates");
            const auto& expected = hollow->vertices.at(vertex++);
            require(x == expected.x() && y == expected.y() && z == expected.z(), "Hollow OBJ must preserve positions");
        } else if (kind == "f") {
            std::vector<size_t> corners;
            for (size_t index; record >> index;) {
                require(index > 0 && index <= vertex, "Hollow OBJ index must reference an emitted vertex");
                corners.push_back(index - 1);
            }
            require(corners == hollow->triangleAndQuads.at(face++), "Hollow OBJ must preserve polygon winding");
        }
    }
    require(vertex == hollow->vertices.size() && face == hollow->triangleAndQuads.size() && face > 0,
        "Hollow OBJ must preserve all original geometry");
    for (const auto& triangle : hollow->triangleSourceCorners)
        for (size_t corner = 0; corner < 3; ++corner) {
            const auto source = triangle[corner].dominantNodeHint();
            require(source == dust3d::Uuid(nodeA) || source == dust3d::Uuid(nodeB), "Hollow corner must retain a source node");
        }
}
void duplicateDominantHints()
{
    dust3d::SourceCorner split, merged;
    split.weighted = merged.weighted = true;
    split.setInfluences({ { dust3d::Uuid(nodeA), .4f }, { dust3d::Uuid(nodeA), .4f }, { dust3d::Uuid(nodeB), .7f } });
    merged.setInfluences({ { dust3d::Uuid(nodeA), .8f }, { dust3d::Uuid(nodeB), .7f } });
    require(split.dominantNodeHint() == std::optional<dust3d::Uuid>(dust3d::Uuid(nodeA))
            && split.dominantNodeHint() == merged.dominantNodeHint(),
        "Equivalent duplicate-weight distributions must choose the same aggregate dominant bind node");
    split.setInfluences({ { dust3d::Uuid(nodeB), .5f }, { dust3d::Uuid(nodeA), .5f } });
    require(split.dominantNodeHint() == std::optional<dust3d::Uuid>(dust3d::Uuid(nodeB)),
        "Exact aggregate ties must retain the original first-node policy");
}
void coincidentPartsAndCache()
{
    dust3d::MeshGenerator::GeneratedCacheContext cache;
    const auto object = generate(true, false, &cache);
    require(object && !object->triangles.empty(), "Coincident tube fixture produced no geometry");
    require(object->triangleSourceCorners.size() == object->triangles.size(), "Generated source corners are not triangle aligned");
    dust3d::RetainedPayloadBudget footprint(std::numeric_limits<size_t>::max());
    footprint.accountDynamic(object->triangleSourceCorners);
    std::cout << "Coincident tube source corners: " << object->triangles.size() * 3
              << ", retained logical/capacity bytes: " << footprint.usedBytes() << '\n';
    size_t observedA = 0, observedB = 0;
    for (size_t triangle = 0; triangle < object->triangles.size(); ++triangle) {
        const bool second = object->triangleComponentKeys.at(triangle) == ownerB;
        second ? ++observedB : ++observedA;
        for (size_t corner = 0; corner < 3; ++corner) {
            const auto& value = object->triangleSourceCorners[triangle][corner];
            require(!value.weighted && value.influences().size() == 1 && value.influences()[0].second == 1,
                "Tube corner needs its exact original node hint");
            const auto& position = object->vertices[object->triangles[triangle][corner]];
            const dust3d::Uuid expected(position.y() > 0 ? (second ? nodeC : nodeA) : (second ? nodeD : nodeB));
            require(value.influences()[0].first == expected,
                "Coincident components borrowed a positional source from another part");
        }
    }
    require(observedA && observedB, "Both distinct coincident source components must be exercised");
    const auto warm = generate(true, false, &cache);
    require(warm->triangleSourceCorners.size() == object->triangleSourceCorners.size(), "Warm cache lost source ownership");
    for (size_t triangle = 0; triangle < object->triangles.size(); ++triangle)
        for (size_t corner = 0; corner < 3; ++corner)
            require(warm->triangleSourceCorners[triangle][corner].influences() == object->triangleSourceCorners[triangle][corner].influences(),
                "Warm cache changed actual corner source ownership");
}
void mirroredCornerOrder()
{
    const auto object = generate(false, true);
    require(object->triangleSourceCorners.size() == object->triangles.size(), "Mirrored source corners are not aligned");
    size_t mirrored = 0;
    for (size_t triangle = 0; triangle < object->triangles.size(); ++triangle) {
        const bool isMirror = object->triangleComponentKeys.at(triangle) != ownerA;
        for (size_t corner = 0; corner < 3; ++corner) {
            const auto& value = object->triangleSourceCorners[triangle][corner];
            require(value.influences().size() == 1, "Mirrored tube lost a corner source");
            const auto& position = object->vertices[object->triangles[triangle][corner]];
            const auto node = object->nodeMap.find(value.influences()[0].first);
            require(node != object->nodeMap.end() && std::abs(node->second.origin.y() - position.y()) < 1e-8,
                "Mirrored source hint no longer follows its actual reversed corner");
            if (isMirror) {
                require(value.influences()[0].first != dust3d::Uuid(nodeA) && value.influences()[0].first != dust3d::Uuid(nodeB),
                    "Generated mirror node identity was silently replaced with original identity");
                ++mirrored;
            }
        }
    }
    require(mirrored, "Mirror source regression was not exercised");
}

std::string identity(unsigned value)
{
    std::ostringstream text;
    text << '{' << std::setfill('0') << std::setw(8) << value << "-0000-4000-8000-" << std::setw(12) << value << '}';
    return text.str();
}
void stitchingLoopSources()
{
    auto source = std::make_unique<dust3d::Snapshot>();
    source->canvas = { { "originX", ".5" }, { "originY", ".5" }, { "originZ", ".5" } };
    std::set<dust3d::Uuid> originalNodes;
    std::string children;
    for (unsigned partIndex = 0; partIndex < 2; ++partIndex) {
        const auto part = identity(100 + partIndex);
        const auto component = identity(200 + partIndex);
        source->parts[part] = { { "id", part }, { "target", "StitchingLoop" }, { "fillLoopInterior", "true" } };
        source->components[component] = { { "id", component }, { "linkDataType", "partId" }, { "linkData", part } };
        children += (children.empty() ? "" : ",") + component;
        for (unsigned corner = 0; corner < 4; ++corner) {
            const double angle = std::acos(-1.0) * corner / 2;
            const double radius = partIndex ? .16 : .4;
            const auto node = identity(1000 + partIndex * 10 + corner);
            originalNodes.insert(dust3d::Uuid(node));
            source->nodes[node] = { { "id", node }, { "partId", part }, { "x", std::to_string(.5 + std::cos(angle) * radius) },
                { "y", std::to_string(.5 + std::sin(angle) * radius) }, { "z", partIndex ? ".6" : ".5" }, { "radius", ".05" } };
            const auto edge = identity(2000 + partIndex * 10 + corner);
            source->edges[edge] = { { "id", edge }, { "partId", part }, { "from", node }, { "to", identity(1000 + partIndex * 10 + (corner + 1) % 4) } };
        }
    }
    const auto group = identity(300);
    source->components[group] = { { "id", group }, { "children", children } };
    source->rootComponent["children"] = group;
    dust3d::MeshGenerator generator(source.release());
    generator.generate();
    require(generator.isSuccessful(), "Stitching source fixture failed");
    std::unique_ptr<dust3d::Object> object(generator.takeObject());
    require(object && !object->triangles.empty() && object->triangleSourceCorners.size() == object->triangles.size(),
        "Stitching source corners lost actual triangle ordinals");
    std::set<dust3d::Uuid> observed;
    for (const auto& triangle : object->triangleSourceCorners)
        for (const auto& corner : triangle.corners)
            if (!corner.influences().empty()) {
                require(!corner.weighted && corner.influences().size() == 1 && corner.influences()[0].second == 1,
                    "Stitching source hints changed their direct-node policy");
                require(originalNodes.count(corner.influences()[0].first), "Stitching corner gained an unrelated source identity");
                observed.insert(corner.influences()[0].first);
            }
    require(observed.size() > 1, "Stitching regression must exercise multiple actual source nodes");
}
void wrapSourcesStayWeighted()
{
    auto source = snapshot(false, false);
    const auto group = identity(400);
    source->components[ownerA]["combineMode"] = "Normal";
    source->components[group] = { { "id", group }, { "children", ownerA }, { "wrap", "Skin" }, { "wrapFaces", "64" }, { "color", "#ffffff" } };
    source->rootComponent["children"] = group;
    dust3d::MeshGenerator generator(source.release());
    generator.generate();
    require(generator.isSuccessful(), "Bounded wrap source fixture failed");
    std::unique_ptr<dust3d::Object> object(generator.takeObject());
    require(object && !object->triangles.empty() && object->triangleSourceCorners.size() == object->triangles.size(),
        "Wrap source weights lost actual triangle ordinals");
    size_t known = 0;
    for (const auto& triangle : object->triangleSourceCorners)
        for (const auto& corner : triangle.corners) {
            if (corner.influences().empty())
                continue;
            require(corner.weighted, "Wrap source weights lost weighted-hint semantics");
            double sum = 0;
            for (const auto& influence : corner.influences()) {
                require((influence.first == dust3d::Uuid(nodeA) || influence.first == dust3d::Uuid(nodeB))
                        && std::isfinite(influence.second) && influence.second >= 0,
                    "Wrap corner source has an invalid or unrelated weighted node");
                sum += influence.second;
            }
            require(std::abs(sum - 1) < 1e-5, "Wrap builder's normalized source hints changed during ordinal transport");
            ++known;
        }
    require(known > 0, "Wrap weighted-source regression was not exercised");
}

void importedNearestHintStaysVertexIndexed()
{
    auto source = snapshot(false, false);
    source->parts[partA]["target"] = "ImportedModel";
    source->parts[partA]["importedModelId"] = "synthetic";
    dust3d::MeshGenerator::ImportedModelData model;
    model.vertices = { { -1, 0, -1 }, { 1, 0, -1 }, { -1, 1, 1 }, { -1, 0, -1 } };
    model.faces = { { 0, 1, 2 }, { 3, 2, 1 } };
    dust3d::MeshGenerator generator(source.release());
    generator.setImportedModelData({ { "synthetic", model } });
    generator.generate();
    require(generator.isSuccessful(), "Imported synthetic fixture failed");
    std::unique_ptr<dust3d::Object> object(generator.takeObject());
    require(object && object->triangleSourceCorners.size() == 2, "Imported source corners must retain actual face ordinals");
    for (size_t triangle = 0; triangle < object->triangles.size(); ++triangle)
        for (size_t corner = 0; corner < 3; ++corner) {
            const auto& value = object->triangleSourceCorners[triangle][corner];
            require(value.influences().size() == 1 && !value.weighted, "Imported vertex lost nearest-node hint policy");
            const auto& position = object->vertices[object->triangles[triangle][corner]];
            const auto a = object->nodeMap.at(dust3d::Uuid(nodeA)).origin;
            const auto b = object->nodeMap.at(dust3d::Uuid(nodeB)).origin;
            const auto expected = (position - a).lengthSquared() <= (position - b).lengthSquared() ? dust3d::Uuid(nodeA) : dust3d::Uuid(nodeB);
            require(value.influences()[0].first == expected, "Imported source hint changed the established nearest-node decision");
        }
}
}
int main()
{
    try {
        intersectingHollowBooleanRetainsIndexedRecords("Normal");
        intersectingHollowBooleanRetainsIndexedRecords("Inversion");
        mirroredHollowRetainsIndexedCorners();
        nearCollapseRetainsIndexedTriangles();
        explicitZeroHollowMatchesDefault();
        hollowTubeRetainsIndexedSources();
        duplicateDominantHints();
        coincidentPartsAndCache();
        mirroredCornerOrder();
        importedNearestHintStaysVertexIndexed();
        stitchingLoopSources();
        wrapSourcesStayWeighted();
        std::cout << "Indexed source producer contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
