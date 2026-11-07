// SPDX-License-Identifier: MIT
#include "GroundedFishIdlePreview.h"
#include "EngineInternal.h"
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
}
