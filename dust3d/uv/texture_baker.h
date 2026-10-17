// SPDX-License-Identifier: MIT
#ifndef DUST3D_UV_TEXTURE_BAKER_H_
#define DUST3D_UV_TEXTURE_BAKER_H_

#include <atomic>
#include <dust3d/uv/cloth_folds.h>
#include <dust3d/uv/surface_pattern.h>
#include <dust3d/uv/uv_atlas_builder.h>

namespace dust3d {

enum class TextureBakeErrorCode { invalidInput,
    limitExceeded,
    cancelled,
    bakingFailed };

/// Typed bake failure; allocation failures can additionally throw std::bad_alloc.
class TextureBakeError : public std::runtime_error {
public:
    TextureBakeError(TextureBakeErrorCode code, const char* message)
        : std::runtime_error(message)
        , m_code(code)
    {
    }
    TextureBakeErrorCode code() const noexcept { return m_code; }

private:
    TextureBakeErrorCode m_code;
};

/// Straight-alpha RGBA8, top-to-bottom rows. Color/emission bytes are source-encoded;
/// normal and scalar map bytes are linear data. Storage belongs to this value.
struct TextureImage {
    size_t width = 0;
    size_t height = 0;
    std::vector<uint8_t> pixels;
};

/// Borrowed straight-alpha RGBA8. Bytes must remain valid throughout bake();
/// rowStride includes padding, byteCount bounds all readable rows.
struct TextureImageView {
    size_t width = 0;
    size_t height = 0;
    size_t rowStride = 0;
    size_t byteCount = 0;
    const uint8_t* pixels = nullptr;
};

struct TextureMaterial {
    double metallic = 0;
    double roughness = 1;
    double emissive = 0;
};

/// Resolved world-space inputs, independent of archive parsing and image registries.
struct TextureSurface {
    std::optional<SurfacePattern::Settings> pattern;
    std::vector<ClothFolds::Fold> folds;
    double wrinkleStrength = 0;
};

struct TextureBakeInputs {
    std::map<Uuid, TextureImageView> images;
    std::map<Uuid, TextureMaterial> materials;
    std::map<Uuid, TextureSurface> surfaces;
};

struct TextureBakeOptions {
    size_t colorSize = 4096;
    size_t normalSize = 2048;
    size_t materialSize = 1024;
    size_t maximumSpatialGridEntries = 1000000; // Conservative cumulative fold-grid insertion bound, 1..16000000.
    size_t maximumPayloadBytes = 0; // Zero selects 256 MiB; not a process-memory cap.
    const std::atomic_bool* cancellation = nullptr; // Borrowed for this call only.
};

/// Unit tangent and explicit handedness, in original triangle order. Unknown or
/// degenerate UVs carry known=false rather than fabricated texture coordinates.
struct TextureTangent {
    Vector3 direction;
    double handedness = 1;
    bool known = false;
};

struct TextureBakeResult {
    TextureImage color;
    std::optional<TextureImage> normal;
    std::optional<TextureImage> metallic;
    std::optional<TextureImage> roughness;
    std::optional<TextureImage> emissive;
    std::vector<TextureTangent> tangents;
    size_t surfaceVertexIndexEntries = 0; // Unique position entries retained for active procedural charts.
    size_t rasterTriangleVisits = 0; // Procedural triangle-row candidate evaluations, for work diagnostics.
    size_t payloadBytes = 0;
};

class TextureBaker {
public:
    /// Bake an owned CPU atlas without modifying geometry, UVs or source pixels.
    /// @param object Generated original triangles and world-space vertices.
    /// @param atlas Aligned UV metadata and ordered layouts from UvAtlasBuilder.
    /// @param inputs Caller-resolved borrowed images and value material/surface inputs.
    /// @param options Square dimensions (1..8192), logical output quota and cancellation.
    /// @return Owned RGBA8 maps and triangle-aligned tangents; no borrowed buffers.
    /// @throws TextureBakeError Invalid inputs, exceeded output quota or cancellation.
    /// No partial result escapes on failure. The quota includes pixels and tangent
    /// records, excluding allocator overhead, inputs and raster workspace.
    /// Example: `auto baked = TextureBaker::bake(object, atlas, inputs, options);`
    static TextureBakeResult bake(const Object& object, const UvAtlas& atlas,
        const TextureBakeInputs& inputs = {}, const TextureBakeOptions& options = {});
};
}
#endif
