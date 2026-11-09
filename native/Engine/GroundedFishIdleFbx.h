// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_GROUNDED_FISH_IDLE_FBX_H_
#define DUST3D_NATIVE_GROUNDED_FISH_IDLE_FBX_H_

#include "FbxMeshScene.h"
#include "FinalRigGeneration.h"
#include "include/Dust3DBridge.h"

namespace dust3d::native {
    struct FishIdleFbxClipInput {
        std::string name;
        AnimationParams parameters;
    };

    struct FishIdleFbxBakeInput {
        const Snapshot& authored;
        Dust3DBakeOptions options;
    };

    /// Source-only route for authored FishIdle clips. The final grounded original
    /// Object, skin bindings and animation keys must come from one generated rig.
    /// The logical quota includes retained key staging and output bytes, not
    /// the writer's transient node tree or evaluator workspace.
    OwnedFbxDocument encodeGroundedFishIdleFbx(const GeneratedRigInput&, const RigStructure&,
        const std::vector<FishIdleFbxClipInput>&, const FbxSceneMaterial*, const RigFinalDataOptions&,
        FbxDocumentLimits = {}, const FishIdleFbxBakeInput* = nullptr);
}
#endif
