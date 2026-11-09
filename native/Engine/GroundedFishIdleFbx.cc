// SPDX-License-Identifier: MIT
#include "GroundedFishIdleFbx.h"
#include "EngineInternal.h"
#include "FishIdleEvaluation.h"
#include <algorithm>

namespace dust3d::native {
    namespace {
        void checkExportCancellation(const std::atomic_bool* cancellation)
        {
            if (cancellation && cancellation->load(std::memory_order_relaxed))
                throw DocumentError(DUST3D_STATUS_CANCELLED, "Grounded FishIdle FBX export was cancelled.");
        }

        void charge(size_t& remaining, size_t count, size_t stride = 1)
        {
            if (count > remaining / stride)
                throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Grounded FishIdle FBX exceeds its retained quota.");
            remaining -= count * stride;
        }

        size_t chargeGroundedRig(const RigStructure& rig, size_t remaining)
        {
            charge(remaining, rig.type.size());
            charge(remaining, rig.description.size());
            charge(remaining, rig.bones.size(), sizeof(RigNode));
            for (const auto& joint : rig.bones) {
                charge(remaining, joint.name.size());
                charge(remaining, joint.parent.size());
            }
            return remaining;
        }

        size_t stageClip(const RigAnimationClip& evaluated, const std::string& name, size_t remaining,
            const std::atomic_bool* cancellation, std::vector<RigAnimationClip>& clips)
        {
            auto available = remaining;
            charge(available, name.size());
            charge(available, evaluated.animationType.size());
            charge(available, evaluated.frames.size(), sizeof(BoneAnimationFrame));
            for (const auto& frame : evaluated.frames) {
                checkExportCancellation(cancellation);
                charge(available, frame.boneWorldTransforms.size(),
                    sizeof(decltype(frame.boneWorldTransforms)::value_type));
                for (const auto& [joint, matrix] : frame.boneWorldTransforms) {
                    (void)matrix;
                    charge(available, joint.size());
                }
            }

            RigAnimationClip staged;
            staged.name = name;
            staged.animationType = evaluated.animationType;
            staged.durationSeconds = evaluated.durationSeconds;
            staged.loop = evaluated.loop;
            staged.frames.reserve(evaluated.frames.size());
            for (const auto& frame : evaluated.frames) {
                checkExportCancellation(cancellation);
                staged.frames.push_back({ frame.time, frame.boneWorldTransforms, {} });
            }
            clips.push_back(std::move(staged));
            return remaining - available;
        }
    }

    OwnedFbxDocument encodeGroundedFishIdleFbx(const GeneratedRigInput& input, const RigStructure& templateRig,
        const std::vector<FishIdleFbxClipInput>& requests, const FbxSceneMaterial* material,
        const RigFinalDataOptions& options, FbxDocumentLimits limits)
    {
        checkExportCancellation(options.cancellation);
        if (templateRig.type != "Fish" || requests.empty())
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Animated FishIdle FBX requires a Fish rig and authored clips.");
        if (requests.size() > 128 || limits.maximumPayloadBytes > 256 * 1024 * 1024
            || (limits.maximumPayloadBytes && limits.maximumPayloadBytes <= sizeof(OwnedFbxDocument))
            || !limits.maximumNodes || limits.maximumNodes > 65536)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Animated FishIdle FBX exceeds its clip or writer quota.");
        for (const auto& request : requests) {
            if (request.name.empty() || request.name.size() > 1024 || request.name.find('\0') != std::string::npos)
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Animated FishIdle FBX requires bounded clip names.");
        }

        RigStructure groundedRig;
        std::vector<RigAnimationClip> clips;
        size_t remainingBytes = 0;
        auto final = generateFinalRigWithGroundedHandoff(input, templateRig, options,
            [&](const RigStructure& grounded, const std::map<std::string, Matrix4x4>& inverse, size_t available) {
                remainingBytes = chargeGroundedRig(grounded, available);
                charge(remainingBytes, requests.size(), sizeof(RigAnimationClip));
                groundedRig = grounded;
                clips.reserve(requests.size());
                size_t remainingJointKeys = 1000000;
                for (const auto& request : requests) {
                    checkExportCancellation(options.cancellation);
                    size_t stagedBytes = 0;
                    (void)evaluateFishIdle(grounded, inverse, request.parameters, remainingBytes, options.cancellation,
                        [&](const RigAnimationClip& evaluated, size_t availableForKeys) {
                            if (evaluated.frames.size() > remainingJointKeys / grounded.bones.size())
                                throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Animated FishIdle FBX exceeds its joint-key quota.");
                            remainingJointKeys -= evaluated.frames.size() * grounded.bones.size();
                            stagedBytes = stageClip(evaluated, request.name, availableForKeys, options.cancellation, clips);
                            return stagedBytes;
                        });
                    charge(remainingBytes, stagedBytes);
                }
            });

        checkExportCancellation(options.cancellation);
        charge(remainingBytes, final.data.vertices.size(), sizeof(GlbSkinWeights));
        std::vector<GlbSkinWeights> bindings;
        bindings.reserve(final.data.vertices.size());
        for (const auto& source : final.data.vertices) {
            checkExportCancellation(options.cancellation);
            GlbSkinWeights binding;
            binding.weights = source.weights;
            for (size_t slot = 0; slot < source.weights.size(); ++slot) {
                if (source.weights[slot] == 0)
                    continue;
                if (source.joints[slot] >= groundedRig.bones.size() || source.joints[slot] > UINT16_MAX)
                    throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "Animated FBX lost an admitted skin joint.");
                binding.joints[slot] = static_cast<uint16_t>(source.joints[slot]);
            }
            bindings.push_back(binding);
        }
        if (!remainingBytes)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Animated FishIdle FBX has no remaining output budget.");
        const auto writerLimit = limits.maximumPayloadBytes ? limits.maximumPayloadBytes : size_t { 64 * 1024 * 1024 };
        limits.maximumPayloadBytes = std::min(writerLimit, remainingBytes);
        auto output = encodeFbxRestMeshScene(*final.object, material, groundedRig, bindings, limits, clips);
        checkExportCancellation(options.cancellation);
        return output;
    }
}
