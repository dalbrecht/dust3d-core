// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_GLB_SKIN_WEIGHTS_H
#define DUST3D_NATIVE_GLB_SKIN_WEIGHTS_H

#include <cmath>
#include <cstdint>
#include <dust3d/base/object.h>
#include <optional>
#include <stdexcept>

namespace dust3d::native {

/// Assign stable glTF joint indices in the order supplied by the rig.
inline std::map<std::string, uint16_t> glbJointIndices(const std::vector<std::string>& names)
{
    if (names.empty() || names.size() > size_t(UINT16_MAX) + 1)
        throw std::invalid_argument("Invalid GLB joint count");
    std::map<std::string, uint16_t> indices;
    for (size_t index = 0; index < names.size(); ++index) {
        if (names[index].empty() || !indices.emplace(names[index], static_cast<uint16_t>(index)).second)
            throw std::invalid_argument("GLB joints require unique names");
    }
    return indices;
}

/// Resolve parent names in joint order and reject cyclic hierarchies.
inline std::vector<std::optional<uint16_t>> glbJointParents(
    const std::vector<std::string>& parents, const std::map<std::string, uint16_t>& indices)
{
    if (parents.size() != indices.size())
        throw std::invalid_argument("GLB parent table must align with joints");
    std::vector<std::optional<uint16_t>> result;
    for (const auto& name : parents) {
        if (name.empty())
            result.push_back(std::nullopt);
        else {
            const auto parent = indices.find(name);
            if (parent == indices.end() || parent->second >= parents.size())
                throw std::invalid_argument("GLB joint parent is missing");
            result.push_back(parent->second);
        }
    }
    std::vector<uint8_t> state(result.size());
    for (size_t start = 0; start < result.size(); ++start) {
        auto current = std::optional<uint16_t>(static_cast<uint16_t>(start));
        while (current && state[*current] == 0) {
            state[*current] = 1;
            current = result[*current];
        }
        if (current && state[*current] == 1)
            throw std::invalid_argument("GLB joint hierarchy contains a cycle");
        current = static_cast<uint16_t>(start);
        while (current && state[*current] == 1) {
            state[*current] = 2;
            current = result[*current];
        }
    }
    return result;
}

struct GlbSkinWeights {
    std::array<uint16_t, 4> joints {};
    std::array<float, 4> weights {};
};

inline GlbSkinWeights glbSkinWeightsForVertex(
    const Object& object, size_t vertex, const std::map<std::string, uint16_t>& jointIndices)
{
    GlbSkinWeights result;
    const auto slots = { &object.vertexBone1, &object.vertexBone2, &object.vertexBone3, &object.vertexBone4 };
    double total = 0;
    size_t index = 0;
    for (const auto* slot : slots) {
        if (!slot->empty()) {
            const auto& [bone, weight] = slot->at(vertex);
            if (!std::isfinite(weight) || weight < 0)
                throw std::invalid_argument("Invalid GLB skin weight");
            if (weight > 0) {
                const auto joint = jointIndices.find(bone);
                if (joint == jointIndices.end())
                    throw std::invalid_argument("GLB skin references an unknown joint");
                result.joints[index] = joint->second;
                result.weights[index] = weight;
                total += weight;
            }
        }
        ++index;
    }
    if (total <= 0)
        throw std::invalid_argument("GLB vertex has no joint influence");
    for (auto& weight : result.weights)
        weight = static_cast<float>(weight / total);
    return result;
}

}
#endif
