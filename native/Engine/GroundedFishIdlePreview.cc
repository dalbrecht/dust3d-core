// SPDX-License-Identifier: MIT
#include "GroundedFishIdlePreview.h"
#include "Cancellation.h"
#include "RigTransformValidation.h"
#include <optional>

namespace dust3d::native {
    GroundedFishIdlePreview generateGroundedFishIdlePreview(const GeneratedRigInput& input,
        const RigStructure& templateRig, const AnimationParams& parameters, const RigFinalDataOptions& options)
    {
        if (templateRig.type != "Fish")
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "FishIdle preview requires a Fish rig template.");

        std::optional<OwnedEvaluatedSkeletonClip> clip;
        auto rest = generateFinalRigWithGroundedHandoff(input, templateRig, options,
            [&](const RigStructure& grounded, const std::map<std::string, Matrix4x4>& inverse, size_t remainingBytes) {
                auto evaluated = evaluateFishIdle(grounded, inverse, parameters, remainingBytes, options.cancellation);
                for (const auto& matrix : evaluated.modelFromJoint()) {
                    if (options.cancellation && options.cancellation->load(std::memory_order_relaxed))
                        throw DocumentError(DUST3D_STATUS_CANCELLED, "FishIdle preview was cancelled.");
                    validateRigTransform(matrix, RigTransformUse::generatedWorld);
                }
                clip.emplace(std::move(evaluated));
            });
        if (!clip || clip->jointCount() != rest.data.joints.size())
            throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "FishIdle preview lost its grounded joint order.");
        return { std::move(rest), std::move(*clip) };
    }

    GroundedFishIdleDrawPreview generateGroundedFishIdleDrawPreview(const GeneratedRigInput& input,
        std::unique_ptr<Dust3DMesh> metadata, const RigStructure& templateRig,
        const AnimationParams& parameters, const RigFinalDataOptions& options,
        size_t maximumDrawVertices, const Dust3DCancellationToken* token)
    {
        auto sharedOptions = options;
        sharedOptions.cancellation = token ? &token->cancelled : nullptr;
        if (!metadata || !metadata->vertices.empty() || !maximumDrawVertices || maximumDrawVertices > 15000000
            || (options.cancellation && options.cancellation != sharedOptions.cancellation))
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Invalid grounded FishIdle draw inputs.");
        checkCancellation(token);
        auto preview = generateGroundedFishIdlePreview(input, templateRig, parameters, sharedOptions);
        const auto limit = sharedOptions.maximumPayloadBytes;
        const auto poseBytes = preview.clip.logicalPayloadBytes();
        if (preview.rest.payloadBytes > limit || poseBytes > limit - preview.rest.payloadBytes)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Grounded FishIdle preview exceeds its aggregate quota.");
        auto rest = makeGeneratedRigDrawOwner(preview.rest, std::move(metadata), maximumDrawVertices,
            limit - preview.rest.payloadBytes - poseBytes, token);
        checkCancellation(token);
        return { std::move(rest), std::move(preview.clip) };
    }
}
