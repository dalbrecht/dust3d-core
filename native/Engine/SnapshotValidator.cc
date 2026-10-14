#include "EngineInternal.h"
#include <cmath>
#include <dust3d/base/uuid.h>
#include <map>
#include <set>

namespace dust3d {
namespace native {

    namespace {
        using Attributes = std::map<std::string, std::string>;

        double validateNumber(const Attributes& attributes, const std::string& key, bool isRequired = false)
        {
            auto found = attributes.find(key);
            if (found == attributes.end() || found->second.empty()) {
                if (isRequired)
                    throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "A node is missing its " + key + " value.");
                return 0;
            }
            size_t consumed = 0;
            double value = std::stod(found->second, &consumed);
            if (consumed != found->second.size() || !std::isfinite(value) || std::abs(value) > 1000000)
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Invalid or nonfinite geometry value: " + key + ".");
            return value;
        }

        void validateIdentifiers(const std::map<std::string, Attributes>& elements)
        {
            for (const auto& element : elements) {
                if (Uuid(element.first).isNull())
                    throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "The model contains an invalid UUID.");
            }
        }

        void orientEdges(Snapshot& snapshot)
        {
            // The Qt loader normalizes legacy chains before generation. A bounded traversal
            // also prevents flattenLinks from looping forever on inconsistent edge cycles.
            using Neighbor = std::pair<std::string, std::string>;
            std::map<std::string, std::map<std::string, std::vector<Neighbor>>> links;
            for (const auto& edge : snapshot.edges) {
                const auto part = String::valueOrEmpty(edge.second, "partId");
                const auto from = String::valueOrEmpty(edge.second, "from");
                const auto to = String::valueOrEmpty(edge.second, "to");
                links[part][from].push_back({ to, edge.first });
                links[part][to].push_back({ from, edge.first });
            }
            std::map<std::string, size_t> partNodeCounts;
            for (const auto& node : snapshot.nodes)
                ++partNodeCounts[String::valueOrEmpty(node.second, "partId")];
            for (const auto& part : partNodeCounts) {
                if (part.second > 1 && links[part.first].size() != part.second)
                    throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "A part contains disconnected nodes.");
            }
            for (const auto& part : links) {
                const auto& neighbors = part.second;
                if (neighbors.empty())
                    continue;
                for (const auto& node : neighbors) {
                    if (node.second.size() > 2)
                        throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "A part contains branching edges; split it into separate parts first.");
                }
                auto current = neighbors.begin()->first;
                for (const auto& node : neighbors) {
                    if (node.second.size() == 1) {
                        current = node.first;
                        break;
                    }
                }
                std::set<std::string> visitedEdges;
                for (;;) {
                    bool advanced = false;
                    for (const auto& next : neighbors.at(current)) {
                        if (!visitedEdges.insert(next.second).second)
                            continue;
                        auto& edge = snapshot.edges.at(next.second);
                        edge["from"] = current;
                        edge["to"] = next.first;
                        current = next.first;
                        advanced = true;
                        break;
                    }
                    if (!advanced)
                        break;
                }
                size_t edgeCount = 0;
                for (const auto& node : neighbors)
                    edgeCount += node.second.size();
                if (visitedEdges.size() * 2 != edgeCount)
                    throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "A part contains disconnected edge chains.");
            }
        }
    }

    void validateSnapshot(Snapshot& snapshot)
    {
        if (snapshot.nodes.empty() || snapshot.parts.empty())
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "The document contains no procedural geometry.");
        if (snapshot.nodes.size() > 10000 || snapshot.edges.size() > 20000 || snapshot.components.size() > 10000)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "The model exceeds the native viewer's graph limits.");
        validateIdentifiers(snapshot.nodes);
        validateIdentifiers(snapshot.edges);
        validateIdentifiers(snapshot.parts);
        validateIdentifiers(snapshot.components);
        for (const auto* key : { "originX", "originY", "originZ" })
            validateNumber(snapshot.canvas, key);
        for (const auto& node : snapshot.nodes) {
            for (const auto* key : { "x", "y", "z", "radius" })
                validateNumber(node.second, key, true);
            if (validateNumber(node.second, "radius") < 0)
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Node radius cannot be negative.");
            if (!snapshot.parts.count(String::valueOrEmpty(node.second, "partId")))
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "A node references a missing part.");
            for (const auto* key : { "deformWidth", "deformThickness", "cutRotation" })
                validateNumber(node.second, key);
        }
        for (const auto& part : snapshot.parts) {
            if (!String::valueOrEmpty(part.second, "importedModelId").empty()
                && !Uuid(String::valueOrEmpty(part.second, "importedModelId")).isNull())
                throw DocumentError(DUST3D_STATUS_UNSUPPORTED_ASSET, "This document references imported GLB geometry, which is not yet supported.");
            for (const auto* key : { "deformWidth", "deformThickness", "cutRotation", "hollowThickness", "metalness", "roughness" })
                validateNumber(part.second, key);
        }
        for (const auto& edge : snapshot.edges) {
            const auto from = String::valueOrEmpty(edge.second, "from");
            const auto to = String::valueOrEmpty(edge.second, "to");
            const auto part = String::valueOrEmpty(edge.second, "partId");
            if (from == to || !snapshot.nodes.count(from) || !snapshot.nodes.count(to) || !snapshot.parts.count(part)
                || String::valueOrEmpty(snapshot.nodes.at(from), "partId") != part
                || String::valueOrEmpty(snapshot.nodes.at(to), "partId") != part)
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "An edge references missing or incompatible nodes.");
        }
        for (const auto& component : snapshot.components) {
            if (String::valueOrEmpty(component.second, "linkDataType") == "partId"
                && !snapshot.parts.count(String::valueOrEmpty(component.second, "linkData")))
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "A component references a missing part.");
            for (const auto* key : { "targetSegments", "smoothCutoffDegrees", "backCloseDepthRatio", "backCloseSharpness", "wrapOffset", "wrapFaces", "wrapWrinkles" })
                validateNumber(component.second, key);
        }
        orientEdges(snapshot);
    }

}
}
