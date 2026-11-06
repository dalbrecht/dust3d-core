// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_FINAL_RIG_GENERATION_H_
#define DUST3D_NATIVE_FINAL_RIG_GENERATION_H_
#include "GeneratedRigInput.h"
#include "RigFinalData.h"
namespace dust3d {
namespace native {
    struct FinalRigGeneration {
        std::unique_ptr<Object> object;
        RigFinalData data;
        bool eyelidsGenerated = false;
        size_t payloadBytes = 0;
    };
    /// Generate actual positions, bind/ground a private Object copy, optionally add eyelids,
    /// and validate/copy final joint/four-weight values. Same generator instance for all stages.
    /// Internally serializes engine calls; do not call while holding engineMutex().
    /// Input must be a validated coherent generator-produced pair with finite quantizable
    /// geometry and bounded graph/mirror depth/work. This is NOT an untrusted C boundary.
    /// Caller inputs remain unchanged; result publishes only after complete success.
    /// Optional eyelid absence leaves the valid base rig and reports eyelidsGenerated=false.
    /// Options limit final Object capacities plus data logical bytes (max256MiB), excluding
    /// input, snapshot borrowing, generation/copy workspace and allocator overhead.
    /// Cooperative cancellation is checked between inherited stages, not inside their loops.
    /// Throws DocumentError for generation/quota/configuration, RigFinalDataError for copied
    /// value validation/cancellation; parser/allocation exceptions propagate for future C handling.
    /// No rest matrices, pose/animation, UV materialization or draw expansion is performed.
    FinalRigGeneration generateFinalRig(const GeneratedRigInput&, const RigStructure&, const RigFinalDataOptions& = {});
}
}
#endif
