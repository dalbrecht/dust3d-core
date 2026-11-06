// SPDX-License-Identifier: MIT
#include "GeneratedRigInput.h"
#include "EngineInternal.h"
#include <dust3d/base/retained_payload_budget.h>
namespace dust3d {
namespace native {
    std::unique_ptr<GeneratedRigInput> retainGeneratedRigInput(std::unique_ptr<Snapshot> snapshot, std::unique_ptr<Object> object, size_t maximumBytes)
    {
        if (!snapshot || !object)
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Generated rig input requires both processed snapshot and Object.");
        if (maximumBytes > 256 * 1024 * 1024)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Generated rig input quota exceeds 256 MiB.");
        RetainedPayloadBudget budget(maximumBytes ? maximumBytes : 64 * 1024 * 1024);
        budget.accountObject(*snapshot);
        budget.accountObject(*object);
        if (budget.isExceeded())
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Generated rig input exceeds its retained payload quota.");
        return std::unique_ptr<GeneratedRigInput>(new GeneratedRigInput(std::move(snapshot), std::move(object), budget.usedBytes()));
    }
}
}
