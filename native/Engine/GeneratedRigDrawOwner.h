// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_GENERATED_RIG_DRAW_OWNER_H_
#define DUST3D_NATIVE_GENERATED_RIG_DRAW_OWNER_H_

#include "EngineInternal.h"
#include "FinalRigGeneration.h"

struct Dust3DGeneratedRig {
    std::unique_ptr<Dust3DMesh> mesh;
    std::vector<Dust3DGeneratedRigJoint> joints;
    std::vector<std::string> names;
    std::vector<Dust3DGeneratedRigWeights> weights;
    size_t outputBytes = 0;
    bool eyelidsGenerated = false;
};

namespace dust3d::native {
    /// Expand the exact final grounded Object and its aligned original weights.
    /// Acquires engineMutex internally. Metadata must be the fresh rig-input generation receipt.
    std::unique_ptr<Dust3DGeneratedRig> makeGeneratedRigDrawOwner(const FinalRigGeneration&,
        std::unique_ptr<Dust3DMesh> metadata, size_t maximumDrawVertices, size_t maximumOutputBytes,
        const Dust3DCancellationToken*);
}
#endif
