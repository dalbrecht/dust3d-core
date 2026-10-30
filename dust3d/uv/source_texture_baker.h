// SPDX-License-Identifier: MIT
#ifndef DUST3D_UV_SOURCE_TEXTURE_BAKER_H_
#define DUST3D_UV_SOURCE_TEXTURE_BAKER_H_
#include <dust3d/uv/color_texture_sampler.h>
#include <dust3d/uv/source_uv_atlas.h>
namespace dust3d {
enum class SourceColorEncoding { linear,
    srgb };
enum class SourceTextureAlphaMode { opaque,
    mask,
    blend };
struct SourceTexturePolicy {
    SourceTextureAlphaMode alphaMode = SourceTextureAlphaMode::opaque;
    /// Finite nonnegative glTF cutoff; MASK values above one hide every fragment.
    /// OPAQUE/BLEND ignore the cutoff. Preserve it without thresholding baked alpha.
    double alphaCutoff = .5;
    bool isDoubleSided = false;
};
struct SourceTextureMaterial {
    std::array<double, 4> baseColor { 1, 1, 1, 1 };
    double metallic = 1;
    double roughness = 1;
    std::array<double, 3> emissive { 0, 0, 0 };
    SourceColorEncoding vertexColorEncoding = SourceColorEncoding::srgb;
    SourceTexturePolicy policy;
    const ColorTextureSampler* colorTexture = nullptr; // Borrowed throughout bake().
};
struct LegacyTextureViews {
    std::optional<TextureImageView> color;
    std::optional<TextureImageView> normal;
    std::optional<TextureImageView> metallic;
    std::optional<TextureImageView> roughness;
    std::optional<TextureImageView> emissive;
    SourceTexturePolicy policy { SourceTextureAlphaMode::blend, .5, false };
};
struct SourceTextureBakeInputs {
    std::map<std::string, SourceTextureMaterial> materials;
    std::vector<std::array<Color, 3>> triangleColors; // Original atlas order, required when imports exist.
    LegacyTextureViews legacy; // Encoded legacy color/emission, linear normal/scalar data.
};
struct SourceTextureBakeOptions {
    size_t colorSize = 4096;
    size_t materialSize = 1024;
    size_t normalSize = 2048;
    size_t maximumPayloadBytes = 0; // Zero selects 256 MiB, also the maximum.
    size_t maximumChartCount = 100000; // Caller work limit, supported range 1..100000.
    size_t maximumRasterSamples = 250000000; // Initialization, candidate pixels and dilation; 1..1000000000.
    const std::atomic_bool* cancellation = nullptr; // Borrowed only during bake().
};
struct SourceTextureBakeResult {
    TextureImage color;
    TextureImage metallic;
    TextureImage roughness;
    TextureImage emissive;
    std::optional<TextureImage> normal;
    std::vector<std::optional<SourceTexturePolicy>> trianglePolicies;
    /// Output supports base-level bilinear filtering. Global atlas mip chains are
    /// not chart-isolated; callers must clamp output sampling to this LOD.
    double maximumSamplingLod = 0;
    size_t payloadBytes = 0;
    size_t rasterSamples = 0;
};
struct SourceTextureMipBakeOptions {
    SourceTextureBakeOptions bake;
    size_t maximumLevels = 14; // Includes the base; supported range 1..14.
    /// Borrow the original procedural atlas throughout bakeMipChain(). Its layouts
    /// identify the islands inside the packed legacy region; absence keeps LOD zero
    /// when a legacy region exists. Known triangle UVs must match that same atlas.
    const UvAtlas* legacyAtlas = nullptr;
};
struct SourceTextureMipBakeResult {
    SourceTextureBakeResult base;
    // Additional floor-halved levels only; level zero is retained once in base.
    std::vector<TextureImage> colorLevels;
    std::vector<TextureImage> metallicLevels;
    std::vector<TextureImage> roughnessLevels;
    std::vector<TextureImage> emissiveLevels;
    std::vector<TextureImage> normalLevels; // Empty when the base normal is absent.
    double maximumSamplingLod = 0;
    bool hasUnprovenLegacyOwnership = false;
    bool hasUnprovenBaseFootprint = false;
    /// Returns a static diagnostic for an ownership fallback, otherwise an empty string.
    /// The pointer remains valid for the program lifetime.
    const char* fallbackDiagnostic() const noexcept
    {
        if (hasUnprovenLegacyOwnership)
            return "Known procedural mip triangle has no single owning island; mipmaps are disabled.";
        if (hasUnprovenBaseFootprint)
            return "Base mip footprint has no owned chart coverage; mipmaps are disabled.";
        return "";
    }
    size_t payloadBytes = 0;
    size_t rasterSamples = 0;
};
class SourceTextureBaker {
public:
    /// Bake owned base maps and chart-isolated, area-filtered additional mip levels.
    /// @param atlas Coherent source chart layout, subject to the same admission as bake().
    /// @param inputs Exact resolved materials and borrowed images, valid for this call.
    /// @param options Base dimensions, cumulative retained/work quotas, cancellation and level cap.
    /// @return Base maps plus additional levels and a common safe LOD. Every map has
    /// that many additional levels; dimensions floor halve. The chain stops before
    /// any map reaches an overlapping one-texel chart collar, loses an island's
    /// interior texel center or has dimension one. Procedural regions require
    /// options.legacyAtlas so distinct internal islands never become one owner.
    /// Box footprints use only matching chart-interior samples; uncovered collars dilate
    /// within that owner. Both bilinear footprints of trilinear sampling stay isolated.
    /// Color/emission average in linear light, scalars linearly, normals as normalized
    /// decoded vectors. Straight alpha and hidden RGB remain independent.
    /// Original triangle policies and base-only bake() behavior remain unchanged.
    /// @throws TextureBakeError Invalid inputs, retained/work quota or cancellation.
    /// Allocation failures propagate. Payload includes root and retained capacities;
    /// temporary ownership/coverage/filtering storage and allocator overhead are excluded.
    /// This is not an RSS limit or native/renderer/export adoption. Callers must honor
    /// maximumSamplingLod; never append globally filtered levels beyond this chain.
    /// Example: `auto maps = SourceTextureBaker::bakeMipChain(atlas, resolvedInputs);`
    static SourceTextureMipBakeResult bakeMipChain(const SourceUvAtlas& atlas, const SourceTextureBakeInputs& inputs, const SourceTextureMipBakeOptions& options = {});

    /// Rasterize independently packed source charts and compose an existing procedural bake.
    /// @param atlas Coherent ordinal layout from SourceUvAtlasBuilder; overlapping charts are rejected.
    /// @param inputs Exact-key resolved materials, aligned source colors and borrowed sampler/image owners.
    /// All borrowed storage stays valid for this call; no file decoding or engine globals are used.
    /// @param options Map dimensions, retained output quota, cumulative texel-work quota and cancellation.
    /// @return Owned sRGB color/emission, linear scalar/normal maps and original-order material policies.
    /// RGB is decoded before interpolation, then multiplied by linear material/texture factors once.
    /// Alpha is straight; opaque forces one, mask/blend retain alpha and their explicit draw policy.
    /// Mask coverage is not prematurely thresholded. Legacy maps use nearest encoded resampling;
    /// transposition swaps and negates normal XY for the baker's top-down tangent convention.
    /// @throws TextureBakeError Invalid/incomplete inputs, insufficient texel resolution, quota or cancellation.
    /// Allocation failures may throw std::bad_alloc. Payload includes root, pixel/policy capacities;
    /// excludes caller inputs, allocator overhead, overlap/raster/dilation workspace (not an RSS cap).
    /// Every imported triangle must cover a texel in each output map, with disjoint
    /// one-texel chart collars for bilinear filtering; otherwise increase resolution.
    /// Output sampling is clamped to LOD zero; source texture mip selection is independent.
    /// Cancellation is checked during validation, between rows and every 4096 texel operations.
    /// Example: `auto maps = SourceTextureBaker::bake(atlas, resolvedInputs);`
    static SourceTextureBakeResult bake(const SourceUvAtlas& atlas, const SourceTextureBakeInputs& inputs, const SourceTextureBakeOptions& options = {});
};
}
#endif
