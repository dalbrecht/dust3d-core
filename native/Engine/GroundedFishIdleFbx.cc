// SPDX-License-Identifier: MIT
#include "GroundedFishIdleFbx.h"
#include "BakedMesh.h"
#include "EngineInternal.h"
#include "FbxBakedMaterial.h"
#include "FbxGeometrySelection.h"
#include "GenerationPipeline.h"
#include "FishIdleEvaluation.h"
#include <algorithm>
#include <dust3d/base/retained_payload_budget.h>

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

        struct PreparedFishIdleMaterial {
            std::unique_ptr<FbxGeometrySelection> geometry;
            FbxSceneMaterial material;
        };

        PreparedFishIdleMaterial prepareFishIdleMaterial(const Object& grounded, const Snapshot& generated,
            const FishIdleFbxBakeInput& input, size_t& remainingBytes)
        {
            auto available = remainingBytes;
            // Reserve both concurrent preparation budgets before entering the bake.
            charge(available, input.options.maximum_uv_payload_bytes);
            charge(available, input.options.maximum_bake_payload_bytes);
            if (!available)
                throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Animated FBX has no atlas geometry budget.");
            BakedMeshData bake(input.options);
            bake.validateVisuals(input.authored);
            UvMetadata uv(input.authored, bake.uvBudget());
            Dust3DMesh receipt;
            PreparedFishIdleMaterial prepared;
            GenerationOutputs outputs;
            outputs.purpose = GenerationPurpose::originalExport;
            outputs.uvs = &uv;
            outputs.baked = &bake;
            outputs.images = &bake.images();
            outputs.textureSize = bake.textureSize();
            outputs.fbxGeometry = &prepared.geometry;
            outputs.maximumFbxGeometryBytes = available;
            outputs.useFbxAtlasUvs = true;
            outputs.cancellation = bake.cancellation();
            {
                std::lock_guard<std::mutex> lock(engineMutex());
                prepareGeneratedObject(grounded, &generated, receipt, outputs);
            }

            const auto& geometry = prepared.geometry->object();
            const auto samePosition = [](const Vector3& first, const Vector3& second) {
                return first.x() == second.x() && first.y() == second.y() && first.z() == second.z();
            };
            if (!std::equal(geometry.vertices.begin(), geometry.vertices.end(),
                    grounded.vertices.begin(), grounded.vertices.end(), samePosition)
                || geometry.triangles != grounded.triangles)
                throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "Animated FBX atlas changed skin control-point order.");
            charge(available, prepared.geometry->payloadBytes());
            if (available < 57)
                throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Animated FBX has no material PNG budget.");
            FbxBakedImageViews images {};
            for (size_t channel = 0; channel < bake.maps.size(); ++channel)
                images[channel] = bake.maps[channel] ? &*bake.maps[channel] : nullptr;
            prepared.material = makeFbxBakedMaterial(images, std::min(available, size_t { 64 * 1024 * 1024 }));

            RetainedPayloadBudget retained(remainingBytes);
            retained.tryAdd(prepared.geometry->payloadBytes());
            for (const auto& map : prepared.material.pngMaps)
                retained.accountDynamic(map);
            if (retained.isExceeded())
                throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Animated FBX atlas and PNGs exceed their retained quota.");
            charge(remainingBytes, retained.usedBytes());
            return prepared;
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
        const RigFinalDataOptions& options, FbxDocumentLimits limits, const FishIdleFbxBakeInput* bake)
    {
        checkExportCancellation(options.cancellation);
        if (templateRig.type != "Fish" || requests.empty())
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Animated FishIdle FBX requires a Fish rig and authored clips.");
        if (requests.size() > 128 || limits.maximumPayloadBytes > 256 * 1024 * 1024
            || (limits.maximumPayloadBytes && limits.maximumPayloadBytes <= sizeof(OwnedFbxDocument))
            || !limits.maximumNodes || limits.maximumNodes > 65536)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Animated FishIdle FBX exceeds its clip or writer quota.");
        if (bake && (material || !bake->options.maximum_uv_payload_bytes || !bake->options.maximum_bake_payload_bytes
                        || (bake->options.cancellation ? &bake->options.cancellation->cancelled : nullptr) != options.cancellation))
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Animated FBX requires explicit bake budgets and a matching cancellation token.");
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
        PreparedFishIdleMaterial prepared;
        if (bake)
            prepared = prepareFishIdleMaterial(*final.object, input.snapshot(), *bake, remainingBytes);
        if (!remainingBytes)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Animated FishIdle FBX has no remaining output budget.");
        const auto writerLimit = limits.maximumPayloadBytes ? limits.maximumPayloadBytes : size_t { 64 * 1024 * 1024 };
        limits.maximumPayloadBytes = std::min(writerLimit, remainingBytes);
        const auto& geometry = prepared.geometry ? prepared.geometry->object() : *final.object;
        auto output = encodeFbxRestMeshScene(geometry, bake ? &prepared.material : material, groundedRig, bindings, limits, clips);
        checkExportCancellation(options.cancellation);
        return output;
    }
}
