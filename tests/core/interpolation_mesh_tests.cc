// SPDX-License-Identifier: MIT
#include <algorithm>
#include <cmath>
#include <dust3d/base/object.h>
#include <dust3d/base/retained_payload_budget.h>
#include <dust3d/base/string.h>
#include <dust3d/mesh/mesh_generator.h>
#include <dust3d/rig/rig_generator.h>
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>

namespace {
void require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}
const std::string part = "{aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa}";
const std::string component = "{cccccccc-cccc-4ccc-8ccc-cccccccccccc}";
const std::string edge = "{30000000-0000-4000-8000-000000000021}";
const std::string a = "{20000000-0000-4000-8000-000000000011}";
const std::string b = "{20000000-0000-4000-8000-000000000012}";
std::unique_ptr<dust3d::Snapshot> snapshot(bool reservedOriginals, bool mirrored)
{
    auto source = std::make_unique<dust3d::Snapshot>();
    source->canvas = { { "originX", ".5" }, { "originY", ".5" }, { "originZ", ".5" } };
    source->parts[part] = { { "id", part }, { "rounded", "false" }, { "xMirrored", mirrored ? "true" : "false" } };
    source->nodes[a] = { { "id", a }, { "partId", part }, { "x", ".3" }, { "y", "0" }, { "z", ".5" }, { "radius", ".1" }, { "deformWidth", "1" }, { "deformThickness", ".5" }, { "name", "Original from" } };
    source->nodes[b] = { { "id", b }, { "partId", part }, { "x", ".3" }, { "y", "1" }, { "z", ".5" }, { "radius", ".1" }, { "deformWidth", "2" }, { "deformThickness", "1.5" }, { "name", "Original to" } };
    source->edges[edge] = { { "id", edge }, { "partId", part }, { "from", a }, { "to", b }, { "boneName", "Spine" } };
    source->components[component] = { { "id", component }, { "linkDataType", "partId" }, { "linkData", part }, { "combineMode", "Uncombined" } };
    source->rootComponent["children"] = component;
    if (reservedOriginals) {
        const std::string otherPart = "{bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb}";
        const std::string otherComponent = "{dddddddd-dddd-4ddd-8ddd-dddddddddddd}";
        const std::string reservedNode = "{20000000-0000-4000-8000-000000000021}";
        const std::string otherNode = "{77777777-7777-4777-8777-777777777777}";
        const std::string reservedEdge = "{30000000-0000-4000-8000-000000000011}";
        source->parts[otherPart] = { { "id", otherPart }, { "interpolated", "false" }, { "rounded", "false" } };
        source->nodes[reservedNode] = { { "id", reservedNode }, { "partId", otherPart }, { "x", "3" }, { "y", "0" }, { "z", ".5" }, { "radius", ".1" }, { "name", "Reserved original" } };
        source->nodes[otherNode] = { { "id", otherNode }, { "partId", otherPart }, { "x", "3" }, { "y", "1" }, { "z", ".5" }, { "radius", ".1" } };
        source->edges[reservedEdge] = { { "id", reservedEdge }, { "partId", otherPart }, { "from", reservedNode }, { "to", otherNode } };
        source->components[otherComponent] = { { "id", otherComponent }, { "linkDataType", "partId" }, { "linkData", otherPart }, { "combineMode", "Uncombined" } };
        source->rootComponent["children"] += ',' + otherComponent;
    }
    return source;
}
void ordinaryInterpolation(bool reservedOriginals, bool mirrored)
{
    auto source = snapshot(reservedOriginals, mirrored);
    const auto originalNodes = source->nodes;
    const auto originalEdges = source->edges;
    dust3d::MeshGenerator generator(source.release());
    generator.generate();
    require(generator.isSuccessful(), "Ordinary interpolated tube failed generation");
    std::unique_ptr<dust3d::Object> object(generator.takeObject());
    std::unique_ptr<dust3d::Snapshot> processed(generator.takeSnapshot());
    require(object && !object->vertices.empty() && !object->triangles.empty(), "Interpolation lost ordinary geometry");
    for (const auto& position : object->vertices)
        require(std::isfinite(position.x()) && std::isfinite(position.y()) && std::isfinite(position.z()), "Interpolation produced a nonfinite position");
    for (const auto& triangle : object->triangles)
        for (const auto index : triangle)
            require(index < object->vertices.size(), "Interpolation lost triangle ownership");
    const auto expectedNodes = originalNodes.size() + 2;
    const auto expectedEdges = originalEdges.size() + 2;
    require(processed->nodes.size() == (mirrored ? expectedNodes * 2 : expectedNodes), "Interpolation overwrote an original or generated node");
    require(processed->edges.size() == (mirrored ? expectedEdges * 2 : expectedEdges), "Interpolation overwrote an original or generated edge");
    for (const auto& node : originalNodes) {
        for (const auto& attribute : node.second)
            require(processed->nodes.at(node.first).at(attribute.first) == attribute.second, "Interpolation changed original node metadata");
    }
    for (const auto& original : originalEdges) {
        if (original.first != edge)
            require(processed->edges.at(original.first) == original.second, "Interpolation replaced another part's original edge");
    }
    std::vector<std::pair<float, std::string>> inserted;
    for (const auto& node : processed->nodes) {
        if (!originalNodes.count(node.first) && dust3d::String::valueOrEmpty(node.second, "partId") == part)
            inserted.emplace_back(dust3d::String::toFloat(node.second.at("y")), node.first);
    }
    std::sort(inserted.begin(), inserted.end());
    require(inserted.size() == 2 && std::abs(inserted[0].first - .1f) < 1e-5f && std::abs(inserted[1].first - .9f) < 1e-5f,
        "Interpolation changed the inherited ring positions");
    for (size_t index = 0; index < inserted.size(); ++index) {
        const auto& node = processed->nodes.at(inserted[index].second);
        require(std::abs(dust3d::String::toFloat(node.at("radius")) - .1f) < 1e-5f, "Interpolation changed radius transport");
        require(std::abs(dust3d::String::toFloat(node.at("deformWidth")) - (index == 0 ? 1.1f : 1.9f)) < 1e-5f,
            "Interpolation changed width transport");
        require(std::abs(dust3d::String::toFloat(node.at("deformThickness")) - (index == 0 ? .6f : 1.4f)) < 1e-5f,
            "Interpolation changed thickness transport");
    }
    std::set<std::string> visited { a };
    auto cursor = a;
    for (size_t segment = 0; segment < 3; ++segment) {
        const std::map<std::string, std::string>* next = nullptr;
        for (const auto& candidate : processed->edges) {
            if (dust3d::String::valueOrEmpty(candidate.second, "partId") == part && candidate.second.at("from") == cursor) {
                require(next == nullptr, "Interpolation introduced a branch");
                next = &candidate.second;
            }
        }
        require(next && next->at("boneName") == "Spine", "Interpolation lost bone metadata");
        cursor = next->at("to");
        require(visited.insert(cursor).second && processed->nodes.count(cursor), "Interpolation introduced a self-edge, cycle or missing endpoint");
    }
    require(cursor == b && visited.size() == 4, "Interpolated chain lost an original endpoint");
    dust3d::RigStructure templateRig;
    templateRig.type = "Custom";
    dust3d::RigNode bone;
    bone.name = "Spine";
    bone.endY = -1;
    bone.capsuleRadius = .1f;
    templateRig.bones.push_back(bone);
    dust3d::RigGenerator rig;
    dust3d::RigStructure actual;
    require(rig.generateRig(processed.get(), templateRig, actual), "Rest-rig generation rejected valid interpolated topology");
    require(actual.bones.size() == 1, "Interpolation changed the rig bone count");
    const auto& actualBone = actual.bones.front();
    const float dx = actualBone.endX - actualBone.posX, dy = actualBone.endY - actualBone.posY, dz = actualBone.endZ - actualBone.posZ;
    require(std::sqrt(dx * dx + dy * dy + dz * dz) > .9f, "Interpolated identity alias collapsed the rig bone");
    std::map<dust3d::Uuid, dust3d::NodeBoneInfluence> influences;
    require(rig.computeNodeBoneInfluences(processed.get(), actual, influences), "Interpolated node influence generation failed");
    for (const auto& id : visited)
        require(influences.at(dust3d::Uuid(id)).primary == "Spine", "Distinct interpolated nodes lost bone ownership");
}
std::set<std::string> objectNodeIds(const dust3d::Object& object)
{
    std::set<std::string> result;
    for (const auto& node : object.nodeMap)
        result.insert(node.first.toString());
    return result;
}
void changedReservationInvalidatesOnlyAffectedCachedIdentity()
{
    dust3d::MeshGenerator::GeneratedCacheContext cache;
    auto original = snapshot(false, false);
    const std::string stablePart = "{eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee}";
    const std::string stableComponent = "{ffffffff-ffff-4fff-8fff-ffffffffffff}";
    const std::string stableNode = "{99999999-9999-4999-8999-999999999999}";
    original->parts[stablePart] = { { "id", stablePart }, { "interpolated", "false" } };
    original->nodes[stableNode] = { { "id", stableNode }, { "partId", stablePart }, { "x", "4" }, { "y", "0" }, { "z", ".5" }, { "radius", ".1" } };
    original->components[stableComponent] = { { "id", stableComponent }, { "linkDataType", "partId" }, { "linkData", stablePart }, { "combineMode", "Uncombined" } };
    original->rootComponent["children"] += ',' + stableComponent;
    auto next = std::make_unique<dust3d::Snapshot>(*original);
    dust3d::MeshGenerator initial(original.release());
    initial.setGeneratedCacheContext(&cache);
    initial.generate();
    require(initial.isSuccessful(), "Initial cache fixture failed");
    std::unique_ptr<dust3d::Object> initialObject(initial.takeObject());
    std::unique_ptr<dust3d::Snapshot> initialSnapshot(initial.takeSnapshot());
    std::string reserved;
    for (const auto& node : initialSnapshot->nodes) {
        if (node.first != a && node.first != b && node.second.at("partId") == part) {
            reserved = node.first;
            break;
        }
    }
    require(!reserved.empty(), "Initial fixture has no generated identity to reserve");
    const std::string extraPart = "{bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb}";
    const std::string extraComponent = "{dddddddd-dddd-4ddd-8ddd-dddddddddddd}";
    next->parts[extraPart] = { { "id", extraPart }, { "interpolated", "false" }, { "__dirty", "true" } };
    next->nodes[reserved] = { { "id", reserved }, { "partId", extraPart }, { "x", "3" }, { "y", "0" }, { "z", ".5" }, { "radius", ".1" } };
    next->components[extraComponent] = { { "id", extraComponent }, { "linkDataType", "partId" }, { "linkData", extraPart }, { "combineMode", "Uncombined" }, { "__dirty", "true" } };
    next->rootComponent["children"] += ',' + extraComponent;
    next->rootComponent["__dirty"] = "true";
    auto freshSource = std::make_unique<dust3d::Snapshot>(*next);
    auto repeatedSource = std::make_unique<dust3d::Snapshot>(*next);
    dust3d::MeshGenerator cached(next.release());
    cached.setGeneratedCacheContext(&cache);
    cached.generate();
    require(cached.isSuccessful(), "Changed reservation cache fixture failed");
    std::unique_ptr<dust3d::Object> cachedObject(cached.takeObject());
    dust3d::MeshGenerator fresh(freshSource.release());
    fresh.generate();
    require(fresh.isSuccessful(), "Fresh reservation fixture failed");
    std::unique_ptr<dust3d::Object> freshObject(fresh.takeObject());
    require(objectNodeIds(*cachedObject) == objectNodeIds(*freshObject),
        "An unrelated original ID changed interpolation identities but cached source ownership was reused");
    require(cached.generationMetrics().rebuiltParts == 2 && cached.generationMetrics().reusedComponents > 0,
        "Changed interpolation identity must rebuild its part while retaining unrelated component reuse");
    repeatedSource->parts[extraPart]["__dirty"] = "false";
    repeatedSource->components[extraComponent]["__dirty"] = "false";
    repeatedSource->rootComponent["__dirty"] = "false";
    dust3d::MeshGenerator repeated(repeatedSource.release());
    repeated.setGeneratedCacheContext(&cache);
    repeated.generate();
    require(repeated.isSuccessful(), "Unchanged identity cache fixture failed");
    std::unique_ptr<dust3d::Object> repeatedObject(repeated.takeObject());
    require(objectNodeIds(*repeatedObject) == objectNodeIds(*freshObject), "Unchanged cache ownership drifted");
    require(repeated.generationMetrics().rebuiltParts == 0 && repeated.generationMetrics().reusedComponents > 0,
        "Unchanged interpolation plans should retain ordinary component reuse");
    auto disabled = snapshot(false, false);
    disabled->parts[part]["interpolated"] = "false";
    disabled->rootComponent["__dirty"] = "true";
    dust3d::MeshGenerator noInterpolation(disabled.release());
    noInterpolation.setGeneratedCacheContext(&cache);
    noInterpolation.generate();
    require(noInterpolation.isSuccessful(), "Interpolation opt-out cache fixture failed");
    std::unique_ptr<dust3d::Object> disabledObject(noInterpolation.takeObject());
    require(objectNodeIds(*disabledObject) == std::set<std::string> { a, b },
        "Removed interpolation plan retained stale generated node ownership");
}

void retainedIdentityPlansAreAccountedAndReleased()
{
    dust3d::MeshGenerator::GeneratedCacheContext cache;
    dust3d::RetainedPayloadBudget empty(1024 * 1024);
    empty.accountObject(cache);
    cache.interpolationIdentityPlans[part] = std::string(512, 'i');
    dust3d::RetainedPayloadBudget populated(1024 * 1024);
    populated.accountObject(cache);
    require(!populated.isExceeded() && populated.usedBytes() >= empty.usedBytes() + 512,
        "Retained interpolation identity plans escaped cache payload accounting");
    cache.interpolationIdentityPlans.clear();
    dust3d::RetainedPayloadBudget cleared(1024 * 1024);
    cleared.accountObject(cache);
    require(cleared.usedBytes() == empty.usedBytes(), "Removed interpolation plans retained owned payload");
}

}
int main()
{
    try {
        ordinaryInterpolation(false, false);
        ordinaryInterpolation(true, false);
        ordinaryInterpolation(false, true);
        changedReservationInvalidatesOnlyAffectedCachedIdentity();
        retainedIdentityPlansAreAccountedAndReleased();
        std::cout << "Ordinary interpolation mesh and rest-rig contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
