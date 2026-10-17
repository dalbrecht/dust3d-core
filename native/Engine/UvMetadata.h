// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_UV_METADATA_H_
#define DUST3D_NATIVE_UV_METADATA_H_

#include "EngineInternal.h"
#include <dust3d/uv/uv_atlas_builder.h>

namespace dust3d {
namespace native {
    Color normalizedUvColor(const std::string& text);
    void validateUvVisuals(const Snapshot& snapshot, const std::map<Uuid, UvAtlasImage>& images);
    // Original identity membership is captured before generator preprocessing.
    class UvMetadata {
    public:
        UvMetadata(const Snapshot& original, size_t maximumBytes);
        void prepare(const Object& object, const Snapshot& generated, const std::map<Uuid, UvAtlasImage>& images, size_t textureSize);
        void appendTriangle(size_t originalIndex);
        const std::vector<Dust3DUvTriangle>& triangles() const { return m_triangles; }
        const std::vector<Dust3DUvChart>& charts() const { return m_charts; }
        const std::vector<Dust3DUvGradient>& gradients() const { return m_gradients; }
        const std::vector<std::string>& components() const { return m_components; }
        const UvAtlas& atlas() const { return m_atlas; }
        size_t payloadBytes() const { return m_payloadBytes; }
        double textureSize() const { return m_atlas.packedTextureSize; }

    private:
        void charge(size_t count, size_t width = 1);
        std::pair<Dust3DUvComponentKind, size_t> resolve(const Uuid& identity) const;
        size_t m_maximumBytes;
        size_t m_payloadBytes = 0;
        std::vector<std::string> m_components;
        std::map<Uuid, size_t> m_componentIndices;
        UvAtlas m_atlas;
        std::vector<Dust3DUvTriangle> m_triangles;
        std::vector<Dust3DUvChart> m_charts;
        std::vector<Dust3DUvGradient> m_gradients;
    };
}
}

struct Dust3DUvMesh {
    std::unique_ptr<Dust3DMesh> geometry;
    std::unique_ptr<dust3d::native::UvMetadata> metadata;
};
#endif
