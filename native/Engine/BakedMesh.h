// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_BAKED_MESH_H_
#define DUST3D_NATIVE_BAKED_MESH_H_
#include "Cancellation.h"
#include "UvMetadata.h"
#include <array>
#include <dust3d/uv/texture_baker.h>

namespace dust3d {
namespace native {
    class BakedMeshData {
    public:
        explicit BakedMeshData(const Dust3DBakeOptions& options);
        void validateVisuals(const Snapshot& snapshot) const;
        void appendTriangle(size_t index) { m_originalIndices.push_back(index); }
        void prepare(const Object& object, const Snapshot& snapshot, const UvAtlas& atlas);
        const Dust3DCancellationToken* cancellation() const { return m_options.cancellation; }
        const std::map<Uuid, UvAtlasImage>& images() const { return m_images; }
        size_t textureSize() const { return m_options.atlas_texture_size; }
        size_t uvBudget() const { return m_options.maximum_uv_payload_bytes; }
        std::array<std::optional<TextureImage>, 5> maps;
        std::vector<Dust3DTextureTangent> tangents;
        size_t payloadBytes = 0;

        /// Shared pure input resolution for source-aware composition. Borrowed views stay call-local.
        TextureBakeInputs resolvedInputs(const Snapshot& snapshot) const;
        const Dust3DBakeOptions& options() const { return m_options; }

    private:
        Dust3DBakeOptions m_options;
        std::map<Uuid, UvAtlasImage> m_images;
        std::map<Uuid, TextureImageView> m_pixels;
        std::vector<size_t> m_originalIndices;
    };
}
}
struct Dust3DBakedMesh {
    std::unique_ptr<Dust3DUvMesh> uvs;
    std::unique_ptr<dust3d::native::BakedMeshData> data;
};
#endif
