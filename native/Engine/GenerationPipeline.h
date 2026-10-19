#ifndef DUST3D_NATIVE_GENERATION_PIPELINE_H_
#define DUST3D_NATIVE_GENERATION_PIPELINE_H_

#include "EngineInternal.h"
#include <dust3d/mesh/mesh_generator.h>
#include <dust3d/uv/uv_atlas_builder.h>
#include <mutex>

namespace dust3d {
namespace native {
    class SourceProvenance;
    class UvMetadata;
    class BakedMeshData;
    class ImportedSurfaceData;
    /// Named borrowed output hooks. All pointees outlive this synchronous generation call.
    struct GenerationOutputs {
        SourceProvenance* sources = nullptr;
        UvMetadata* uvs = nullptr;
        BakedMeshData* baked = nullptr;
        ImportedSurfaceData* surfaces = nullptr;
        const std::map<Uuid, UvAtlasImage>* images = nullptr;
        size_t textureSize = 4096;
    };
    Color sourceCornerColor(const Object& object, size_t triangleIndex, size_t corner);
    // Every native entry point shares this lock because inherited globals are not thread-safe.
    std::mutex& engineMutex();
    std::unique_ptr<Dust3DMesh> generateMesh(std::unique_ptr<Snapshot> snapshot, const std::string& warning, ImportedModels* imports = nullptr,
        MeshGenerator::GeneratedCacheContext* cache = nullptr, MeshGenerator::GenerationMetrics* metrics = nullptr, GenerationOutputs outputs = {});
}
}
#endif
