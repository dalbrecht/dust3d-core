// SPDX-License-Identifier: MIT
#ifndef DUST3D_INTERPOLATION_IDENTITIES_H_
#define DUST3D_INTERPOLATION_IDENTITIES_H_

#include <array>
#include <set>
#include <string>
#include <vector>

namespace dust3d {
/// One original edge and its endpoint identities, before any snapshot mutation.
struct InterpolationEdge {
    std::string edgeId;
    std::string fromNodeId;
    std::string toNodeId;
};

/// Inserted nodes run from-side to to-side; edges run from-to in three consecutive segments.
struct InterpolatedEdgeIdentities {
    std::array<std::string, 2> nodeIds;
    std::array<std::string, 3> edgeIds;
};

/// Preflight every generated identity before interpolation mutates a snapshot.
/// @param edges Existing edges to split, with canonical braced UUID endpoints.
/// @param existingNodeIds All original node keys, including nodes in other parts.
/// @param existingEdgeIds All original edge keys, including edges not being split.
/// @return One plan per input edge, preserving input order. Uncontested legacy IDs remain unchanged.
/// Collision replacements are deterministic in edge-ID order and cannot consume another plan's legacy ID.
/// @throws std::invalid_argument for malformed IDs, duplicate edges or absent/self endpoint references.
/// @throws std::overflow_error if the deterministic fallback namespace is exhausted.
/// @example planInterpolationIdentities({{edgeId, fromId, toId}}, {fromId, toId}, {edgeId});
std::vector<InterpolatedEdgeIdentities> planInterpolationIdentities(
    const std::vector<InterpolationEdge>& edges,
    const std::set<std::string>& existingNodeIds,
    const std::set<std::string>& existingEdgeIds);
}
#endif
