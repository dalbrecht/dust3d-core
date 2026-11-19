// SPDX-License-Identifier: MIT
#include <cmath>
#include <dust3d/rig/rig_generator.h>
#include <iostream>
#include <stdexcept>
using namespace dust3d;
namespace {
void require(bool condition, const char* cause)
{
    if (!condition)
        throw std::runtime_error(cause);
}
const Uuid first("{11111111-1111-4111-8111-111111111111}");
const Uuid second("{22222222-2222-4222-8222-222222222222}");
Object geometry()
{
    Object result;
    result.vertices = { Vector3(0, 0, 0), Vector3(1, 0, 0), Vector3(2, 0, 0) };
    result.positionToNodeIdMap.emplace(PositionKey(result.vertices[0]), first);
    result.positionToNodeIdMap.emplace(PositionKey(result.vertices[1]), second);
    return result;
}
void removingAllInfluencesClearsEveryChannel()
{
    auto object = geometry();
    RigGenerator generator;
    std::map<Uuid, NodeBoneInfluence> influences { { first, NodeBoneInfluence("A", "B", .25f) }, { second, NodeBoneInfluence("A") } };
    require(generator.computeVertexBoneBindings(&object, influences), "Initial ordinary data binding");
    require(object.vertexBone1[0].first == "A" && object.vertexBone2[0].first == "B", "Initial two-slot binding");
    const auto positions = object.vertices;
    const auto sources = object.positionToNodeIdMap;
    influences.clear();
    require(generator.computeVertexBoneBindings(&object, influences), "Empty replacement succeeds");
    for (const auto* channel : { &object.vertexBone1, &object.vertexBone2, &object.vertexBone3, &object.vertexBone4 }) {
        require(channel->size() == positions.size(), "All four channels remain parallel");
        for (const auto& binding : *channel)
            require(binding.first.empty() && binding.second == 0, "Removed influences must clear stale names and weights");
    }
    require(object.positionToNodeIdMap == sources, "Source associations are preserved");
    for (size_t i = 0; i < positions.size(); ++i)
        for (size_t axis = 0; axis < 3; ++axis)
            require(object.vertices[i][axis] == positions[i][axis], "Binding does not move geometry");
}
void nearestFallbackUsesOnlyCurrentBindings()
{
    auto object = geometry();
    RigGenerator generator;
    std::map<Uuid, NodeBoneInfluence> influences { { first, NodeBoneInfluence("OldA") }, { second, NodeBoneInfluence("OldB") } };
    require(generator.computeVertexBoneBindings(&object, influences), "Initial source bindings");
    require(object.vertexBone1[1].first == "OldB", "Initial second node has its own influence");
    influences = { { first, NodeBoneInfluence("Current") } };
    require(generator.computeVertexBoneBindings(&object, influences), "Rebinding current source");
    for (size_t i = 0; i < object.vertices.size(); ++i) {
        require(object.vertexBone1[i].first == "Current" && object.vertexBone1[i].second == 1, "Nearest fallback must use current bound vertex only");
        for (const auto* channel : { &object.vertexBone2, &object.vertexBone3, &object.vertexBone4 })
            require((*channel)[i].first.empty() && (*channel)[i].second == 0, "Unused channels stay empty");
    }
}
void wrappedFourInfluencesAreReplacedOnRemoval()
{
    Object object;
    object.vertices = { Vector3(0, 0, 0) };
    const Uuid third("{33333333-3333-4333-8333-333333333333}");
    const Uuid fourth("{44444444-4444-4444-8444-444444444444}");
    object.positionToNodeWeights.emplace(PositionKey(object.vertices[0]),
        std::vector<std::pair<Uuid, float>> { { first, .1f }, { second, .2f }, { third, .3f }, { fourth, .4f } });
    std::map<Uuid, NodeBoneInfluence> influences { { first, NodeBoneInfluence("A") }, { second, NodeBoneInfluence("B") },
        { third, NodeBoneInfluence("C") }, { fourth, NodeBoneInfluence("D") } };
    RigGenerator generator;
    require(generator.computeVertexBoneBindings(&object, influences), "Ordinary four-node wrap binding");
    const std::string names[] = { "D", "C", "B", "A" };
    const float weights[] = { .4f, .3f, .2f, .1f };
    size_t slot = 0;
    for (const auto* channel : { &object.vertexBone1, &object.vertexBone2, &object.vertexBone3, &object.vertexBone4 }) {
        require((*channel)[0].first == names[slot] && std::abs((*channel)[0].second - weights[slot]) < .000001f,
            "First binding preserves all four normalized wrap influences");
        ++slot;
    }
    influences.clear();
    require(generator.computeVertexBoneBindings(&object, influences), "Removing all wrap influences succeeds");
    for (const auto* channel : { &object.vertexBone1, &object.vertexBone2, &object.vertexBone3, &object.vertexBone4 })
        require((*channel)[0].first.empty() && (*channel)[0].second == 0, "Removed wrap sources clear every channel");
}

}
int main()
{
    try {
        removingAllInfluencesClearsEveryChannel();
        nearestFallbackUsesOnlyCurrentBindings();
        wrappedFourInfluencesAreReplacedOnRemoval();
        std::cout << "Rig vertex rebinding contracts passed\n";
        return 0;
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}
