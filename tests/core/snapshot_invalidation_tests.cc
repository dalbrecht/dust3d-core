#include "SnapshotInvalidation.h"
#include <dust3d/base/snapshot.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <tuple>

namespace {
void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

dust3d::Snapshot snapshot()
{
    // Independent metadata graph; this pure analysis treats identities as opaque keys.
    dust3d::Snapshot result;
    result.canvas = { { "originX", ".5" } };
    result.parts = { { "part-a", {} }, { "part-b", {} }, { "profile", {} } };
    result.nodes = {
        { "node-a", { { "partId", "part-a" }, { "x", ".5" }, { "radius", ".1" } } },
        { "node-b", { { "partId", "part-b" }, { "x", ".7" }, { "radius", ".1" } } },
        { "node-profile", { { "partId", "profile" }, { "x", ".9" }, { "radius", ".1" } } }
    };
    result.components = {
        { "group", { { "children", "leaf-a" } } },
        { "leaf-a", { { "linkDataType", "partId" }, { "linkData", "part-a" } } },
        { "leaf-b", { { "linkDataType", "partId" }, { "linkData", "part-b" } } },
        { "leaf-profile", { { "linkDataType", "partId" }, { "linkData", "profile" } } },
        { "wrapper", { { "wrap", "Cloth" }, { "wrapBindTo", "group" } } }
    };
    result.rootComponent = { { "children", "group,leaf-b,leaf-profile,wrapper" } };
    return result;
}

void firstLoadAndNoOp()
{
    const auto source = snapshot();
    const auto initial = dust3d::native::deriveSnapshotInvalidation(nullptr, source);
    require(initial.dirtyPartIds.size() == source.parts.size(), "First load must invalidate all parts");
    require(initial.dirtyComponentIds.size() == source.components.size() && initial.isRootDirty, "First load must invalidate the component tree");
    auto flagged = source;
    flagged.parts.at("part-a")["__dirty"] = "true";
    flagged.components.at("group")["__dirty"] = "true";
    flagged.rootComponent["__dirty"] = "true";
    const auto clean = dust3d::native::deriveSnapshotInvalidation(&source, flagged);
    require(clean.dirtyPartIds.empty() && clean.dirtyComponentIds.empty() && !clean.isRootDirty, "Saved dirty flags cannot control semantic invalidation");
    require(flagged.parts.at("part-a").at("__dirty") == "true", "Analysis must not mutate source metadata");
}

void nodeAndOwnershipChanges()
{
    const auto source = snapshot();
    auto changed = source;
    changed.nodes.at("node-a")["x"] = ".6";
    const auto moved = dust3d::native::deriveSnapshotInvalidation(&source, changed);
    require(moved.dirtyPartIds.count("part-a") && !moved.dirtyPartIds.count("part-b"), "Node movement must invalidate its part without unrelated parts");
    require(moved.dirtyComponentIds.count("leaf-a") && moved.dirtyComponentIds.count("group") && moved.isRootDirty, "Dirty geometry must reach its owners and root");
    require(moved.dirtyComponentIds.count("wrapper"), "Binding consumers must invalidate when their body changes");
    require(!moved.dirtyComponentIds.count("leaf-b"), "Unrelated leaf caches must stay eligible");
    changed = source;
    changed.nodes.at("node-a")["partId"] = "part-b";
    const auto reassigned = dust3d::native::deriveSnapshotInvalidation(&source, changed);
    require(reassigned.dirtyPartIds.count("part-a") && reassigned.dirtyPartIds.count("part-b"), "Ownership changes invalidate both old and new owners");
}

void profileAndComponentChanges()
{
    auto source = snapshot();
    source.parts.at("part-b")["cutFace"] = "profile";
    auto changed = source;
    changed.nodes.at("node-profile")["radius"] = ".2";
    const auto profile = dust3d::native::deriveSnapshotInvalidation(&source, changed);
    require(profile.dirtyPartIds.count("profile") && profile.dirtyPartIds.count("part-b") && !profile.dirtyPartIds.count("part-a"), "Profile dependency changes must invalidate consumers only");
    changed = source;
    changed.components.at("group")["color"] = "#3366cc";
    const auto color = dust3d::native::deriveSnapshotInvalidation(&source, changed);
    require(color.dirtyPartIds.count("part-a") && color.dirtyComponentIds.count("leaf-a"), "Inherited group attributes must invalidate descendant part caches");
    require(!color.dirtyPartIds.count("part-b"), "Other roots cannot be invalidated by an unrelated group attribute");
}

void removalsGlobalChangesAndCycles()
{
    const auto source = snapshot();
    auto changed = source;
    changed.nodes.erase("node-a");
    changed.parts.erase("part-a");
    changed.components.erase("leaf-a");
    changed.components.at("group")["children"] = "";
    const auto removed = dust3d::native::deriveSnapshotInvalidation(&source, changed);
    require(removed.dirtyPartIds.count("part-a") && removed.dirtyComponentIds.count("leaf-a") && removed.dirtyComponentIds.count("group"), "Removed identities and old owners must be evictable");
    changed = source;
    changed.canvas["originX"] = ".6";
    const auto global = dust3d::native::deriveSnapshotInvalidation(&source, changed);
    require(global.dirtyPartIds.size() == source.parts.size() && global.dirtyComponentIds.size() == source.components.size() && global.isRootDirty, "Global generation input changes invalidate everything");
    changed = source;
    changed.components.at("group")["wrapBindTo"] = "wrapper";
    changed.components.at("wrapper")["children"] = "group";
    const auto cyclic = dust3d::native::deriveSnapshotInvalidation(&source, changed);
    require(cyclic.dirtyComponentIds.count("wrapper") && cyclic.dirtyComponentIds.count("group"), "Dependency analysis must terminate even for cyclic metadata");
}
void edgeAndNodeLifecycle()
{
    auto source = snapshot();
    source.nodes["node-a2"] = { { "partId", "part-a" }, { "x", ".6" } };
    source.edges["edge-a"] = { { "partId", "part-a" }, { "from", "node-a" }, { "to", "node-a2" } };
    auto changed = source;
    changed.edges.erase("edge-a");
    auto invalidation = dust3d::native::deriveSnapshotInvalidation(&source, changed);
    require(invalidation.dirtyPartIds == std::set<std::string> { "part-a" }, "Deleting an edge must invalidate its old part");
    invalidation = dust3d::native::deriveSnapshotInvalidation(&changed, source);
    require(invalidation.dirtyPartIds == std::set<std::string> { "part-a" }, "Adding an edge must invalidate its new part");
    changed = source;
    changed.edges.at("edge-a")["from"] = "node-a2";
    changed.edges.at("edge-a")["to"] = "node-a";
    require(dust3d::native::deriveSnapshotInvalidation(&source, changed).dirtyPartIds.count("part-a"), "Edge direction changes must not retain old generation inputs");
    changed.edges.at("edge-a")["partId"] = "part-b";
    invalidation = dust3d::native::deriveSnapshotInvalidation(&source, changed);
    require(invalidation.dirtyPartIds.count("part-a") && invalidation.dirtyPartIds.count("part-b"), "Edge ownership changes must reach both parts");
    changed = source;
    changed.nodes.erase("node-a2");
    require(dust3d::native::deriveSnapshotInvalidation(&source, changed).dirtyPartIds.count("part-a"), "Removed nodes must invalidate old ownership");
    require(dust3d::native::deriveSnapshotInvalidation(&changed, source).dirtyPartIds.count("part-a"), "Added nodes must invalidate new ownership");
}

void nodeProfilesAndTransitiveDependencies()
{
    auto source = snapshot();
    source.nodes.at("node-b")["cutFace"] = "profile";
    source.parts.at("part-a")["cutFace"] = "part-b";
    auto changed = source;
    changed.nodes.at("node-profile")["x"] = "1";
    const auto transitive = dust3d::native::deriveSnapshotInvalidation(&source, changed);
    require(transitive.dirtyPartIds.size() == 3, "Node profile dependencies must propagate transitively to other profile consumers");
    source.parts.at("profile")["cutFace"] = "part-a";
    changed = source;
    changed.nodes.at("node-profile")["radius"] = ".2";
    require(dust3d::native::deriveSnapshotInvalidation(&source, changed).dirtyPartIds.size() == 3, "Cyclic profile metadata must terminate and cover the full dependency closure");
    source = snapshot();
    source.parts.at("part-b")["cutFace"] = "profile";
    changed = source;
    changed.parts.at("part-b").erase("cutFace");
    changed.nodes.at("node-profile")["radius"] = ".2";
    require(dust3d::native::deriveSnapshotInvalidation(&source, changed).dirtyPartIds.count("part-b"), "Old profile references must remain part of removal invalidation");
}

void reparentingOrderingAndBindingChanges()
{
    auto source = snapshot();
    source.components["group-b"] = { { "children", "leaf-b" } };
    source.rootComponent["children"] = "group,group-b,leaf-profile,wrapper";
    auto changed = source;
    changed.components.at("group")["children"] = "";
    changed.components.at("group-b")["children"] = "leaf-b,leaf-a";
    const auto moved = dust3d::native::deriveSnapshotInvalidation(&source, changed);
    require(moved.dirtyPartIds.count("part-a") && moved.dirtyComponentIds.count("group") && moved.dirtyComponentIds.count("group-b"), "Reparenting must invalidate both old and new inheritance paths");
    require(!moved.dirtyPartIds.count("profile"), "Reparenting cannot invalidate an unrelated tree");
    source = changed;
    changed.components.at("group-b")["children"] = "leaf-a,leaf-b";
    require(dust3d::native::deriveSnapshotInvalidation(&source, changed).dirtyComponentIds.count("group-b"), "Ordered Boolean children are generation inputs");
    changed = source;
    changed.components.at("wrapper")["wrapBindTo"] = "group-b";
    require(dust3d::native::deriveSnapshotInvalidation(&source, changed).dirtyComponentIds.count("wrapper"), "A changed binding must invalidate the wrapping component");
    changed = source;
    changed.components.erase("group");
    require(dust3d::native::deriveSnapshotInvalidation(&source, changed).dirtyComponentIds.count("wrapper"), "Deleted binding targets must invalidate old consumers");
}

void opaqueMetadataDirtyFlagsAndImmutability()
{
    auto source = snapshot();
    source.edges["edge"] = { { "partId", "part-a" } };
    source.animations["clip"] = { { "length", "1" } };
    auto flagged = source;
    flagged.canvas["__dirty"] = "true";
    flagged.rootComponent["__dirty"] = "true";
    for (auto* elements : { &flagged.parts, &flagged.nodes, &flagged.edges, &flagged.components, &flagged.animations }) {
        for (auto& element : *elements)
            element.second["__dirty"] = "true";
    }
    const auto before = flagged;
    for (const auto& pair : { std::make_pair(&source, &flagged), std::make_pair(&flagged, &source) }) {
        const auto ignored = dust3d::native::deriveSnapshotInvalidation(pair.first, *pair.second);
        require(ignored.dirtyPartIds.empty() && ignored.dirtyComponentIds.empty() && !ignored.isRootDirty, "Persisted dirty metadata is never semantic input in either direction");
    }
    require(std::tie(flagged.canvas, flagged.nodes, flagged.edges, flagged.parts, flagged.components, flagged.rootComponent, flagged.animations)
            == std::tie(before.canvas, before.nodes, before.edges, before.parts, before.components, before.rootComponent, before.animations),
        "Analysis must preserve every source map and attribute");
    auto changed = source;
    changed.parts.at("part-a")["future-property"] = "preserved";
    require(dust3d::native::deriveSnapshotInvalidation(&source, changed).dirtyPartIds.count("part-a"), "Unknown properties must invalidate conservatively");
    changed = source;
    changed.animations.at("clip")["length"] = "2";
    require(dust3d::native::deriveSnapshotInvalidation(&source, changed).dirtyPartIds.size() == source.parts.size(), "Animation input changes must not reuse stale future generation state");
    changed = source;
    changed.rootComponent["children"] = "wrapper,leaf-profile,leaf-b,group";
    require(dust3d::native::deriveSnapshotInvalidation(&source, changed).isRootDirty, "Root composition order must invalidate");
}

void emptyAndCollidingNamespaces()
{
    const dust3d::Snapshot empty;
    require(dust3d::native::deriveSnapshotInvalidation(nullptr, empty).isRootDirty, "First empty input still establishes a generation revision");
    const auto unchanged = dust3d::native::deriveSnapshotInvalidation(&empty, empty);
    require(unchanged.dirtyPartIds.empty() && unchanged.dirtyComponentIds.empty() && !unchanged.isRootDirty, "Unchanged empty snapshots are a no-op");
    auto source = snapshot();
    source.parts["group"] = {};
    auto changed = source;
    changed.parts.at("group")["deformWidth"] = "2";
    const auto collision = dust3d::native::deriveSnapshotInvalidation(&source, changed);
    require(collision.dirtyPartIds.count("group") && collision.dirtyComponentIds.empty(), "Part and component identifiers occupy different dependency namespaces");
    source.nodes["unowned"] = {};
    changed = source;
    changed.nodes.at("unowned")["future"] = "value";
    require(dust3d::native::deriveSnapshotInvalidation(&source, changed).dirtyPartIds.empty(), "Analysis must remain safe for incomplete metadata; validation remains separate");
}

void emptyChildReferencesRemainEmpty()
{
    auto source = snapshot();
    // A direct Snapshot producer can retain empty comma-separated child slots.
    source.components.at("group")["children"] = ",leaf-a,,";
    auto changed = source;
    changed.nodes.at("node-a")["radius"] = ".2";
    const auto invalidation = dust3d::native::deriveSnapshotInvalidation(&source, changed);
    require(invalidation.dirtyComponentIds.count("group") && invalidation.isRootDirty, "Empty child slots cannot break propagation through existing children");
    require(!invalidation.dirtyComponentIds.count(""), "Empty child slots cannot become component identities");
}

}

int main()
{
    try {
        firstLoadAndNoOp();
        nodeAndOwnershipChanges();
        profileAndComponentChanges();
        removalsGlobalChangesAndCycles();
        edgeAndNodeLifecycle();
        nodeProfilesAndTransitiveDependencies();
        reparentingOrderingAndBindingChanges();
        opaqueMetadataDirtyFlagsAndImmutability();
        emptyAndCollidingNamespaces();
        emptyChildReferencesRemainEmpty();
        std::cout << "Semantic snapshot invalidation contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
