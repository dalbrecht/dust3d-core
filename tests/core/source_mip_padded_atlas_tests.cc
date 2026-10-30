// SPDX-License-Identifier: MIT
#include <algorithm>
#include <cmath>
#include <dust3d/base/object.h>
#include <dust3d/uv/source_texture_baker.h>
#include <dust3d/uv/source_uv_atlas.h>
#include <dust3d/uv/uv_atlas_builder.h>
#include <iostream>
#include <native/Engine/MipAtlasPreparation.h>
#include <limits>
#include <stdexcept>

namespace {
void require(bool condition, const char* cause)
{
    if (!condition)
        throw std::runtime_error(cause);
}
double sample(const dust3d::TextureImage& image, const dust3d::Vector2& uv, size_t channel)
{
    const double x = uv.x() * image.width - .5, y = uv.y() * image.height - .5;
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
void paddedProductionAtlasKeepsAllSamplingFootprintsIsolated()
{
    const dust3d::Uuid first("00000000-0000-0000-0000-000000000001");
    const dust3d::Uuid second("00000000-0000-0000-0000-000000000002");
    dust3d::Object object;
    object.vertices = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 }, { 2, 0, 0 }, { 2.7654321, 0, 0 }, { 2, 1, 0 } };
    object.triangles = { { 0, 1, 2 }, { 3, 4, 5 } };
    const std::array<dust3d::Vector2, 3> corners { dust3d::Vector2(0, 0), dust3d::Vector2(1, 0), dust3d::Vector2(0, 1) };
    for (size_t index = 0; index < object.triangles.size(); ++index) {
        const auto& triangle = object.triangles[index];
        object.componentTriangleUvs[index ? second : first][{ dust3d::PositionKey(object.vertices[triangle[0]]),
            dust3d::PositionKey(object.vertices[triangle[1]]), dust3d::PositionKey(object.vertices[triangle[2]]) }] = corners;
    }
    std::map<dust3d::Uuid, dust3d::UvAtlasComponent> components;
    components[first] = {};
    components[second] = {};
    dust3d::UvAtlasOptions atlasOptions;
    atlasOptions.textureSize = 256;
    atlasOptions.usePackedChartExtents = true;
    atlasOptions.paddingFraction = .0625;
    auto legacy = dust3d::UvAtlasBuilder::build(object, components, atlasOptions);
    require(legacy.layouts.size() == 2, "Two procedural islands must be packed");
    std::vector<uint8_t> pixels(256 * 256 * 4, 255);
    for (size_t y = 0; y < 256; ++y)
        for (size_t x = 0; x < 256; ++x) {
            const size_t offset = (y * 256 + x) * 4;
            pixels[offset] = pixels[offset + 2] = 0;
            const double u = (x + .5) / 256, v = (y + .5) / 256;
            for (size_t chart = 0; chart < legacy.layouts.size(); ++chart) {
                const auto& layout = legacy.layouts[chart];
                if (u >= layout.left && u < layout.left + layout.width && v >= layout.top && v < layout.top + layout.height) {
                    pixels[offset + (chart ? 2 : 0)] = 255;
                    pixels[offset + 1] = 0;
                }
            }
        }
    object.vertices.insert(object.vertices.end(), { { 5, 0, 0 }, { 6, 0, 0 }, { 5, 1, 0 } });
    object.triangles.push_back({ 6, 7, 8 });
    object.triangleModelKeys = { "", "", "synthetic" };
    legacy.triangleUvs.resize(3);
    legacy.triangleUvKnown.push_back(0);
    dust3d::SourceUvAtlasOptions sourceOptions;
    sourceOptions.textureSize = 256;
    sourceOptions.paddingFraction = .0625;
    const auto source = dust3d::SourceUvAtlasBuilder::build(object, legacy, sourceOptions);
    require(source.legacyChart.has_value(), "Mixed atlas must retain the procedural region");
    dust3d::SourceTextureBakeInputs inputs;
    inputs.legacy.color = dust3d::TextureImageView { 256, 256, 1024, pixels.size(), pixels.data() };
    dust3d::SourceTextureMaterial material;
    material.baseColor = { 1, 0, 1, 1 };
    inputs.materials.emplace("synthetic", material);
    inputs.triangleColors.resize(3, { dust3d::Color(1, 1, 1), dust3d::Color(1, 1, 1), dust3d::Color(1, 1, 1) });
    dust3d::SourceTextureMipBakeOptions mip;
    mip.bake.colorSize = mip.bake.materialSize = 256;
    mip.legacyAtlas = &legacy;
    const auto result = dust3d::SourceTextureBaker::bakeMipChain(source, inputs, mip);
    require(result.maximumSamplingLod > 0 && !result.hasUnprovenLegacyOwnership && !result.hasUnprovenBaseFootprint,
        "Padded mixed atlas must admit useful proven mip levels");
    for (size_t chart = 0; chart < legacy.layouts.size(); ++chart) {
        const auto& layout = legacy.layouts[chart];
        const dust3d::SourceUvChart local { layout.left, layout.top, layout.width, layout.height, false };
        for (const auto& corner : { dust3d::Vector2(0, 0), dust3d::Vector2(1, 0), dust3d::Vector2(0, 1), dust3d::Vector2(1, 1) }) {
            const auto uv = source.legacyChart->map(local.map(corner));
            for (size_t level = 0; level <= result.colorLevels.size(); ++level) {
                const auto& image = level ? result.colorLevels[level - 1] : result.base.color;
                require(sample(image, uv, 1) < 1e-6 && sample(image, uv, chart ? 0 : 2) < 1e-6,
                    "Bilinear footprints exclude poisoned gaps and other charts");
                require(sample(image, uv, chart ? 2 : 0) > 254.99, "Bilinear footprints retain their chart color");
                if (level) {
                    const auto& previous = level == 1 ? result.base.color : result.colorLevels[level - 2];
                    for (double fraction : { .25, .5, .75 })
                        require(sample(previous, uv, 1) * (1 - fraction) + sample(image, uv, 1) * fraction < 1e-6
                                && sample(previous, uv, chart ? 0 : 2) * (1 - fraction) + sample(image, uv, chart ? 0 : 2) * fraction < 1e-6,
                            "Trilinear footprints exclude poisoned gaps");
                }
            }
        }
    }
    require(source.triangles[2].importedChart.has_value(), "Synthetic imported chart remains explicitly owned");
    for (const auto& corner : { dust3d::Vector2(0, 0), dust3d::Vector2(1, 0), dust3d::Vector2(0, 1), dust3d::Vector2(1, 1) }) {
        const auto uv = source.triangles[2].importedChart->map(corner);
        for (size_t level = 0; level <= result.colorLevels.size(); ++level) {
            const auto& image = level ? result.colorLevels[level - 1] : result.base.color;
            require(sample(image, uv, 0) > 254.99 && sample(image, uv, 1) < 1e-6 && sample(image, uv, 2) > 254.99,
                "Synthetic imported chart remains magenta at each admitted level");
        }
    }
    mip.bake.maximumRasterSamples = result.rasterSamples;
    require(dust3d::SourceTextureBaker::bakeMipChain(source, inputs, mip).rasterSamples == result.rasterSamples,
        "Exact work quota includes padding and base isolation");
    --mip.bake.maximumRasterSamples;
    try {
        (void)dust3d::SourceTextureBaker::bakeMipChain(source, inputs, mip);
        throw std::runtime_error("One-short work quota unexpectedly succeeded");
    } catch (const dust3d::TextureBakeError& error) {
        require(error.code() == dust3d::TextureBakeErrorCode::limitExceeded, "Work quota preserves its typed failure");
    }
    mip.bake.maximumRasterSamples = 250000000;
    mip.bake.maximumPayloadBytes = result.payloadBytes;
    require(dust3d::SourceTextureBaker::bakeMipChain(source, inputs, mip).payloadBytes == result.payloadBytes,
        "Exact retained quota includes all padded output");
    --mip.bake.maximumPayloadBytes;
    try {
        (void)dust3d::SourceTextureBaker::bakeMipChain(source, inputs, mip);
        throw std::runtime_error("One-short retained quota unexpectedly succeeded");
    } catch (const dust3d::TextureBakeError& error) {
        require(error.code() == dust3d::TextureBakeErrorCode::limitExceeded, "Retained quota preserves its typed failure");
    }
}
void crowdedChartsFallBackToBaseAtlas()
{
    dust3d::Object object;
    std::map<dust3d::Uuid, dust3d::UvAtlasComponent> components;
    const std::array<dust3d::Vector2, 3> corners { dust3d::Vector2(0, 0), dust3d::Vector2(1, 0), dust3d::Vector2(0, 1) };
    for (size_t index = 0; index < 25; ++index) {
        const auto id = std::to_string(index + 1);
        const dust3d::Uuid component("00000000-0000-0000-0000-" + std::string(12 - id.size(), '0') + id);
        const size_t first = object.vertices.size();
        object.vertices.insert(object.vertices.end(), { { double(index * 2), 0, 0 }, { double(index * 2 + 1), 0, 0 }, { double(index * 2), 1, 0 } });
        object.triangles.push_back({ first, first + 1, first + 2 });
        object.componentTriangleUvs[component][{ dust3d::PositionKey(object.vertices[first]), dust3d::PositionKey(object.vertices[first + 1]), dust3d::PositionKey(object.vertices[first + 2]) }] = corners;
        components[component] = {};
    }
    dust3d::UvAtlasOptions options;
    options.textureSize = 32;
    options.paddingFraction = .125;
    options.usePackedChartExtents = true;
    bool mip = true;
    std::string diagnostic;
    const auto atlas = dust3d::native::buildMipAtlasWithFallback(mip, diagnostic,
        [&] { return dust3d::UvAtlasBuilder::build(object, components, options); },
        [&] { options.paddingFraction = 0; options.usePackedChartExtents = false; return dust3d::UvAtlasBuilder::build(object, components, options); });
    require(!mip && !diagnostic.empty(), "Crowded mip padding must retain its failure and disable mipmaps");
    require(atlas.layouts.size() == 25, "A crowded valid atlas must remain usable at base level");
    require(diagnostic.find("packer") != std::string::npos, "Original packing cause must be retained");
    mip = true;
    try {
        (void)dust3d::native::buildMipAtlasWithFallback(mip, diagnostic,
            [&]() -> dust3d::UvAtlas { throw dust3d::UvAtlasError(dust3d::UvAtlasErrorCode::packingFailed, "padded cause"); },
            [&]() -> dust3d::UvAtlas { throw dust3d::UvAtlasError(dust3d::UvAtlasErrorCode::invalidInput, "base cause"); });
        throw std::runtime_error("Base failure unexpectedly succeeded");
    } catch (const dust3d::UvAtlasError& error) {
        require(error.code() == dust3d::UvAtlasErrorCode::invalidInput && std::string(error.what()).find("padded cause") != std::string::npos && std::string(error.what()).find("base cause") != std::string::npos,
            "Failed base retry must retain both causes and the base failure type");
    }
    bool retried = false;
    mip = true;
    try {
        (void)dust3d::native::buildMipAtlasWithFallback(mip, diagnostic,
            [&]() -> dust3d::UvAtlas { throw dust3d::UvAtlasError(dust3d::UvAtlasErrorCode::limitExceeded, "quota cause"); },
            [&] { retried = true; return dust3d::UvAtlas {}; });
        throw std::runtime_error("Quota failure unexpectedly succeeded");
    } catch (const dust3d::UvAtlasError& error) {
        require(error.code() == dust3d::UvAtlasErrorCode::limitExceeded && !retried, "Quota failures must not be retried");
    }
}
void invalidPaddingFailsBeforeAtlasConstruction()
{
    dust3d::Object object;
    for (double invalid : { -1.0, .126, 1e-100, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity() }) {
        dust3d::UvAtlasOptions legacy;
        legacy.paddingFraction = invalid;
        try {
            (void)dust3d::UvAtlasBuilder::build(object, {}, legacy);
            throw std::runtime_error("Invalid procedural padding unexpectedly succeeded");
        } catch (const dust3d::UvAtlasError& error) {
            require(error.code() == dust3d::UvAtlasErrorCode::invalidInput, "Procedural padding preserves typed validation");
        }
        dust3d::SourceUvAtlasOptions source;
        source.paddingFraction = invalid;
        try {
            (void)dust3d::SourceUvAtlasBuilder::build(object, {}, source);
            throw std::runtime_error("Invalid source padding unexpectedly succeeded");
        } catch (const dust3d::UvAtlasError& error) {
            require(error.code() == dust3d::UvAtlasErrorCode::invalidInput, "Source padding preserves typed validation");
        }
    }
}
}
int main()
{
    try {
        paddedProductionAtlasKeepsAllSamplingFootprintsIsolated();
        invalidPaddingFailsBeforeAtlasConstruction();
        crowdedChartsFallBackToBaseAtlas();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
