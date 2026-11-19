// SPDX-License-Identifier: MIT
#include "SnapshotReferences.h"
#include "EngineInternal.h"
#include <dust3d/base/string.h>
#include <dust3d/base/uuid.h>
#include <map>
#include <string>
#include <utility>

namespace dust3d {
namespace native {
    namespace {
        using Attributes = std::map<std::string, std::string>;
        using Elements = std::map<std::string, Attributes>;

        // One borrowed authored key per distinct UUID, or null for ambiguity.
        // Index size is bounded by the already-parsed entity collection; keys are
        // never inserted/erased while these references are used.
        class ReferenceIndex {
        public:
            explicit ReferenceIndex(const Elements& elements)
                : m_elements(elements)
            {
                for (const auto& element : elements) {
                    const Uuid id(element.first);
                    if (id.isNull())
                        continue;
                    const auto result = m_aliases.emplace(id, &element.first);
                    if (!result.second)
                        result.first->second = nullptr;
                }
            }

            const std::string& resolve(const std::string& reference, const char* field) const
            {
                const auto exact = m_elements.find(reference);
                if (exact != m_elements.end())
                    return exact->first;
                const Uuid id(reference);
                if (id.isNull())
                    return reference;
                const auto found = m_aliases.find(id);
                if (found == m_aliases.end())
                    return reference;
                if (!found->second)
                    throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, std::string("An aliased ") + field + " reference is ambiguous.");
                return *found->second;
            }

        private:
            const Elements& m_elements;
            std::map<Uuid, const std::string*> m_aliases;
        };

        void resolveAttribute(Attributes& attributes, const char* field, const ReferenceIndex& index, bool uuidOnly = false)
        {
            const auto found = attributes.find(field);
            if (found == attributes.end() || (uuidOnly && Uuid(found->second).isNull()))
                return;
            found->second = index.resolve(found->second, field);
        }

        void resolveChildren(Attributes& attributes, const ReferenceIndex& index)
        {
            const auto found = attributes.find("children");
            if (found == attributes.end())
                return;
            const auto& source = found->second;
            std::string resolved;
            resolved.reserve(source.size());
            size_t start = 0;
            for (;;) {
                const auto comma = source.find(',', start);
                const auto count = comma == std::string::npos ? source.size() - start : comma - start;
                const auto reference = source.substr(start, count);
                resolved += index.resolve(reference, "children");
                if (comma == std::string::npos)
                    break;
                resolved += ',';
                start = comma + 1;
            }
            found->second = std::move(resolved);
        }
    }

    void resolveSnapshotReferences(Snapshot& snapshot)
    {
        const ReferenceIndex nodes(snapshot.nodes);
        const ReferenceIndex parts(snapshot.parts);
        const ReferenceIndex components(snapshot.components);
        for (auto& node : snapshot.nodes) {
            resolveAttribute(node.second, "partId", parts);
            resolveAttribute(node.second, "cutFace", parts, true);
        }
        for (auto& edge : snapshot.edges) {
            resolveAttribute(edge.second, "from", nodes);
            resolveAttribute(edge.second, "to", nodes);
            resolveAttribute(edge.second, "partId", parts);
        }
        for (auto& part : snapshot.parts)
            resolveAttribute(part.second, "cutFace", parts, true);
        for (auto& component : snapshot.components) {
            const auto type = component.second.find("linkDataType");
            if (type != component.second.end() && type->second == "partId") {
                resolveAttribute(component.second, "linkData", parts);
                // Apply the inherited mode to the selected authored owner, including exact UUID aliases.
                const auto part = snapshot.parts.find(String::valueOrEmpty(component.second, "linkData"));
                if (part != snapshot.parts.end() && String::isTrue(String::valueOrEmpty(part->second, "inverse")))
                    component.second["combineMode"] = "Inversion";
            }
            resolveAttribute(component.second, "wrapBindTo", components, true);
            resolveChildren(component.second, components);
        }
        resolveChildren(snapshot.rootComponent, components);
    }
}
}
