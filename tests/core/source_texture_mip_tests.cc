// SPDX-License-Identifier: MIT
#include <algorithm>
#include <cmath>
#include <dust3d/uv/source_texture_baker.h>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
template <typename Action>
void rejects(Action action, dust3d::TextureBakeErrorCode code)
{
    try {
        action();
    } catch (const dust3d::TextureBakeError& error) {
        require(error.code() == code, "Wrong mip rejection classification");
        return;
    }
    throw std::runtime_error("Expected mip rejection");
}
const dust3d::TextureImage& colorLevel(const dust3d::SourceTextureMipBakeResult& maps, size_t level)
{
    return level ? maps.colorLevels.at(level - 1) : maps.base.color;
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
void diagonalChartsAndTrilinearFootprints()
{
    dust3d::SourceUvAtlas atlas;
    dust3d::SourceTextureBakeInputs inputs;
    const dust3d::SourceUvChart charts[] { { .05, .05, .25, .25, false }, { .7, .7, .25, .25, false } };
    for (size_t index = 0; index < 2; ++index) {
        dust3d::SourceUvTriangle triangle;
        triangle.modelKey = index ? "blue" : "red";
        triangle.componentKey = triangle.modelKey;
        triangle.importedChart = charts[index];
        triangle.known = true;
        triangle.packedUvs = { charts[index].map({ 0, 0 }), charts[index].map({ 1, 0 }), charts[index].map({ 0, 1 }) };
        atlas.triangles.push_back(triangle);
        dust3d::SourceTextureMaterial material;
        material.baseColor = index ? std::array<double, 4> { 0, 0, 1, 1 } : std::array<double, 4> { 1, 0, 0, 0 };
        material.policy.alphaMode = dust3d::SourceTextureAlphaMode::blend;
        material.metallic = index ? .8 : .2;
        material.roughness = index ? .1 : .7;
        material.emissive = index ? std::array<double, 3> { 0, 0, 1 } : std::array<double, 3> { 1, 0, 0 };
        inputs.materials.emplace(triangle.modelKey, material);
        inputs.triangleColors.push_back({ dust3d::Color(1, 1, 1), dust3d::Color(1, 1, 1), dust3d::Color(1, 1, 1) });
    }
    dust3d::SourceTextureMipBakeOptions options;
    options.bake.colorSize = options.bake.materialSize = 32;
    const auto maps = dust3d::SourceTextureBaker::bakeMipChain(atlas, inputs, options);
    const auto base = dust3d::SourceTextureBaker::bake(atlas, inputs, options.bake);
    require(maps.base.color.pixels == base.color.pixels && maps.base.metallic.pixels == base.metallic.pixels
            && maps.base.roughness.pixels == base.roughness.pixels && maps.base.emissive.pixels == base.emissive.pixels
            && maps.base.maximumSamplingLod == 0,
        "Opt-in mips preserve the existing base maps and their base-only sampling contract");
    require(maps.maximumSamplingLod == 2 && maps.colorLevels.size() == 2, "Derive the safe cap from diagonal collars, not chart count or a global full chain");
    require(maps.normalLevels.empty() && !maps.base.normal, "Missing normal maps remain absent");
    require(maps.base.trianglePolicies.size() == 2 && maps.base.trianglePolicies[0]->alphaMode == dust3d::SourceTextureAlphaMode::blend, "Original triangle policies survive");
    for (size_t chart = 0; chart < 2; ++chart)
        for (size_t row = 0; row <= 4; ++row)
            for (size_t column = 0; column <= 4; ++column) {
                const auto uv = charts[chart].map({ column / 4.0, row / 4.0 });
                for (size_t level = 0; level <= maps.colorLevels.size(); ++level) {
                    const auto& image = colorLevel(maps, level);
                    require(std::abs(sample(image, uv.x(), uv.y(), chart ? 2 : 0) - 255) < 1e-6, "Own chart color survives bilinear corners and gutters");
                    require(sample(image, uv.x(), uv.y(), chart ? 0 : 2) < 1e-6, "Bilinear sampling must never cross chart ownership");
                    require(std::abs(sample(image, uv.x(), uv.y(), 3) - (chart ? 255 : 0)) < 1e-6, "Straight transparent alpha and hidden RGB survive");
                    if (level) {
                        const auto& previous = colorLevel(maps, level - 1);
                        for (double fraction : { .25, .5, .75 })
                            require(sample(previous, uv.x(), uv.y(), chart ? 0 : 2) * (1 - fraction) + sample(image, uv.x(), uv.y(), chart ? 0 : 2) * fraction < 1e-6, "Fractional LOD remains isolated at both footprints");
                        require(std::abs(sample(maps.metallicLevels[level - 1], uv.x(), uv.y(), 0) - (chart ? 204 : 51)) < 1e-6, "Scalar maps retain independent chart values");
                        require(sample(maps.emissiveLevels[level - 1], uv.x(), uv.y(), chart ? 0 : 2) < 1e-6, "Emission gutters remain chart isolated");
                    }
                }
            }
    options.bake.maximumPayloadBytes = maps.payloadBytes;
    require(dust3d::SourceTextureBaker::bakeMipChain(atlas, inputs, options).payloadBytes == maps.payloadBytes, "Exact retained quota passes");
    --options.bake.maximumPayloadBytes;
    rejects([&] { dust3d::SourceTextureBaker::bakeMipChain(atlas, inputs, options); }, dust3d::TextureBakeErrorCode::limitExceeded);
    options.bake.maximumPayloadBytes = 0;
    options.bake.maximumRasterSamples = maps.rasterSamples;
    require(dust3d::SourceTextureBaker::bakeMipChain(atlas, inputs, options).rasterSamples == maps.rasterSamples, "Exact cumulative work quota passes");
    --options.bake.maximumRasterSamples;
    rejects([&] { dust3d::SourceTextureBaker::bakeMipChain(atlas, inputs, options); }, dust3d::TextureBakeErrorCode::limitExceeded);
    options.bake.maximumRasterSamples = 250000000;
    options.bake.colorSize = options.bake.materialSize = 8;
    require(dust3d::SourceTextureBaker::bakeMipChain(atlas, inputs, options).maximumSamplingLod == 0, "Closely packed reduced charts keep an explicit base-only limit");
}
void oddDimensionsAndMapSemantics()
{
    dust3d::SourceUvAtlas atlas;
    atlas.legacyChart = dust3d::SourceUvChart {};
    std::vector<uint8_t> color(5 * 5 * 4), scalar(5 * 5 * 4), normal(5 * 5 * 4);
    for (size_t pixel = 0; pixel < 25; ++pixel) {
        for (size_t channel = 0; channel < 3; ++channel)
            color[pixel * 4 + channel] = scalar[pixel * 4 + channel] = pixel % 5 == 4 ? 255 : 0;
        scalar[pixel * 4 + 3] = normal[pixel * 4 + 3] = 255;
        normal[pixel * 4] = pixel % 5 < 3 ? 255 : 128;
        normal[pixel * 4 + 1] = 128;
        normal[pixel * 4 + 2] = pixel % 5 < 3 ? 128 : 255;
    }
    dust3d::SourceTextureBakeInputs inputs;
    inputs.legacy.color = inputs.legacy.emissive = dust3d::TextureImageView { 5, 5, 20, color.size(), color.data() };
    inputs.legacy.metallic = inputs.legacy.roughness = dust3d::TextureImageView { 5, 5, 20, scalar.size(), scalar.data() };
    inputs.legacy.normal = dust3d::TextureImageView { 5, 5, 20, normal.size(), normal.data() };
    dust3d::SourceTextureMipBakeOptions options;
    dust3d::UvAtlas legacy;
    legacy.layouts.resize(1);
    legacy.layouts[0].width = legacy.layouts[0].height = 1;
    options.legacyAtlas = &legacy;
    options.bake.colorSize = options.bake.materialSize = options.bake.normalSize = 5;
    auto maps = dust3d::SourceTextureBaker::bakeMipChain(atlas, inputs, options);
    require(maps.colorLevels.size() == 2 && maps.colorLevels[0].width == 2 && maps.colorLevels[1].width == 1, "Odd dimensions floor halve without dropping edge texels");
    require(std::abs(int(maps.colorLevels[0].pixels[4]) - 170) <= 1 && maps.colorLevels[0].pixels[7] == 0, "Color uses area-weighted linear sRGB and preserves hidden RGB");
    require(maps.metallicLevels[0].pixels[4] == 102 && maps.roughnessLevels[0].pixels[4] == 102, "Scalar maps average linear values");
    require(maps.emissiveLevels[0].pixels[4] == maps.colorLevels[0].pixels[4], "Emission uses linear-light filtering");
    const auto& normals = maps.normalLevels[0].pixels;
    require(normals[4] > 150 && normals[4] < 170 && normals[6] > 245, "Normal vectors are decoded, averaged and renormalized");
    options.bake.colorSize = 33;
    options.bake.materialSize = 17;
    options.bake.normalSize = 9;
    maps = dust3d::SourceTextureBaker::bakeMipChain(atlas, inputs, options);
    require(maps.maximumSamplingLod == 3 && maps.colorLevels.back().width == 4 && maps.metallicLevels.back().width == 2 && maps.normalLevels.back().width == 1, "One common safe cap accommodates different map sizes");
    options.maximumLevels = 1;
    require(dust3d::SourceTextureBaker::bakeMipChain(atlas, inputs, options).maximumSamplingLod == 0, "Explicit base-only request retains LOD zero");
    for (size_t invalid : { size_t(0), size_t(15) }) {
        options.maximumLevels = invalid;
        rejects([&] { dust3d::SourceTextureBaker::bakeMipChain(atlas, inputs, options); }, dust3d::TextureBakeErrorCode::invalidInput);
    }
    options.maximumLevels = 14;
    std::atomic_bool cancelled { true };
    options.bake.cancellation = &cancelled;
    rejects([&] { dust3d::SourceTextureBaker::bakeMipChain(atlas, inputs, options); }, dust3d::TextureBakeErrorCode::cancelled);
}
void neutralNormalAndUnknownAtlas()
{
    const uint8_t normals[] { 255, 255, 255, 255, 0, 0, 0, 255, 255, 255, 255, 255, 0, 0, 0, 255 };
    const uint8_t white[] { 255, 255, 255, 255 };
    dust3d::SourceUvAtlas atlas;
    atlas.legacyChart = dust3d::SourceUvChart {};
    dust3d::SourceTextureBakeInputs inputs;
    inputs.legacy.color = dust3d::TextureImageView { 1, 1, 4, 4, white };
    inputs.legacy.normal = dust3d::TextureImageView { 2, 2, 8, sizeof(normals), normals };
    dust3d::SourceTextureMipBakeOptions options;
    dust3d::UvAtlas legacy;
    legacy.layouts.resize(1);
    legacy.layouts[0].width = legacy.layouts[0].height = 1;
    options.legacyAtlas = &legacy;
    options.bake.colorSize = options.bake.materialSize = options.bake.normalSize = 2;
    auto maps = dust3d::SourceTextureBaker::bakeMipChain(atlas, inputs, options);
    require(maps.normalLevels[0].pixels == std::vector<uint8_t>({ 128, 128, 255, 255 }), "Opposing normal vectors use the neutral tangent normal");
    atlas.legacyChart.reset();
    inputs = {};
    maps = dust3d::SourceTextureBaker::bakeMipChain(atlas, inputs, options);
    require(maps.colorLevels[0].pixels == std::vector<uint8_t>({ 0, 0, 0, 0 }) && maps.normalLevels.empty(), "Unknown geometry retains default transparent maps without fabricated normals");
}
void proceduralIslandsRetainTheirOwnCollars()
{
    dust3d::SourceUvAtlas atlas;
    atlas.legacyChart = dust3d::SourceUvChart {};
    dust3d::UvAtlas legacy;
    const dust3d::SourceUvChart charts[] { { .05, .1, .25, .2, false }, { .7, .6, .25, .3, false } };
    std::vector<uint8_t> pixels(32 * 32 * 4, 255);
    for (size_t pixel = 0; pixel < 32 * 32; ++pixel) {
        // Deliberately poisonous unused gutters expose reads outside an actual island.
        pixels[pixel * 4] = pixels[pixel * 4 + 2] = 0;
        for (size_t chart = 0; chart < 2; ++chart) {
            double u = (pixel % 32 + .5) / 32.0, v = (pixel / 32 + .5) / 32.0;
            if (u >= charts[chart].left && u < charts[chart].left + charts[chart].width
                && v >= charts[chart].top && v < charts[chart].top + charts[chart].height) {
                pixels[pixel * 4 + (chart ? 2 : 0)] = 255;
                pixels[pixel * 4 + 1] = 0;
            }
        }
    }
    for (const auto& chart : charts) {
        dust3d::UvMapPacker::Layout layout;
        layout.left = chart.left;
        layout.top = chart.top;
        layout.width = chart.width;
        layout.height = chart.height;
        legacy.layouts.push_back(layout);
    }
    dust3d::SourceTextureBakeInputs inputs;
    inputs.legacy.color = dust3d::TextureImageView { 32, 32, 128, pixels.size(), pixels.data() };
    dust3d::SourceTextureMipBakeOptions options;
    options.bake.colorSize = options.bake.materialSize = 32;
    require(dust3d::SourceTextureBaker::bakeMipChain(atlas, inputs, options).maximumSamplingLod == 0, "Unresolved procedural island ownership cannot enable global atlas mips");
    options.legacyAtlas = &legacy;
    const auto maps = dust3d::SourceTextureBaker::bakeMipChain(atlas, inputs, options);
    require(maps.maximumSamplingLod == 2, "Internal procedural islands determine the safe LOD");
    for (size_t level = 1; level <= maps.colorLevels.size(); ++level)
        for (size_t chart = 0; chart < 2; ++chart)
            for (const auto& coordinate : { dust3d::Vector2(0, 0), dust3d::Vector2(1, 0), dust3d::Vector2(0, 1), dust3d::Vector2(1, 1) }) {
                const auto uv = charts[chart].map(coordinate);
                const auto& image = colorLevel(maps, level);
                require(sample(image, uv.x(), uv.y(), 1) < 1e-6 && std::abs(sample(image, uv.x(), uv.y(), chart ? 2 : 0) - 255) < 1e-6, "Procedural mip collars never sample another island or poisoned source padding");
            }
    atlas.legacyChart = dust3d::SourceUvChart { .1, .05, .8, .9, true };
    options.bake.colorSize = options.bake.materialSize = 33;
    const auto transposed = dust3d::SourceTextureBaker::bakeMipChain(atlas, inputs, options);
    require(transposed.maximumSamplingLod > 0, "Transposed procedural regions retain usable mip levels");
    for (size_t level = 1; level <= transposed.colorLevels.size(); ++level)
        for (size_t chart = 0; chart < 2; ++chart)
            for (const auto& coordinate : { dust3d::Vector2(0, 0), dust3d::Vector2(1, 0), dust3d::Vector2(0, 1), dust3d::Vector2(1, 1) }) {
                const auto uv = atlas.legacyChart->map(charts[chart].map(coordinate));
                require(sample(colorLevel(transposed, level), uv.x(), uv.y(), 1) < 1e-6, "Transposition and resampling cannot import poisoned procedural padding");
            }
    atlas.legacyChart = dust3d::SourceUvChart {};
    dust3d::SourceUvTriangle unmatched;
    unmatched.known = true;
    unmatched.packedUvs = { dust3d::Vector2(.4, .4), dust3d::Vector2(.5, .4), dust3d::Vector2(.4, .5) };
    atlas.triangles.push_back(unmatched);
    legacy.triangleUvs = { unmatched.packedUvs };
    legacy.triangleUvKnown = { 1 };
    rejects([&] { dust3d::SourceTextureBaker::bakeMipChain(atlas, inputs, options); }, dust3d::TextureBakeErrorCode::invalidInput);
    atlas.triangles[0].packedUvs = { charts[0].map({ 0, 0 }), charts[0].map({ 1, 0 }), charts[0].map({ 0, 1 }) };
    legacy.triangleUvs[0] = atlas.triangles[0].packedUvs;
    require(dust3d::SourceTextureBaker::bakeMipChain(atlas, inputs, options).base.trianglePolicies[0].has_value(), "Matching known procedural triangles retain their policy");
    legacy.triangleUvKnown.clear();
    rejects([&] { dust3d::SourceTextureBaker::bakeMipChain(atlas, inputs, options); }, dust3d::TextureBakeErrorCode::invalidInput);
    legacy.triangleUvKnown = { 1 };
    legacy.triangleUvs[0][0] = { -1, 0 };
    rejects([&] { dust3d::SourceTextureBaker::bakeMipChain(atlas, inputs, options); }, dust3d::TextureBakeErrorCode::invalidInput);
    legacy.triangleUvs[0][0] = { .5, .5 };
    rejects([&] { dust3d::SourceTextureBaker::bakeMipChain(atlas, inputs, options); }, dust3d::TextureBakeErrorCode::invalidInput);
    legacy.triangleUvs[0] = atlas.triangles[0].packedUvs;
    options.bake.maximumChartCount = 1;
    rejects([&] { dust3d::SourceTextureBaker::bakeMipChain(atlas, inputs, options); }, dust3d::TextureBakeErrorCode::limitExceeded);
    options.bake.maximumChartCount = 100000;

    // A mixed atlas must keep imported ordinals distinct from internal legacy islands.
    atlas.triangles.clear();
    legacy.triangleUvs.clear();
    legacy.triangleUvKnown.clear();
    atlas.legacyChart = dust3d::SourceUvChart { 0, 0, .45, 1, false };
    dust3d::SourceUvTriangle imported;
    imported.known = true;
    imported.modelKey = "magenta";
    imported.importedChart = dust3d::SourceUvChart { .65, .1, .3, .8, false };
    imported.packedUvs = { imported.importedChart->map({ 0, 0 }), imported.importedChart->map({ 1, 0 }), imported.importedChart->map({ 0, 1 }) };
    atlas.triangles.push_back(imported);
    dust3d::SourceTextureMaterial material;
    material.baseColor = { 1, 0, 1, 1 };
    inputs.materials.emplace("magenta", material);
    inputs.triangleColors.push_back({ dust3d::Color(1, 1, 1), dust3d::Color(1, 1, 1), dust3d::Color(1, 1, 1) });
    options.bake.colorSize = options.bake.materialSize = 64;
    options.bake.maximumChartCount = 2;
    rejects([&] { dust3d::SourceTextureBaker::bakeMipChain(atlas, inputs, options); }, dust3d::TextureBakeErrorCode::limitExceeded);
    options.bake.maximumChartCount = 100000;
    const auto mixed = dust3d::SourceTextureBaker::bakeMipChain(atlas, inputs, options);
    require(mixed.maximumSamplingLod > 0, "Mixed internal and imported charts retain supported mip levels");
    for (size_t level = 1; level <= mixed.colorLevels.size(); ++level) {
        const auto uv = imported.importedChart->map({ 1, 1 });
        const auto& image = colorLevel(mixed, level);
        require(sample(image, uv.x(), uv.y(), 0) > 254.99 && sample(image, uv.x(), uv.y(), 2) > 254.99
                && sample(image, uv.x(), uv.y(), 1) < 1e-6,
            "Imported chart ownership is independent of legacy island indices");
    }
    atlas.triangles.clear();
    atlas.legacyChart = dust3d::SourceUvChart {};
    legacy.layouts.resize(1);
    legacy.layouts[0].left = legacy.layouts[0].top = .33;
    legacy.layouts[0].width = legacy.layouts[0].height = .09;
    inputs.materials.clear();
    inputs.triangleColors.clear();
    options.bake.colorSize = 10;
    options.bake.materialSize = 9;
    require(dust3d::SourceTextureBaker::bakeMipChain(atlas, inputs, options).maximumSamplingLod == 0, "Every map's actual texel centers constrain LOD, including the larger odd-dimension map");
    const uint8_t undersampled[] { 255, 255, 255, 255 };
    inputs.legacy.color = dust3d::TextureImageView { 1, 1, 4, sizeof(undersampled), undersampled };
    options.bake.colorSize = options.bake.materialSize = 32;
    rejects([&] { dust3d::SourceTextureBaker::bakeMipChain(atlas, inputs, options); }, dust3d::TextureBakeErrorCode::bakingFailed);
}
void legacyLayoutRoundingMatchesChartAdmission()
{
    dust3d::SourceUvAtlas atlas;
    atlas.legacyChart = dust3d::SourceUvChart {};
    dust3d::UvAtlas legacy;
    legacy.layouts.resize(1);
    legacy.layouts[0].left = .5;
    legacy.layouts[0].width = .5000005;
    legacy.layouts[0].height = 1;
    const uint8_t white[] { 255, 255, 255, 255 };
    dust3d::SourceTextureBakeInputs inputs;
    inputs.legacy.color = dust3d::TextureImageView { 1, 1, 4, sizeof(white), white };
    dust3d::SourceTextureMipBakeOptions options;
    options.bake.colorSize = options.bake.materialSize = 4;
    options.maximumLevels = 1;
    options.legacyAtlas = &legacy;
    require(dust3d::SourceTextureBaker::bakeMipChain(atlas, inputs, options).maximumSamplingLod == 0, "Accepted packer rounding must use clamped chart mapping rather than leak a different error type");
}
}
int main()
{
    try {
        diagonalChartsAndTrilinearFootprints();
        oddDimensionsAndMapSemantics();
        neutralNormalAndUnknownAtlas();
        proceduralIslandsRetainTheirOwnCollars();
        legacyLayoutRoundingMatchesChartAdmission();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
