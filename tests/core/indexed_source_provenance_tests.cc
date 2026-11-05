// SPDX-License-Identifier: MIT
#include "GenerationPipeline.h"
#include "SourceProvenance.h"
#include <cmath>
#include <dust3d/base/object.h>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace dust3d;
using namespace dust3d::native;
const std::string first = "{aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa}";
const std::string second = "{bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb}";
const std::string absent = "{cccccccc-cccc-4ccc-8ccc-cccccccccccc}";
void require(bool value, const char* cause)
{
    if (!value)
        throw std::runtime_error(cause);
}
Snapshot membership()
{
    Snapshot snapshot;
    snapshot.nodes[first]["partId"] = "first-original-part";
    snapshot.nodes[second]["partId"] = "second-original-part";
    return snapshot;
}
Object coincident()
{
    Object object;
    object.vertices = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 },
        { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } };
    object.triangles = { { 0, 1, 2 }, { 3, 4, 5 } };
    object.triangleSourceCorners.resize(2);
    object.triangleSourceCorners[0][0].setInfluences({ { Uuid(first), 1 } });
    object.triangleSourceCorners[1][0].setInfluences({ { Uuid(second), 1 } });
    object.triangleSourceCorners[0][1].weighted = true;
    object.triangleSourceCorners[0][1].setInfluences({ { Uuid(first), .25f }, { Uuid(second), .75f } });
    object.positionToNodeIdMap[object.vertices[0]] = Uuid(absent);
    return object;
}
std::string nodeAt(const SourceProvenance& source, size_t index)
{
    const auto& corner = source.corners().at(index);
    require(corner.influence_count == 1, "Expected one indexed source node");
    const auto& influence = source.influences().at(corner.first_influence);
    require(influence.kind == DUST3D_SOURCE_NODE_HINT && influence.weight == 1,
        "Indexed unit hint lost original identity or weight");
    return source.identities().at(influence.source_index).node;
}
template <typename Action>
void rejects(Action action, Dust3DStatus status)
{
    try {
        action();
    } catch (const DocumentError& error) {
        require(error.status() == status && *error.what(), "Indexed admission must retain typed cause");
        return;
    }
    throw std::runtime_error("Invalid indexed source channel unexpectedly succeeded");
}
void coincidentCornersAndOwnership()
{
    auto snapshot = membership();
    auto object = coincident();
    SourceProvenance source(snapshot, 1024 * 1024);
    source.appendCorner(object, object.vertices[0], 0, 0);
    source.appendCorner(object, object.vertices[3], 1, 0);
    source.appendCorner(object, object.vertices[1], 0, 1);
    source.appendCorner(object, object.vertices[4], 1, 1);
    require(nodeAt(source, 0) == first && nodeAt(source, 1) == second,
        "Coincident corners must retain distinct triangle-indexed source ownership");
    const auto& weighted = source.corners()[2];
    require(weighted.kind == DUST3D_SOURCE_WEIGHTED_HINT && weighted.influence_count == 2,
        "Indexed weighted hints must retain all original associations");
    require(source.influences()[weighted.first_influence].weight == .25f
            && source.influences()[weighted.first_influence + 1].weight == .75f,
        "Indexed source weights must not be normalized or reordered");
    require(source.corners()[3].kind == DUST3D_SOURCE_UNKNOWN && source.corners()[3].influence_count == 0,
        "Explicit unknown indexed corner must not borrow a positional hint");
    const auto payload = source.payloadBytes();
    SourceProvenance exact(snapshot, payload);
    SourceProvenance shortBudget(snapshot, payload - 1);
    auto append = [&](SourceProvenance& output) {
        output.appendCorner(object, object.vertices[0], 0, 0);
        output.appendCorner(object, object.vertices[3], 1, 0);
        output.appendCorner(object, object.vertices[1], 0, 1);
        output.appendCorner(object, object.vertices[4], 1, 1);
    };
    append(exact);
    require(exact.payloadBytes() == payload, "Exact indexed source payload quota must succeed");
    rejects([&] { append(shortBudget); }, DUST3D_STATUS_LIMIT_EXCEEDED);
    snapshot.nodes.clear();
    object.triangleSourceCorners.clear();
    require(nodeAt(source, 1) == second && source.identities()[0].part == "first-original-part",
        "Exported source identities and weights must own their original values");
}
void aliasesAndUnknownInfluences()
{
    auto snapshot = membership();
    snapshot.nodes[first.substr(1, 36)]["partId"] = "distinct-alias-part";
    auto object = coincident();
    SourceProvenance source(snapshot, 1024 * 1024);
    source.appendCorner(object, {}, 0, 0);
    require(source.corners()[0].kind == DUST3D_SOURCE_AMBIGUOUS
            && source.influences()[0].source_index == SIZE_MAX,
        "Indexed UUID aliases must not select an arbitrary original key");
    auto& record = object.triangleSourceCorners[0][0];
    record.weighted = true;
    record.setInfluences({ { Uuid(first), .5f }, { Uuid(absent), .5f } });
    source.appendCorner(object, {}, 0, 0);
    const auto& corner = source.corners()[1];
    require(corner.kind == DUST3D_SOURCE_WEIGHTED_HINT && corner.influence_count == 2
            && source.influences()[corner.first_influence].kind == DUST3D_SOURCE_AMBIGUOUS
            && source.influences()[corner.first_influence + 1].kind == DUST3D_SOURCE_UNKNOWN,
        "Every indexed unresolved weight must retain its explicit identity status");
}
void invalidChannels()
{
    for (int variant = 0; variant < 8; ++variant) {
        auto object = coincident();
        SourceProvenance source(membership(), 1024 * 1024);
        size_t triangle = 0, corner = 0;
        if (variant == 0)
            object.triangleSourceCorners.pop_back();
        else if (variant == 1)
            triangle = SIZE_MAX;
        else if (variant == 2)
            corner = 3;
        else if (variant == 3)
            object.triangleSourceCorners[0][0].setInfluences({ { Uuid(first), 1 }, { Uuid(second), 1 } });
        else if (variant == 4)
            object.triangleSourceCorners[0][0].setInfluences({ { Uuid(first), .5f } });
        else {
            object.triangleSourceCorners[0][0].weighted = true;
            object.triangleSourceCorners[0][0].setInfluences({ { Uuid(first), variant == 5 ? -1.0f : variant == 6 ? std::numeric_limits<float>::infinity()
                                                                                                                  : std::numeric_limits<float>::quiet_NaN() } });
        }
        rejects([&] { source.appendCorner(object, {}, triangle, corner); }, DUST3D_STATUS_GENERATION_FAILED);
    }
    auto object = coincident();
    SourceProvenance source(membership(), 1024 * 1024);
    rejects([&] { source.appendCorner(object, {}, 0); }, DUST3D_STATUS_GENERATION_FAILED);
}
void filteredOriginalOrdinals()
{
    auto object = coincident();
    object.vertices.insert(object.vertices.begin(), { 1000000, 1000000, 1000000 });
    object.vertices.insert(object.vertices.begin() + 1, { 1000000.001, 1000000, 1000000 });
    object.vertices.insert(object.vertices.begin() + 2, { 1000000, 1000000.001, 1000000 });
    object.triangles = { { 0, 1, 2 }, { 3, 4, 5 }, { 6, 7, 8 } };
    object.triangleSourceCorners.insert(object.triangleSourceCorners.begin(), SourceTriangleCorners {});
    SourceProvenance source(membership(), 1024 * 1024);
    GenerationOutputs outputs;
    outputs.sources = &source;
    Dust3DMesh geometry;
    prepareGeneratedObject(object, nullptr, geometry, outputs);
    require(geometry.vertices.size() == 6 && source.corners().size() == 6,
        "Float-collapsed prefix must not consume retained source corner ordinals");
    require(nodeAt(source, 0) == first && nodeAt(source, 3) == second,
        "Retained corners must use original triangle and actual corner indices");
    require(source.corners()[1].kind == DUST3D_SOURCE_WEIGHTED_HINT
            && source.corners()[5].kind == DUST3D_SOURCE_UNKNOWN,
        "Filtered output must preserve exact weighted and unknown corner association");
}
}
int main()
{
    try {
        coincidentCornersAndOwnership();
        aliasesAndUnknownInfluences();
        invalidChannels();
        filteredOriginalOrdinals();
        std::cout << "Indexed source provenance contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
