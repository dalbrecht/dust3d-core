// SPDX-License-Identifier: MIT
#include "GroundedFishIdlePreview.h"
#include "Cancellation.h"
#include "RigTransformValidation.h"
#include <optional>

namespace dust3d::native {
    GroundedFishIdlePreview generateGroundedFishIdlePreview(const GeneratedRigInput& input,
        const RigStructure& templateRig, const AnimationParams& parameters, const RigFinalDataOptions& options,
        const FishIdleClipHandoff& handoff, const std::string& clipType)
    {
        if (evaluatedClipRig(clipType).empty() || evaluatedClipRig(clipType) != templateRig.type)
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Animation preview requires a matching rig template.");

        std::optional<OwnedEvaluatedSkeletonClip> clip;
        size_t derivedPayloadBytes = 0;
        FishIdleClipHandoff stageDerived;
        if (handoff) {
            stageDerived = [&](const RigAnimationClip& evaluated, size_t remainingBytes) {
                derivedPayloadBytes = handoff(evaluated, remainingBytes);
                return derivedPayloadBytes;
            };
        }
        auto rest = generateFinalRigWithGroundedHandoff(input, templateRig, options,
            [&](const RigStructure& grounded, const std::map<std::string, Matrix4x4>& inverse, size_t remainingBytes) {
                auto evaluated = evaluateFishIdle(grounded, inverse, parameters, remainingBytes, options.cancellation, stageDerived, clipType);
                for (const auto& matrix : evaluated.modelFromJoint()) {
                    if (options.cancellation && options.cancellation->load(std::memory_order_relaxed))
                        throw DocumentError(DUST3D_STATUS_CANCELLED, "FishIdle preview was cancelled.");
                    validateRigTransform(matrix, RigTransformUse::generatedWorld);
                }
                clip.emplace(std::move(evaluated));
            });
        if (!clip || clip->jointCount() != rest.data.joints.size())
            throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "FishIdle preview lost its grounded joint order.");
        return { std::move(rest), std::move(*clip), derivedPayloadBytes };
    }

    GroundedFishIdleDrawPreview generateGroundedFishIdleDrawPreview(const GeneratedRigInput& input,
        std::unique_ptr<Dust3DMesh> metadata, const RigStructure& templateRig,
        const AnimationParams& parameters, const RigFinalDataOptions& options,
        size_t maximumDrawVertices, const Dust3DCancellationToken* token, const FishIdleClipHandoff& handoff, const std::string& clipType)
    {
        auto sharedOptions = options;
        sharedOptions.cancellation = token ? &token->cancelled : nullptr;
        if (!metadata || !metadata->vertices.empty() || !maximumDrawVertices || maximumDrawVertices > 15000000
            || (options.cancellation && options.cancellation != sharedOptions.cancellation))
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Invalid grounded FishIdle draw inputs.");
        checkCancellation(token);
        auto preview = generateGroundedFishIdlePreview(input, templateRig, parameters, sharedOptions, handoff, clipType);
        const auto limit = sharedOptions.maximumPayloadBytes;
        const auto poseBytes = preview.clip.logicalPayloadBytes();
        if (preview.rest.payloadBytes > limit || poseBytes > limit - preview.rest.payloadBytes)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Grounded FishIdle preview exceeds its aggregate quota.");
        const auto remaining = limit - preview.rest.payloadBytes - poseBytes;
        if (preview.derivedPayloadBytes > remaining)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Grounded FishIdle derived output exceeds its aggregate quota.");
        auto rest = makeGeneratedRigDrawOwner(preview.rest, std::move(metadata), maximumDrawVertices,
            remaining - preview.derivedPayloadBytes, token);
        checkCancellation(token);
        return { std::move(rest), std::move(preview.clip) };
    }
}
