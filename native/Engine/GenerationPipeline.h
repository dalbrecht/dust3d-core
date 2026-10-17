#ifndef DUST3D_NATIVE_GENERATION_PIPELINE_H_
#define DUST3D_NATIVE_GENERATION_PIPELINE_H_

#include "EngineInternal.h"
#include <dust3d/mesh/mesh_generator.h>
#include <mutex>

namespace dust3d {
namespace native {
    class SourceProvenance;
    // Every native entry point shares this lock because inherited globals are not thread-safe.
    std::mutex& engineMutex();
    std::unique_ptr<Dust3DMesh> generateMesh(std::unique_ptr<Snapshot> snapshot, const std::string& warning,
        MeshGenerator::GeneratedCacheContext* cache = nullptr, MeshGenerator::GenerationMetrics* metrics = nullptr, SourceProvenance* sources = nullptr);
}
}
#endif
