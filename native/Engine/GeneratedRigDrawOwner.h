// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_GENERATED_RIG_DRAW_OWNER_H_
#define DUST3D_NATIVE_GENERATED_RIG_DRAW_OWNER_H_

#include "EngineInternal.h"
#include "FinalRigGeneration.h"
#include "ImportedModels.h"
#include "SourceBakedMesh.h"
#include <array>
#include <dust3d/rig/rig_generator.h>

struct Dust3DGeneratedRig {
    std::unique_ptr<Dust3DMesh> mesh;
    std::vector<Dust3DGeneratedRigJoint> joints;
    std::vector<std::string> names;
    std::vector<Dust3DGeneratedRigWeights> weights;
    std::unique_ptr<dust3d::native::ImportedSurfaceData> sourceSurfaces;
    std::unique_ptr<dust3d::native::SourceBakedData> sourceBake;
    size_t outputBytes = 0;
    bool eyelidsGenerated = false;
};

namespace dust3d::native {
    /// Keep the import material owners alive after the generator takes their geometry.
    struct GeneratedRigDrawBakeInput {
        ImportedModels imports;
        std::unique_ptr<ImportedSurfaceData> surfaces;
        std::unique_ptr<SourceBakedData> atlas;
        std::array<size_t, 5> payloadLimits {};
    };

    /// The template and both outputs from a single validated rig-input-only generation.
    struct PreparedGeneratedRigSource {
        RigStructure rig;
        RigFinalDataOptions options;
        std::unique_ptr<GeneratedRigInput> input;
        std::unique_ptr<Dust3DMesh> metadata;
        std::unique_ptr<GeneratedRigDrawBakeInput> bake;
    };
    enum class GeneratedRigImportMode { proceduralOnly, allowImportedGeometry };
    PreparedGeneratedRigSource prepareGeneratedRigSource(const uint8_t*, size_t, const char*, size_t,
        const Dust3DGeneratedRigBoneInput*, size_t, const Dust3DGeneratedRigOptions&,
        const Dust3DSourceBakeOptions* bakeOptions = nullptr,
        GeneratedRigImportMode imports = GeneratedRigImportMode::proceduralOnly);

    /// Expand the exact final grounded Object and its aligned original weights.
    /// Acquires engineMutex internally. Metadata must be the fresh rig-input generation receipt.
    std::unique_ptr<Dust3DGeneratedRig> makeGeneratedRigDrawOwner(const FinalRigGeneration&,
        std::unique_ptr<Dust3DMesh> metadata, size_t maximumDrawVertices, size_t maximumOutputBytes,
        const Dust3DCancellationToken*, const Snapshot* generated = nullptr,
        GeneratedRigDrawBakeInput* bake = nullptr);
}
#endif
