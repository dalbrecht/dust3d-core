// SPDX-License-Identifier: MIT
#include "FinalRigGeneration.h"
#include "EngineInternal.h"
#include <dust3d/base/retained_payload_budget.h>
#include <dust3d/rig/rig_generator.h>
#include <mutex>
namespace dust3d {
namespace native {
    namespace {
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
        {
            std::lock_guard<std::mutex> lock(engineMutex());
            cancellation(options);
            RigGenerator generator;
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
        if (retained.isExceeded() || retained.usedBytes() == options.maximumPayloadBytes)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Final rig Object exceeds its aggregate output quota.");
        auto copyOptions = options;
        copyOptions.maximumPayloadBytes -= retained.usedBytes();
        auto data = copyFinalRigData(actual, *object, copyOptions);
        const auto bytes = retained.usedBytes() + data.logicalBytes;
        cancellation(options);
        return { std::move(object), std::move(data), eyelidsGenerated, bytes };
    }
}
}
