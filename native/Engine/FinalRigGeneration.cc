// SPDX-License-Identifier: MIT
#include "FinalRigGeneration.h"
#include "EngineInternal.h"
#include "RigTransformValidation.h"
#include <algorithm>
#include <cmath>
#include <dust3d/base/retained_payload_budget.h>
#include <dust3d/rig/rig_generator.h>
#include <mutex>
namespace dust3d {
namespace native {
    namespace {
        void validateRestPair(const RigRestTransforms& pair)
        {
            for (size_t column = 0; column < 4; ++column)
                for (size_t row = 0; row < 4; ++row) {
                    double forward = 0, reverse = 0;
                    for (size_t k = 0; k < 4; ++k) {
                        forward += pair.world[k * 4 + row] * pair.inverseBind[column * 4 + k];
                        reverse += pair.inverseBind[k * 4 + row] * pair.world[column * 4 + k];
                    }
                    const double expected = column == row ? 1 : 0;
                    if (std::abs(forward - expected) > 1e-6 || std::abs(reverse - expected) > 1e-6)
                        throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "Final rig rest/inverse-bind matrices do not compose to identity.");
                }
        }
        void cancellation(const RigFinalDataOptions& options)
        {
            if (options.cancellation && options.cancellation->load(std::memory_order_relaxed))
                throw RigFinalDataError(RigFinalDataFailure::cancelled, "Final rig generation was cancelled.");
        }
    }
    FinalRigGeneration generateFinalRig(const GeneratedRigInput& input, const RigStructure& templateRig, const RigFinalDataOptions& options)
    {
        cancellation(options);
        if (!options.maximumPayloadBytes || options.maximumPayloadBytes > 256 * 1024 * 1024)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Final rig output quota must be in 1..256 MiB.");
        if (templateRig.bones.empty() || input.object().vertices.empty())
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Final rig generation requires template bones and mesh vertices.");
        // Validate hierarchy before the inherited topological fallback can tolerate malformed data.
        (void)copyFinalRigData(templateRig, Object {}, options);
        if (input.object().vertices.size() > options.maximumVertices)
            throw RigFinalDataError(RigFinalDataFailure::limitExceeded, "Final rig source exceeds the vertex count limit.");
        RetainedPayloadBudget initial(options.maximumPayloadBytes);
        initial.accountObject(input.object());
        if (initial.isExceeded())
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Final rig source Object exceeds its output quota.");
        auto object = std::make_unique<Object>(input.object());
        RigStructure actual;
        bool eyelidsGenerated = false;
        RigGenerator generator;
        {
            std::lock_guard<std::mutex> lock(engineMutex());
            cancellation(options);
            if (!generator.generateRig(&input.snapshot(), templateRig, actual))
                throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, generator.getErrorMessage());
            cancellation(options);
            if (!generator.applyRigBindings(object.get(), &input.snapshot(), &actual))
                throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, generator.getErrorMessage());
            cancellation(options);
            if (actual.headHasEyelids)
                eyelidsGenerated = generator.generateEyelidBones(object.get(), &input.snapshot(), actual);
        }
        cancellation(options);
        RetainedPayloadBudget retained(options.maximumPayloadBytes);
        retained.accountObject(*object);
        if (!retained.tryAdd(actual.bones.size(), sizeof(RigRestTransforms)) || retained.usedBytes() == options.maximumPayloadBytes)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Final rig Object exceeds its aggregate output quota.");
        auto copyOptions = options;
        copyOptions.maximumPayloadBytes -= retained.usedBytes();
        auto data = copyFinalRigData(actual, *object, copyOptions);
        std::vector<RigRestTransforms> rest(data.joints.size());
        {
            std::lock_guard<std::mutex> lock(engineMutex());
            cancellation(options);
            std::map<std::string, Matrix4x4> world;
            std::map<std::string, Matrix4x4> inverse;
            if (!generator.computeBoneWorldTransforms(actual, world))
                throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, generator.getErrorMessage());
            // Validate generated world frames before asking the inherited inverse routine
            // to invert them; its singular-matrix fallback is not a valid bind contract.
            for (const auto& entry : world) {
                RigMatrix copied;
                std::copy_n(entry.second.constData(), copied.size(), copied.begin());
                validateRigTransform(copied, RigTransformUse::generatedWorld);
            }
            cancellation(options);
            if (!generator.computeBoneInverseBindMatrices(actual, inverse))
                throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, generator.getErrorMessage());
            if (world.size() != data.joints.size() || inverse.size() != data.joints.size())
                throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "Final rig rest matrix counts differ from joints.");
            for (size_t index = 0; index < data.joints.size(); ++index) {
                cancellation(options);
                const auto foundWorld = world.find(data.joints[index].name);
                const auto foundInverse = inverse.find(data.joints[index].name);
                if (foundWorld == world.end() || foundInverse == inverse.end())
                    throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "Final rig rest matrices omit a joint.");
                std::copy_n(foundWorld->second.constData(), rest[index].world.size(), rest[index].world.begin());
                std::copy_n(foundInverse->second.constData(), rest[index].inverseBind.size(), rest[index].inverseBind.begin());
                validateRigTransform(rest[index].inverseBind, RigTransformUse::generatedWorld);
                validateRestPair(rest[index]);
            }
        }
        const auto bytes = retained.usedBytes() + data.logicalBytes;
        cancellation(options);
        return { std::move(object), std::move(data), std::move(rest), eyelidsGenerated, bytes };
    }
}
}
