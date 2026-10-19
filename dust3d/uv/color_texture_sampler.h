// SPDX-License-Identifier: MIT
#ifndef DUST3D_UV_COLOR_TEXTURE_SAMPLER_H_
#define DUST3D_UV_COLOR_TEXTURE_SAMPLER_H_
#include <array>
#include <dust3d/uv/texture_baker.h>

namespace dust3d {
enum class ColorTextureWrap { repeatTexture,
    mirroredRepeat,
    clampToEdge };
enum class ColorTextureMagnificationFilter { nearest,
    linear };
enum class ColorTextureMinificationFilter { nearest,
    linear,
    nearestMipmapNearest,
    linearMipmapNearest,
    nearestMipmapLinear,
    linearMipmapLinear };
struct ColorTextureSamplingOptions {
    ColorTextureWrap wrapS = ColorTextureWrap::repeatTexture;
    ColorTextureWrap wrapT = ColorTextureWrap::repeatTexture;
    ColorTextureMagnificationFilter magnification = ColorTextureMagnificationFilter::linear;
    ColorTextureMinificationFilter minification = ColorTextureMinificationFilter::linearMipmapLinear;
    size_t maximumPayloadBytes = 0; // Zero selects 64 MiB; maximum 256 MiB.
    const std::atomic_bool* cancellation = nullptr; // Borrowed only during create().
};
/// Immutable owned sRGB color texture, independent of image decoders and GPU APIs.
/// RGB is decoded before filtering; alpha remains straight and linear. Hidden RGB
/// is retained. Mips use full-area box reduction, then quantize back to RGBA8;
/// this is a portable policy, not a promise of bit equality with driver mipmaps.
class ColorTextureSampler {
public:
    /// Copy a top-to-bottom RGBA8 image and construct requested mip levels.
    /// @param image Borrowed source (including optional row padding), valid for this call.
    /// @param options Wrapping, filters, retained payload quota and call cancellation.
    /// @return Independent owned sampler. No source or cancellation pointer is retained.
    /// @throws TextureBakeError Invalid dimensions/storage/options, exceeded quota or cancellation.
    /// Allocation failures may throw std::bad_alloc. Logical payload includes this
    /// object, allocated level descriptors and pixel capacities; excludes allocator
    /// overhead, input and temporary construction workspace (not a process-memory cap).
    /// Example: `auto sampler = ColorTextureSampler::create(image);`
    static ColorTextureSampler create(const TextureImageView& image, const ColorTextureSamplingOptions& options = {});
    /// Sample linear RGB and straight alpha at authored UVs and explicit mip LOD.
    /// @param u Horizontal texture coordinate; finite values may be outside [0,1].
    /// @param v Vertical coordinate; zero addresses the top of the source image.
    /// @param lod Finite mip level; nonpositive selects magnification, positive
    /// selects minification, clamped to available levels before integer conversion.
    /// @return Four channels in [0,1]. Concurrent const sampling is supported.
    /// @throws TextureBakeError For nonfinite UVs or LOD.
    std::array<double, 4> sampleLinear(double u, double v, double lod = 0) const;
    /// Number of owned levels, including the original image.
    size_t mipLevelCount() const noexcept { return m_levels.size(); }
    /// Retained logical payload in bytes, as defined by create().
    size_t payloadBytes() const noexcept { return m_payloadBytes; }

private:
    struct Level {
        size_t width = 0;
        size_t height = 0;
        std::vector<uint8_t> pixels;
    };
    ColorTextureSampler() = default;
    std::array<double, 4> sampleLevel(size_t level, double u, double v, bool linear) const;
    std::vector<Level> m_levels;
    ColorTextureWrap m_wrapS = ColorTextureWrap::repeatTexture;
    ColorTextureWrap m_wrapT = ColorTextureWrap::repeatTexture;
    ColorTextureMagnificationFilter m_magnification = ColorTextureMagnificationFilter::linear;
    ColorTextureMinificationFilter m_minification = ColorTextureMinificationFilter::linearMipmapLinear;
    size_t m_payloadBytes = 0;
};
}
#endif
