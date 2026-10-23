// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_SOURCE_BAKED_MESH_H_
#define DUST3D_NATIVE_SOURCE_BAKED_MESH_H_
#include "BakedMesh.h"
#include "ImportedSurface.h"
#include <dust3d/uv/source_texture_baker.h>
namespace dust3d {
namespace native {
    class SourceBakedData {
    public:
        SourceBakedData(const Dust3DSourceBakeOptions& options, ImportedModels& imports, size_t maximumMipLevels = 1);
        void validateVisuals(const Snapshot& snapshot) const;
        void appendTriangle(size_t index) { m_originalIndices.push_back(index); }
        void prepare(const Object& object, const Snapshot& snapshot, Dust3DMesh& geometry, const ImportedSurfaceData& sources);
        const Dust3DCancellationToken* cancellation() const { return m_options.legacy.cancellation; }
        size_t maximumOutputBytes() const { return m_maximumOutputBytes; }
        std::array<std::optional<TextureImage>, 5> maps;
        std::array<std::vector<TextureImage>, 5> mipLevels;
        std::vector<Dust3DSourceBakedTriangle> triangles;
        size_t payloadBytes = 0;
        double maximumSamplingLod = 0;

    private:
        Dust3DSourceBakeOptions m_options;
        size_t m_maximumOutputBytes;
        size_t m_maximumMipLevels;
        ImportedModels* m_imports;
        std::unique_ptr<BakedMeshData> m_legacy;
        std::map<std::string, TextureImageView> m_modelImages;
        std::vector<size_t> m_originalIndices;
    };
}
}
struct Dust3DSourceBakedMesh {
    std::unique_ptr<Dust3DImportedSurfaceMesh> source;
    std::unique_ptr<dust3d::native::SourceBakedData> data;
};
#endif
