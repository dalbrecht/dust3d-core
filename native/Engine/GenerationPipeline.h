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
    class SourceBakedData;
    class FbxGeometrySelection;
    class OwnedObjDocument;
    enum class GenerationPurpose { preview,
        originalExport };
    /// Named borrowed output hooks. All pointees outlive this synchronous generation call.
    struct GenerationOutputs {
        SourceProvenance* sources = nullptr;
        UvMetadata* uvs = nullptr;
        BakedMeshData* baked = nullptr;
        ImportedSurfaceData* surfaces = nullptr;
        SourceBakedData* sourceBaked = nullptr;
        const std::map<Uuid, UvAtlasImage>* images = nullptr;
        size_t textureSize = 4096;
        /// Optional original double-geometry output, including faces omitted only by float preview collapse.
        /// Replaced only on complete success; caller owns the result independently of generator/preview.
        /// No colors, rig or document metadata. Requires serialized engine access like generateMesh.
        std::unique_ptr<FbxGeometrySelection>* fbxGeometry = nullptr;
        /// Logical export geometry quota; zero selects64MiB, maximum256MiB. Ignored without output.
        size_t maximumFbxGeometryBytes = 0;
        /// Replace local UVs with the complete original-face atlas from `uvs`.
        /// Requires both fbxGeometry and uvs; unknown atlas mappings fail explicitly.
        bool useFbxAtlasUvs = false;
        /// Optional whole-generation token, borrowed throughout this serialized call.
        const Dust3DCancellationToken* cancellation = nullptr;
        /// Optional original polygon OBJ, published only after every requested output succeeds.
        std::unique_ptr<OwnedObjDocument>* objDocument = nullptr;
        /// OBJ root/capacity quota; zero64MiB, maximum256MiB. Ignored without objDocument.
        size_t maximumObjPayloadBytes = 0;
        /// Original export skips float conversion and render descriptors. Requires an export
        /// owner and rejects source/surface/source-bake outputs. UVs/bake prepare export maps only.
        GenerationPurpose purpose = GenerationPurpose::preview;
    };
    Color sourceCornerColor(const Object& object, size_t triangleIndex, size_t corner);
    /// Prepare a healthy owned Object without invoking inherited mesh generation.
    /// Source stays immutable/alive. generated is required for UV/bake preparation.
    /// mesh must have empty preview storage; existing node/part/warning metadata is retained.
    /// Preview mode keeps float validation/filtering; originalExport leaves preview storage unallocated.
    /// All requested export owners publish only after full success/cancellation checks.
    /// Throws typed configuration/geometry/quota/cancellation errors; allocation/codec errors
    /// propagate. Same serialized caller/lifetime requirements as generateMesh.
    void prepareGeneratedObject(const Object& object, const Snapshot* generated, Dust3DMesh& mesh, GenerationOutputs outputs);
    // Every native entry point shares this lock because inherited globals are not thread-safe.
    std::mutex& engineMutex();
    std::unique_ptr<Dust3DMesh> generateMesh(std::unique_ptr<Snapshot> snapshot, const std::string& warning, ImportedModels* imports = nullptr,
        MeshGenerator::GeneratedCacheContext* cache = nullptr, MeshGenerator::GenerationMetrics* metrics = nullptr, GenerationOutputs outputs = {});
}
}
#endif
