// SPDX-License-Identifier: MIT
#include <algorithm>
#include <cmath>
#include <dust3d/uv/source_texture_baker.h>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* cause)
{
    if (!condition)
        throw std::runtime_error(cause);
}
double sample(const dust3d::TextureImage& image, double u, double v, size_t channel)
{
    const double x = u * image.width - .5, y = v * image.height - .5;
    const double left = std::floor(x), top = std::floor(y);
    double result = 0;
    for (size_t row = 0; row < 2; ++row)
        for (size_t column = 0; column < 2; ++column) {
            const auto sourceX = static_cast<size_t>(std::clamp(left + column, 0.0, double(image.width - 1)));
            const auto sourceY = static_cast<size_t>(std::clamp(top + row, 0.0, double(image.height - 1)));
            result += image.pixels[(sourceY * image.width + sourceX) * 4 + channel]
                * (column ? x - left : 1 - (x - left)) * (row ? y - top : 1 - (y - top));
        }
    return result;
}
void isolatedBaseAndMipSampling()
{
    const dust3d::SourceUvChart charts[] { { .1, .1, .25, .25, false }, { .6, .6, .25, .25, false } };
    dust3d::SourceUvAtlas atlas;
    atlas.legacyChart = dust3d::SourceUvChart {};
    dust3d::UvAtlas legacy;
    for (const auto& chart : charts) {
        dust3d::UvMapPacker::Layout layout;
        layout.left = chart.left;
        layout.top = chart.top;
        layout.width = chart.width;
        layout.height = chart.height;
        legacy.layouts.push_back(layout);
        dust3d::SourceUvTriangle triangle;
        triangle.known = true;
        triangle.packedUvs = { chart.map({ 0, 0 }), chart.map({ 1, 0 }), chart.map({ 0, 1 }) };
        atlas.triangles.push_back(triangle);
        legacy.triangleUvs.push_back(triangle.packedUvs);
        legacy.triangleUvKnown.push_back(1);
    }
    std::vector<uint8_t> pixels(128 * 128 * 4, 255);
    std::vector<uint8_t> metallic(128 * 128 * 4, 255);
    std::vector<uint8_t> roughness(128 * 128 * 4, 255);
    std::vector<uint8_t> normals(128 * 128 * 4, 255);
    for (size_t y = 0; y < 128; ++y)
        for (size_t x = 0; x < 128; ++x) {
            const size_t offset = (y * 128 + x) * 4;
            pixels[offset] = 0;
            pixels[offset + 1] = 255;
            pixels[offset + 2] = 0;
            metallic[offset] = metallic[offset + 1] = metallic[offset + 2] = 0;
            roughness[offset] = roughness[offset + 1] = roughness[offset + 2] = 0;
            normals[offset] = 0;
            normals[offset + 1] = 255;
            normals[offset + 2] = 0;
            for (size_t chart = 0; chart < 2; ++chart) {
                const double u = (x + .5) / 128, v = (y + .5) / 128;
                if (u >= charts[chart].left && u < charts[chart].left + charts[chart].width
                    && v >= charts[chart].top && v < charts[chart].top + charts[chart].height) {
                    pixels[offset + (chart ? 2 : 0)] = 255;
                    pixels[offset + 1] = 0;
                    pixels[offset + 3] = chart ? 255 : 0;
                    for (size_t channel = 0; channel < 3; ++channel) {
                        metallic[offset + channel] = chart ? 204 : 51;
                        roughness[offset + channel] = chart ? 26 : 179;
                    }
                    normals[offset] = chart ? 128 : 255;
                    normals[offset + 1] = 128;
                    normals[offset + 2] = chart ? 255 : 128;
                }
            }
        }
    dust3d::SourceTextureBakeInputs inputs;
    inputs.legacy.color = dust3d::TextureImageView { 128, 128, 512, pixels.size(), pixels.data() };
    inputs.legacy.emissive = inputs.legacy.color;
    inputs.legacy.metallic = dust3d::TextureImageView { 128, 128, 512, metallic.size(), metallic.data() };
    inputs.legacy.roughness = dust3d::TextureImageView { 128, 128, 512, roughness.size(), roughness.data() };
    inputs.legacy.normal = dust3d::TextureImageView { 128, 128, 512, normals.size(), normals.data() };
    dust3d::SourceTextureMipBakeOptions options;
    options.bake.colorSize = options.bake.materialSize = options.bake.normalSize = 128;
    options.legacyAtlas = &legacy;
    const auto result = dust3d::SourceTextureBaker::bakeMipChain(atlas, inputs, options);
    require(result.maximumSamplingLod > 0, "Separated charts must admit at least one mip level");
    for (size_t chart = 0; chart < 2; ++chart)
        for (const auto& corner : { dust3d::Vector2(0, 0), dust3d::Vector2(1, 0), dust3d::Vector2(0, 1), dust3d::Vector2(1, 1) }) {
            const auto uv = charts[chart].map(corner);
            for (size_t level = 0; level <= result.colorLevels.size(); ++level) {
                const auto& image = level ? result.colorLevels[level - 1] : result.base.color;
                require(sample(image, uv.x(), uv.y(), 1) < 1e-6, "Bilinear footprint must not sample poisoned gaps");
                require(sample(image, uv.x(), uv.y(), chart ? 0 : 2) < 1e-6, "Bilinear footprint must not sample another chart");
                require(sample(image, uv.x(), uv.y(), chart ? 2 : 0) > 254.99, "Bilinear footprint retains its chart color");
                require(std::abs(sample(image, uv.x(), uv.y(), 3) - (chart ? 255 : 0)) < 1e-6, "Straight alpha and hidden RGB remain independent");
                const auto& metal = level ? result.metallicLevels[level - 1] : result.base.metallic;
                const auto& rough = level ? result.roughnessLevels[level - 1] : result.base.roughness;
                const auto& emission = level ? result.emissiveLevels[level - 1] : result.base.emissive;
                const auto& normal = level ? result.normalLevels[level - 1] : *result.base.normal;
                require(std::abs(sample(metal, uv.x(), uv.y(), 0) - (chart ? 204 : 51)) < 1e-6, "Metallic scalar remains chart-local");
                require(std::abs(sample(rough, uv.x(), uv.y(), 0) - (chart ? 26 : 179)) < 1e-6, "Roughness scalar remains chart-local");
                require(sample(emission, uv.x(), uv.y(), 1) < 1e-6 && sample(emission, uv.x(), uv.y(), chart ? 2 : 0) > 254.99,
                    "Emission and hidden RGB remain chart-local");
                require(std::abs(sample(normal, uv.x(), uv.y(), 1) - 128) <= 1 && sample(normal, uv.x(), uv.y(), chart ? 2 : 0) > 254,
                    "Normal directions remain chart-local and normalized");
                if (level) {
                    const auto& previous = level == 1 ? result.base.color : result.colorLevels[level - 2];
                    for (double fraction : { .25, .5, .75 }) {
                        const auto green = sample(previous, uv.x(), uv.y(), 1) * (1 - fraction) + sample(image, uv.x(), uv.y(), 1) * fraction;
                        const auto other = sample(previous, uv.x(), uv.y(), chart ? 0 : 2) * (1 - fraction) + sample(image, uv.x(), uv.y(), chart ? 0 : 2) * fraction;
                        require(green < 1e-6 && other < 1e-6, "Trilinear transitions cannot sample gaps or another chart");
                    }
                }
            }
        }
    options.bake.maximumRasterSamples = result.rasterSamples;
    require(dust3d::SourceTextureBaker::bakeMipChain(atlas, inputs, options).rasterSamples == result.rasterSamples, "Exact isolation work quota succeeds");
    --options.bake.maximumRasterSamples;
    try {
        (void)dust3d::SourceTextureBaker::bakeMipChain(atlas, inputs, options);
        throw std::runtime_error("One-short isolation work quota must fail");
    } catch (const dust3d::TextureBakeError& error) {
        require(error.code() == dust3d::TextureBakeErrorCode::limitExceeded, "Isolation work quota retains its typed cause");
    }
    options.bake.maximumRasterSamples = 250000000;
    options.bake.maximumPayloadBytes = result.payloadBytes;
    require(dust3d::SourceTextureBaker::bakeMipChain(atlas, inputs, options).payloadBytes == result.payloadBytes, "Exact retained quota succeeds");
    --options.bake.maximumPayloadBytes;
    try {
        (void)dust3d::SourceTextureBaker::bakeMipChain(atlas, inputs, options);
        throw std::runtime_error("One-short retained quota must fail");
    } catch (const dust3d::TextureBakeError& error) {
        require(error.code() == dust3d::TextureBakeErrorCode::limitExceeded, "Retained quota retains its typed cause");
    }
    options.bake.maximumPayloadBytes = 0;
    const auto ordinary = dust3d::SourceTextureBaker::bake(atlas, inputs, options.bake);
    options.maximumLevels = 1;
    const auto baseOnly = dust3d::SourceTextureBaker::bakeMipChain(atlas, inputs, options);
    require(baseOnly.maximumSamplingLod == 0 && baseOnly.base.color.pixels == ordinary.color.pixels, "Base-only requests retain their original pixels");
    options.maximumLevels = 14;
    const uint8_t undersampled[] { 255, 255, 255, 255 };
    inputs.legacy.color = dust3d::TextureImageView { 1, 1, 4, sizeof(undersampled), undersampled };
    const auto undersampledBase = dust3d::SourceTextureBaker::bake(atlas, inputs, options.bake);
    const auto undersampledResult = dust3d::SourceTextureBaker::bakeMipChain(atlas, inputs, options);
    require(undersampledResult.maximumSamplingLod == 0 && undersampledResult.hasUnprovenBaseFootprint,
        "Missing owned base samples conservatively disable mipmaps");
    require(undersampledResult.base.color.pixels == undersampledBase.color.pixels,
        "Missing owned base samples retain the original base pixels");
    require(std::string(undersampledResult.fallbackDiagnostic()).find("no owned chart coverage") != std::string::npos,
        "Missing owned base samples retain their specific diagnostic");
}
}
int main()
{
    try {
        isolatedBaseAndMipSampling();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
