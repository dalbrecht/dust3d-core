#include "SnapshotInvalidation.h"
#include <deque>
#include <dust3d/base/string.h>
#include <utility>

namespace dust3d {
namespace native {
    namespace {
        using Attributes = std::map<std::string, std::string>;
        using Elements = std::map<std::string, Attributes>;
        enum class Kind { Part,
            Component,
            Root };
        using Identity = std::pair<Kind, std::string>;

        bool attributesEqual(const Attributes& first, const Attributes& second)
        {
            auto left = first.begin();
            auto right = second.begin();
            for (;;) {
                while (left != first.end() && left->first == "__dirty")
                    ++left;
                while (right != second.end() && right->first == "__dirty")
                    ++right;
                if (left == first.end() || right == second.end())
                    return left == first.end() && right == second.end();
                if (*left != *right)
                    return false;
                ++left;
                ++right;
            }
        }

        std::set<std::string> changedIdentities(const Elements& previous, const Elements& current)
        {
            std::set<std::string> changed;
            for (const auto& entry : previous) {
                const auto found = current.find(entry.first);
                if (found == current.end() || !attributesEqual(entry.second, found->second))
                    changed.insert(entry.first);
            }
            for (const auto& entry : current) {
                if (!previous.count(entry.first))
                    changed.insert(entry.first);
            }
            return changed;
        }

        void includeAll(SnapshotInvalidation& result, const Snapshot& snapshot)
        {
            for (const auto& part : snapshot.parts)
                result.dirtyPartIds.insert(part.first);
            for (const auto& component : snapshot.components)
                result.dirtyComponentIds.insert(component.first);
            result.isRootDirty = true;
        }

        struct Dependencies {
            std::map<Identity, std::set<Identity>> consumers;
            std::map<std::string, std::set<std::string>> children;
            std::map<std::string, std::set<std::string>> linkedParts;

            void addComponent(const std::string& id, const Attributes& attributes, bool isRoot)
            {
                const Identity owner { isRoot ? Kind::Root : Kind::Component, id };
                for (const auto& child : String::split(String::valueOrEmpty(attributes, "children"), ',')) {
                    if (child.empty())
                        continue;
                    consumers[{ Kind::Component, child }].insert(owner);
                    if (!isRoot)
                        children[id].insert(child);
                }
                if (String::valueOrEmpty(attributes, "linkDataType") == "partId") {
                    const auto part = String::valueOrEmpty(attributes, "linkData");
                    if (!part.empty()) {
                        consumers[{ Kind::Part, part }].insert(owner);
                        if (!isRoot)
                            linkedParts[id].insert(part);
                    }
                }
                const auto binding = String::valueOrEmpty(attributes, "wrapBindTo");
                if (!binding.empty())
                    consumers[{ Kind::Component, binding }].insert(owner);
            }

            void addSnapshot(const Snapshot& snapshot)
            {
                for (const auto& component : snapshot.components)
                    addComponent(component.first, component.second, false);
                addComponent("", snapshot.rootComponent, true);
                for (const auto& part : snapshot.parts) {
                    const auto profile = String::valueOrEmpty(part.second, "cutFace");
                    if (snapshot.parts.count(profile))
                        consumers[{ Kind::Part, profile }].insert({ Kind::Part, part.first });
                }
                for (const auto& node : snapshot.nodes) {
                    const auto profile = String::valueOrEmpty(node.second, "cutFace");
                    const auto owner = String::valueOrEmpty(node.second, "partId");
                    if (!owner.empty() && snapshot.parts.count(profile))
                        consumers[{ Kind::Part, profile }].insert({ Kind::Part, owner });
                }
            }
        };

        void includeOwner(SnapshotInvalidation& result, const Elements& elements, const std::string& id)
        {
            const auto found = elements.find(id);
            if (found == elements.end())
                return;
            const auto owner = String::valueOrEmpty(found->second, "partId");
            if (!owner.empty())
                result.dirtyPartIds.insert(owner);
        }
    }

    SnapshotInvalidation deriveSnapshotInvalidation(const Snapshot* previous, const Snapshot& current)
    {
        SnapshotInvalidation result;
        if (!previous) {
            includeAll(result, current);
            return result;
        }
        if (!attributesEqual(previous->canvas, current.canvas)
            || !attributesEqual(previous->rootComponent, current.rootComponent)
            || !changedIdentities(previous->animations, current.animations).empty()) {
            includeAll(result, *previous);
            includeAll(result, current);
            return result;
        }
        Dependencies dependencies;
        dependencies.addSnapshot(*previous);
        dependencies.addSnapshot(current);
        result.dirtyPartIds = changedIdentities(previous->parts, current.parts);
        for (const auto& id : changedIdentities(previous->nodes, current.nodes)) {
            includeOwner(result, previous->nodes, id);
            includeOwner(result, current.nodes, id);
        }
        for (const auto& id : changedIdentities(previous->edges, current.edges)) {
            includeOwner(result, previous->edges, id);
            includeOwner(result, current.edges, id);
        }
        // An authored group change may alter inherited attributes of every descendant.
        // Aggregate invalidation from a child must not flow back down into unrelated siblings.
        std::deque<std::string> descendants;
        for (const auto& id : changedIdentities(previous->components, current.components))
            descendants.push_back(id);
        while (!descendants.empty()) {
            const auto id = descendants.front();
            descendants.pop_front();
            if (!result.dirtyComponentIds.insert(id).second)
                continue;
            for (const auto& part : dependencies.linkedParts[id])
                result.dirtyPartIds.insert(part);
            for (const auto& child : dependencies.children[id])
                descendants.push_back(child);
        }
        std::deque<Identity> pending;
        for (const auto& part : result.dirtyPartIds)
            pending.push_back({ Kind::Part, part });
        for (const auto& component : result.dirtyComponentIds)
            pending.push_back({ Kind::Component, component });
        std::set<Identity> visited;
        while (!pending.empty()) {
            const auto identity = pending.front();
            pending.pop_front();
            if (!visited.insert(identity).second)
                continue;
            if (identity.first == Kind::Part)
                result.dirtyPartIds.insert(identity.second);
            else if (identity.first == Kind::Component)
                result.dirtyComponentIds.insert(identity.second);
            for (const auto& consumer : dependencies.consumers[identity])
                pending.push_back(consumer);
        }
        result.isRootDirty = !visited.empty();
        return result;
    }
}
}
