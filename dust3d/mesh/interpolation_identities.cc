// SPDX-License-Identifier: MIT
#include <algorithm>
#include <cstdint>
#include <dust3d/mesh/interpolation_identities.h>
#include <iomanip>
#include <limits>
#include <locale>
#include <numeric>
#include <sstream>
#include <stdexcept>

namespace dust3d {
namespace {
    bool canonicalUuid(const std::string& id)
    {
        if (id.size() != 38 || id.front() != '{' || id.back() != '}')
            return false;
        for (size_t i = 1; i < 37; ++i) {
            if (i == 9 || i == 14 || i == 19 || i == 24) {
                if (id[i] != '-')
                    return false;
            } else if (!((id[i] >= '0' && id[i] <= '9') || (id[i] >= 'a' && id[i] <= 'f') || (id[i] >= 'A' && id[i] <= 'F'))) {
                return false;
            }
        }
        return true;
    }
    std::string identityKey(std::string id)
    {
        // Preserve source spelling in output while avoiding aliases in UUID-keyed consumers.
        if (id.size() == 36)
            id = '{' + id + '}';
        for (auto& character : id) {
            if (character >= 'A' && character <= 'F')
                character += 'a' - 'A';
        }
        return id;
    }
    std::string rawUuid(const std::string& id)
    {
        return id.substr(1, 8) + id.substr(10, 4) + id.substr(15, 4) + id.substr(20, 4) + id.substr(25, 12);
    }
    std::string formatUuid(const std::string& raw)
    {
        return '{' + raw.substr(0, 8) + '-' + raw.substr(8, 4) + '-' + raw.substr(12, 4)
            + '-' + raw.substr(16, 4) + '-' + raw.substr(20, 12) + '}';
    }
    std::array<std::string, 5> legacyIdentities(const InterpolationEdge& edge)
    {
        const auto from = rawUuid(edge.fromNodeId);
        const auto to = rawUuid(edge.toNodeId);
        const auto source = rawUuid(edge.edgeId);
        return { formatUuid(from.substr(0, 16) + source.substr(16, 16)),
            formatUuid(to.substr(0, 16) + source.substr(16, 16)),
            formatUuid(source.substr(0, 16) + from.substr(16, 16)),
            formatUuid(from.substr(0, 16) + to.substr(16, 16)),
            formatUuid(source.substr(0, 16) + to.substr(16, 16)) };
    }
    std::string fallbackIdentity(uint64_t sequence)
    {
        std::ostringstream raw;
        raw.imbue(std::locale::classic());
        raw << "00000000" << std::hex << std::setfill('0') << std::setw(4) << (sequence >> 48)
            << "40008000" << std::setw(12) << (sequence & UINT64_C(0x0000ffffffffffff));
        return formatUuid(raw.str());
    }
}
std::vector<InterpolatedEdgeIdentities> planInterpolationIdentities(
    const std::vector<InterpolationEdge>& edges,
    const std::set<std::string>& existingNodeIds,
    const std::set<std::string>& existingEdgeIds)
{
    std::set<std::string> occupied;
    for (const auto& id : existingNodeIds)
        occupied.insert(identityKey(id));
    for (const auto& id : existingEdgeIds)
        occupied.insert(identityKey(id));
    std::set<std::string> requestedEdges;
    std::vector<std::array<std::string, 5>> legacy;
    legacy.reserve(edges.size());
    for (const auto& edge : edges) {
        if (!canonicalUuid(edge.edgeId) || !canonicalUuid(edge.fromNodeId) || !canonicalUuid(edge.toNodeId)
            || !existingEdgeIds.count(edge.edgeId) || !existingNodeIds.count(edge.fromNodeId)
            || !existingNodeIds.count(edge.toNodeId) || identityKey(edge.fromNodeId) == identityKey(edge.toNodeId)
            || !requestedEdges.insert(identityKey(edge.edgeId)).second)
            throw std::invalid_argument("Invalid interpolation identity input");
        legacy.push_back(legacyIdentities(edge));
    }
    std::vector<size_t> order(edges.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return identityKey(edges[a].edgeId) < identityKey(edges[b].edgeId);
    });
    std::vector<std::array<bool, 5>> admitted(edges.size());
    // Reserve all usable legacy identities first: an early fallback must not steal a later one.
    for (const auto index : order) {
        for (size_t role = 0; role < 5; ++role)
            admitted[index][role] = occupied.insert(identityKey(legacy[index][role])).second;
    }
    uint64_t sequence = 1;
    auto nextIdentity = [&]() {
        for (;;) {
            const auto candidate = fallbackIdentity(sequence);
            const bool last = sequence == std::numeric_limits<uint64_t>::max();
            if (!last)
                ++sequence;
            if (occupied.insert(candidate).second)
                return candidate;
            if (last)
                throw std::overflow_error("Interpolation identity namespace exhausted");
        }
    };
    std::vector<InterpolatedEdgeIdentities> result(edges.size());
    for (const auto index : order) {
        for (size_t role = 0; role < 5; ++role) {
            const auto id = admitted[index][role] ? legacy[index][role] : nextIdentity();
            if (role < 2)
                result[index].nodeIds[role] = id;
            else
                result[index].edgeIds[role - 2] = id;
        }
    }
    return result;
}
}
