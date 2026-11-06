// SPDX-License-Identifier: MIT
#include "RigFinalData.h"
#include "WorkerUtf8.h"
#include <cmath>
#include <dust3d/rig/rig_generator.h>
#include <map>
namespace dust3d {
namespace native {
    RigFinalDataError::RigFinalDataError(RigFinalDataFailure failure, const char* cause)
        : std::runtime_error(cause)
        , m_failure(failure)
    {
    }
    namespace {
        [[noreturn]] void fail(RigFinalDataFailure failure, const char* cause) { throw RigFinalDataError(failure, cause); }
        void cancelled(const RigFinalDataOptions& options)
        {
            if (options.cancellation && options.cancellation->load(std::memory_order_relaxed))
                fail(RigFinalDataFailure::cancelled, "Final rig data copy was cancelled.");
        }
        void text(const std::string& value, bool emptyAllowed)
        {
            if (value.size() > 1024)
                fail(RigFinalDataFailure::limitExceeded, "Final rig name exceeds 1024 bytes.");
            if ((!emptyAllowed && value.empty()) || !worker::detail::isIdentityUtf8(value))
                fail(RigFinalDataFailure::invalidData, "Final rig name is not non-NUL UTF8.");
            for (unsigned char byte : value)
                if (byte < 32 || byte == 127)
                    fail(RigFinalDataFailure::invalidData, "Final rig name contains an ASCII control.");
        }
        void coordinate(double value)
        {
            if (!std::isfinite(value))
                fail(RigFinalDataFailure::invalidData, "Final rig coordinate is not finite.");
            if (std::abs(value) > 1000000)
                fail(RigFinalDataFailure::limitExceeded, "Final rig coordinate exceeds its bound.");
        }
        void charge(size_t& bytes, size_t count, size_t unit, size_t maximum)
        {
            if (bytes > maximum || count > (maximum - bytes) / unit)
                fail(RigFinalDataFailure::limitExceeded, "Final rig retained output exceeds its quota.");
            bytes += count * unit;
        }
    }
    RigFinalData copyFinalRigData(const RigStructure& rig, const Object& object, const RigFinalDataOptions& options)
    {
        if (!options.maximumBones || options.maximumBones > 256 || !options.maximumVertices || options.maximumVertices > 1000000 || !options.maximumPayloadBytes)
            fail(RigFinalDataFailure::invalidData, "Final rig copy limits are invalid.");
        cancelled(options);
        if (rig.bones.size() > options.maximumBones || object.vertices.size() > options.maximumVertices)
            fail(RigFinalDataFailure::limitExceeded, "Final rig record count exceeds its limit.");
        size_t bytes = 0;
        charge(bytes, rig.bones.size(), sizeof(RigFinalJoint), options.maximumPayloadBytes);
        charge(bytes, object.vertices.size(), sizeof(RigFinalVertexWeights), options.maximumPayloadBytes);
        for (const auto& bone : rig.bones) {
            text(bone.name, false);
            text(bone.parent, true);
            charge(bytes, bone.name.size() + 1, 1, options.maximumPayloadBytes);
            for (float value : { bone.posX, bone.posY, bone.posZ, bone.endX, bone.endY, bone.endZ })
                coordinate(value);
            if (!std::isfinite(bone.capsuleRadius) || bone.capsuleRadius < 0 || !std::isfinite(bone.closingAngle))
                fail(RigFinalDataFailure::invalidData, "Final rig radius or closing angle is invalid.");
            coordinate(bone.capsuleRadius);
        }
        using Channel = std::vector<std::pair<std::string, float>>;
        const std::array<const Channel*, 4> channels { &object.vertexBone1, &object.vertexBone2, &object.vertexBone3, &object.vertexBone4 };
        for (const auto* channel : channels)
            if (!channel->empty() && channel->size() != object.vertices.size())
                fail(RigFinalDataFailure::invalidData, "Final rig influence channel has a different vertex count.");
        RigFinalData result;
        result.logicalBytes = bytes;
        result.joints.reserve(rig.bones.size());
        result.vertices.reserve(object.vertices.size());
        std::map<std::string, uint32_t> indices;
        for (size_t index = 0; index < rig.bones.size(); ++index) {
            cancelled(options);
            const auto& bone = rig.bones[index];
            if (!indices.emplace(bone.name, static_cast<uint32_t>(index)).second)
                fail(RigFinalDataFailure::invalidData, "Final rig bone names are duplicated.");
            result.joints.push_back({ bone.name, rigFinalNoJoint,
                { bone.posX, bone.posY, bone.posZ }, { bone.endX, bone.endY, bone.endZ }, bone.capsuleRadius, bone.closingAngle });
        }
        for (size_t index = 0; index < rig.bones.size(); ++index) {
            const auto& parent = rig.bones[index].parent;
            if (parent.empty())
                continue;
            const auto found = indices.find(parent);
            if (found == indices.end())
                fail(RigFinalDataFailure::invalidData, "Final rig parent does not resolve.");
            result.joints[index].parentIndex = found->second;
        }
        for (size_t index = 0; index < result.joints.size(); ++index) {
            std::array<bool, 256> visited {};
            auto cursor = static_cast<uint32_t>(index);
            while (cursor != rigFinalNoJoint) {
                if (visited[cursor])
                    fail(RigFinalDataFailure::invalidData, "Final rig parent graph is cyclic.");
                visited[cursor] = true;
                cursor = result.joints[cursor].parentIndex;
            }
        }
        for (size_t index = 0; index < object.vertices.size(); ++index) {
            if (index % 1024 == 0)
                cancelled(options);
            for (size_t axis = 0; axis < 3; ++axis)
                coordinate(object.vertices[index][axis]);
            RigFinalVertexWeights vertex;
            double sum = 0;
            for (size_t slot = 0; slot < channels.size(); ++slot) {
                if (channels[slot]->empty())
                    continue;
                const auto& influence = (*channels[slot])[index];
                const auto weight = influence.second;
                if (!std::isfinite(weight) || weight < 0 || weight > 1)
                    fail(RigFinalDataFailure::invalidData, "Final rig influence weight is invalid.");
                if (weight == 0)
                    continue;
                text(influence.first, false);
                const auto found = indices.find(influence.first);
                if (found == indices.end())
                    fail(RigFinalDataFailure::invalidData, "Final rig positive influence has an unknown joint.");
                vertex.joints[slot] = found->second;
                vertex.weights[slot] = weight;
                sum += weight;
            }
            if (sum > 0 && std::abs(sum - 1) > .0001)
                fail(RigFinalDataFailure::invalidData, "Final rig bound weights are not normalized.");
            result.vertices.push_back(vertex);
        }
        cancelled(options);
        return result;
    }
}
}
