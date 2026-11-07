// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_FINAL_RIG_GENERATION_H_
#define DUST3D_NATIVE_FINAL_RIG_GENERATION_H_
#include "GeneratedRigInput.h"
#include "RigFinalData.h"
#include "RigMatrix.h"
#include <functional>
#include <map>
namespace dust3d {
class Matrix4x4;
namespace native {
    struct RigRestTransforms {
        RigMatrix world {};
        RigMatrix inverseBind {};
    };
    struct FinalRigGeneration {
        std::unique_ptr<Object> object;
        RigFinalData data;
        /// Source-joint order; column-major doubles acting on column vectors.
        std::vector<RigRestTransforms> restTransforms;
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
    /// Options limit final Object capacities plus data/rest logical bytes (max256MiB), excluding
    /// input, snapshot borrowing, generation/copy workspace and allocator overhead.
    /// Cooperative cancellation is checked between stages and inside vertex binding.
    /// Other inherited stages/allocations/sorting retain no fixed cancellation latency.
    /// Throws DocumentError for generation/quota/configuration, RigFinalDataError for copied
    /// value validation/cancellation; parser/allocation exceptions propagate for future C handling.
    /// Rest-world/inverse-bind matrices use final grounded/post-eyelid bones and validated rigid frames.
    /// No pose/animation, UV materialization or draw expansion is performed.
    FinalRigGeneration generateFinalRig(const GeneratedRigInput&, const RigStructure&, const RigFinalDataOptions& = {});

    /// Private synchronous handoff of the same grounded rig and validated inverse binds.
    /// Borrows both values under engineMutex; the callback must not reacquire that lock
    /// or publish results. The remaining bytes bound any copied output the callback stages.
    using GroundedRigHandoff = std::function<void(const RigStructure&,
        const std::map<std::string, Matrix4x4>&, size_t remainingBytes)>;
    FinalRigGeneration generateFinalRigWithGroundedHandoff(const GeneratedRigInput&, const RigStructure&,
        const RigFinalDataOptions&, const GroundedRigHandoff&);
}
}
#endif
