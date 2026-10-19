// SPDX-License-Identifier: MIT
#include <cmath>
#include <dust3d/uv/source_texture_baker.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <native/Engine/GenerationPipeline.h>
#include <native/Engine/ImportedModels.h>
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
        require(error.code() == code, "wrong source bake error classification");
        return;
    }
    throw std::runtime_error("expected source bake failure");
}
dust3d::SourceUvAtlas atlas()
{
    dust3d::SourceUvAtlas result;
    dust3d::SourceUvTriangle triangle;
    triangle.modelKey = "model";
    triangle.componentKey = "component";
    triangle.known = true;
    triangle.importedChart = dust3d::SourceUvChart {};
    triangle.packedUvs = { dust3d::Vector2(0, 0), dust3d::Vector2(1, 0), dust3d::Vector2(0, 1) };
    triangle.authoredUvs = triangle.packedUvs;
    result.triangles.push_back(triangle);
    return result;
}
dust3d::SourceTextureBakeInputs inputs()
{
    dust3d::SourceTextureBakeInputs result;
    result.materials.emplace("model", dust3d::SourceTextureMaterial {});
    result.triangleColors.push_back({ dust3d::Color(1, 1, 1), dust3d::Color(1, 1, 1), dust3d::Color(1, 1, 1) });
    return result;
}
dust3d::SourceTextureBakeOptions options()
{
    dust3d::SourceTextureBakeOptions result;
    result.colorSize = result.materialSize = result.normalSize = 4;
    return result;
}
void linearFactorsAndPolicies()
{
    auto source = atlas();
    auto resolved = inputs();
    auto settings = options();
    const uint8_t gray[] { 128, 128, 128, 128 };
    auto sampler = dust3d::ColorTextureSampler::create({ 1, 1, 4, 4, gray });
    auto& material = resolved.materials.at("model");
    material.colorTexture = &sampler;
    material.vertexColorEncoding = dust3d::SourceColorEncoding::linear;
    material.baseColor = { .5, .5, .5, .5 };
    material.metallic = .25;
    material.roughness = .75;
    material.emissive = { .5, 0, 1 };
    for (auto& color : resolved.triangleColors[0])
        color = dust3d::Color(.5, .5, .5, .5);
    for (auto mode : { dust3d::SourceTextureAlphaMode::opaque, dust3d::SourceTextureAlphaMode::mask, dust3d::SourceTextureAlphaMode::blend }) {
        material.policy.alphaMode = mode;
        material.policy.alphaCutoff = .3;
        material.policy.isDoubleSided = true;
        auto result = dust3d::SourceTextureBaker::bake(source, resolved, settings);
        require(result.color.pixels[0] == 66, "sRGB filtering and linear factors must apply once");
        require(result.color.pixels[3] == (mode == dust3d::SourceTextureAlphaMode::opaque ? 255 : 32), "straight alpha policy");
        require(result.metallic.pixels[0] == 64 && result.roughness.pixels[0] == 191, "scalar material output");
        require(result.emissive.pixels[0] == 188 && result.emissive.pixels[1] == 0 && result.emissive.pixels[2] == 255, "linear emissive encoded independently");
        require(result.trianglePolicies.size() == 1 && result.trianglePolicies[0] && result.trianglePolicies[0]->alphaMode == mode
                && result.trianglePolicies[0]->alphaCutoff == .3 && result.trianglePolicies[0]->isDoubleSided,
            "per-triangle alpha and sidedness retained");
        require(result.color.pixels.back() == result.color.pixels[3], "transparent coverage participates in gutter dilation");
    }
    material.policy.alphaMode = dust3d::SourceTextureAlphaMode::blend;
    material.vertexColorEncoding = dust3d::SourceColorEncoding::srgb;
    auto encoded = dust3d::SourceTextureBaker::bake(source, resolved, settings);
    require(encoded.color.pixels[0] == 42, "encoded vertex colors decode before factors");
    auto budget = encoded.payloadBytes;
    settings.maximumPayloadBytes = budget;
    require(dust3d::SourceTextureBaker::bake(source, resolved, settings).payloadBytes == budget, "exact owned output quota");
    --settings.maximumPayloadBytes;
    rejects([&] { dust3d::SourceTextureBaker::bake(source, resolved, settings); }, dust3d::TextureBakeErrorCode::limitExceeded);
}
void authoredRepeatAndLod()
{
    {
        const uint8_t transparent[] { 200, 0, 100, 0 };
        auto sampler = dust3d::ColorTextureSampler::create({ 1, 1, 4, 4, transparent });
        auto resolved = inputs();
        resolved.materials.at("model").colorTexture = &sampler;
        resolved.materials.at("model").policy.alphaMode = dust3d::SourceTextureAlphaMode::blend;
        auto result = dust3d::SourceTextureBaker::bake(atlas(), resolved, options());
        require(result.color.pixels[0] == 200 && result.color.pixels[2] == 100 && result.color.pixels[3] == 0
                && result.color.pixels[result.color.pixels.size() - 4] == 200 && result.color.pixels.back() == 0,
            "hidden RGB and alpha-zero coverage survive rasterization and dilation");
    }
    auto source = atlas();
    source.triangles[0].authoredUvs = std::array<dust3d::Vector2, 3> { dust3d::Vector2(0, 0), dust3d::Vector2(4, 0), dust3d::Vector2(0, 4) };
    const uint8_t checker[] { 0, 0, 0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0, 0, 0, 255 };
    auto sampler = dust3d::ColorTextureSampler::create({ 2, 2, 8, 16, checker });
    auto resolved = inputs();
    resolved.materials.at("model").colorTexture = &sampler;
    auto result = dust3d::SourceTextureBaker::bake(source, resolved, options());
    require(result.color.pixels[0] == 188, "UV derivatives choose the linear-light mip for repeated sampling");
    source.triangles[0].authoredUvs = std::array<dust3d::Vector2, 3> { dust3d::Vector2(-.25, .25), dust3d::Vector2(-.25, .25), dust3d::Vector2(-.25, .25) };
    dust3d::ColorTextureSamplingOptions nearest;
    nearest.magnification = dust3d::ColorTextureMagnificationFilter::nearest;
    nearest.minification = dust3d::ColorTextureMinificationFilter::nearest;
    auto repeated = dust3d::ColorTextureSampler::create({ 2, 2, 8, 16, checker }, nearest);
    resolved.materials.at("model").colorTexture = &repeated;
    result = dust3d::SourceTextureBaker::bake(source, resolved, options());
    require(result.color.pixels[0] == 255, "authored negative UVs retain repeat semantics");
    source.triangles[0].authoredUvs.reset();
    rejects([&] { dust3d::SourceTextureBaker::bake(source, resolved, options()); }, dust3d::TextureBakeErrorCode::invalidInput);
    resolved.materials.at("model").colorTexture = nullptr;
    require(dust3d::SourceTextureBaker::bake(source, resolved, options()).color.pixels[0] == 255, "textureless source needs no fabricated UVs");
}

void fullNonnegativeAlphaCutoffRange()
{
    auto resolved = inputs();
    auto& material = resolved.materials.at("model");
    material.baseColor[3] = .5;
    for (auto mode : { dust3d::SourceTextureAlphaMode::opaque, dust3d::SourceTextureAlphaMode::mask, dust3d::SourceTextureAlphaMode::blend }) {
        material.policy.alphaMode = mode;
        for (double cutoff : { 0.0, std::numeric_limits<double>::denorm_min(), 1.0, 1.5, std::numeric_limits<double>::max() }) {
            material.policy.alphaCutoff = cutoff;
            auto result = dust3d::SourceTextureBaker::bake(atlas(), resolved, options());
            require(result.trianglePolicies.at(0) && result.trianglePolicies[0]->alphaCutoff == cutoff,
                "Every finite nonnegative glTF alpha cutoff must remain exact in source policy");
            require(result.color.pixels[3] == (mode == dust3d::SourceTextureAlphaMode::opaque ? 255 : 128),
                "Cutoff belongs to rendering; baking must preserve combined straight alpha for mask and blend");
        }
    }
    for (double cutoff : { -1.0, -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN() }) {
        material.policy.alphaCutoff = cutoff;
        rejects([&] { dust3d::SourceTextureBaker::bake(atlas(), resolved, options()); }, dust3d::TextureBakeErrorCode::invalidInput);
    }
}
void legacyComposition()
{
    auto source = atlas();
    source.triangles[0].modelKey.clear();
    source.triangles[0].importedChart.reset();
    source.triangles[0].authoredUvs.reset();
    source.legacyChart = dust3d::SourceUvChart {};
    auto resolved = inputs();
    resolved.materials.clear();
    resolved.triangleColors.clear();
    const uint8_t color[] { 40, 80, 120, 20 };
    const uint8_t normal[] { 128, 255, 128, 255 };
    resolved.legacy.color = dust3d::TextureImageView { 1, 1, 4, 4, color };
    resolved.legacy.normal = dust3d::TextureImageView { 1, 1, 4, 4, normal };
    auto result = dust3d::SourceTextureBaker::bake(source, resolved, options());
    require(result.color.pixels[0] == 40 && result.color.pixels[3] == 20, "legacy encoded pixels preserve color policy and alpha");
    require(result.normal && result.normal->pixels[0] == 128 && result.normal->pixels[1] == 255, "legacy normal ownership");
    require(result.metallic.pixels[0] == 0 && result.roughness.pixels[0] == 255 && result.emissive.pixels[0] == 0, "legacy default material maps");
    source.legacyChart->flipped = true;
    result = dust3d::SourceTextureBaker::bake(source, resolved, options());
    require(result.normal->pixels[0] == 0 && result.normal->pixels[1] == 127, "transposed top-down tangent basis changes both normal signs");
    resolved.legacy.color.reset();
    rejects([&] { dust3d::SourceTextureBaker::bake(source, resolved, options()); }, dust3d::TextureBakeErrorCode::invalidInput);
}
void failures()
{
    auto source = atlas();
    auto resolved = inputs();
    auto settings = options();
    settings.maximumRasterSamples = 1;
    rejects([&] { dust3d::SourceTextureBaker::bake(source, resolved, settings); }, dust3d::TextureBakeErrorCode::limitExceeded);
    settings = options();
    std::atomic_bool cancellation { true };
    settings.cancellation = &cancellation;
    rejects([&] { dust3d::SourceTextureBaker::bake(source, resolved, settings); }, dust3d::TextureBakeErrorCode::cancelled);
    settings = options();
    resolved.materials.clear();
    rejects([&] { dust3d::SourceTextureBaker::bake(source, resolved, settings); }, dust3d::TextureBakeErrorCode::invalidInput);
    resolved = inputs();
    source.triangles.push_back(source.triangles[0]);
    resolved.triangleColors.push_back(resolved.triangleColors[0]);
    rejects([&] { dust3d::SourceTextureBaker::bake(source, resolved, settings); }, dust3d::TextureBakeErrorCode::invalidInput);
    source = atlas();
    resolved = inputs();
    source.triangles[0].packedUvs[2] = source.triangles[0].packedUvs[0];
    rejects([&] { dust3d::SourceTextureBaker::bake(source, resolved, settings); }, dust3d::TextureBakeErrorCode::invalidInput);
    source = atlas();
    source.triangles[0].importedChart = dust3d::SourceUvChart { 0, 0, .01, .01, false };
    for (auto& uv : source.triangles[0].packedUvs)
        uv = uv * .01;
    rejects([&] { dust3d::SourceTextureBaker::bake(source, resolved, settings); }, dust3d::TextureBakeErrorCode::limitExceeded);
    source = atlas();
    resolved.materials.at("model").metallic = std::numeric_limits<double>::quiet_NaN();
    rejects([&] { dust3d::SourceTextureBaker::bake(source, resolved, settings); }, dust3d::TextureBakeErrorCode::invalidInput);
}

void mixedAndInterpolated()
{
    auto source = atlas();
    auto resolved = inputs();
    resolved.materials.at("model").vertexColorEncoding = dust3d::SourceColorEncoding::linear;
    resolved.triangleColors[0] = { dust3d::Color(1, 0, 0), dust3d::Color(0, 1, 0), dust3d::Color(0, 0, 1) };
    auto result = dust3d::SourceTextureBaker::bake(source, resolved, options());
    require(result.color.pixels[0] == 225 && result.color.pixels[1] == 99 && result.color.pixels[2] == 99, "independent barycentric corner interpolation");
    std::swap(source.triangles[0].packedUvs[1], source.triangles[0].packedUvs[2]);
    std::swap(resolved.triangleColors[0][1], resolved.triangleColors[0][2]);
    require(dust3d::SourceTextureBaker::bake(source, resolved, options()).color.pixels == result.color.pixels, "reversed winding preserves corresponding corner colors");
    source = atlas();
    resolved = inputs();
    source.legacyChart = dust3d::SourceUvChart { 0, 0, .4, 1, false };
    source.triangles[0].importedChart = dust3d::SourceUvChart { .6, 0, .4, 1, false };
    for (auto& uv : source.triangles[0].packedUvs)
        uv[0] = .6 + .4 * uv.x();
    auto procedural = source.triangles[0];
    procedural.modelKey.clear();
    procedural.importedChart.reset();
    procedural.authoredUvs.reset();
    for (auto& uv : procedural.packedUvs)
        uv[0] -= .6;
    source.triangles.push_back(procedural);
    resolved.triangleColors.push_back(resolved.triangleColors[0]);
    for (auto& color : resolved.triangleColors[0])
        color = dust3d::Color(1, 0, 0);
    const uint8_t legacyColor[] { 40, 80, 120, 20 };
    const uint8_t scalar[] { 51, 51, 51, 255 };
    const uint8_t normal[] { 128, 128, 255, 255 };
    resolved.legacy.color = dust3d::TextureImageView { 1, 1, 4, 4, legacyColor };
    resolved.legacy.metallic = dust3d::TextureImageView { 1, 1, 4, 4, scalar };
    resolved.legacy.normal = dust3d::TextureImageView { 1, 1, 4, 4, normal };
    auto mixedSettings = options();
    mixedSettings.colorSize = mixedSettings.materialSize = mixedSettings.normalSize = 16;
    result = dust3d::SourceTextureBaker::bake(source, resolved, mixedSettings);
    require(result.color.pixels[0] == 40 && result.color.pixels[3] == 20 && result.color.pixels[48] == 255 && result.color.pixels[49] == 0, "mixed chart composition");
    require(result.metallic.pixels[0] == 51 && result.metallic.pixels[48] == 255, "mixed scalar maps");
    require(result.normal && result.normal->pixels[48] == 128 && result.normal->pixels[50] == 255, "imported geometric normals keep flat tangent normal");
    require(result.trianglePolicies[0]->alphaMode == dust3d::SourceTextureAlphaMode::opaque && result.trianglePolicies[1]->alphaMode == dust3d::SourceTextureAlphaMode::blend, "mixed per-triangle policies");
}
void additionalFailures()
{
    auto source = atlas();
    auto resolved = inputs();
    for (size_t size : { size_t(0), size_t(8193) }) {
        auto settings = options();
        settings.colorSize = size;
        rejects([&] { dust3d::SourceTextureBaker::bake(source, resolved, settings); }, dust3d::TextureBakeErrorCode::invalidInput);
    }
    for (size_t work : { size_t(0), size_t(1000000001) }) {
        auto settings = options();
        settings.maximumRasterSamples = work;
        rejects([&] { dust3d::SourceTextureBaker::bake(source, resolved, settings); }, dust3d::TextureBakeErrorCode::invalidInput);
    }
    auto settings = options();
    settings.maximumPayloadBytes = 256 * 1024 * 1024 + 1;
    rejects([&] { dust3d::SourceTextureBaker::bake(source, resolved, settings); }, dust3d::TextureBakeErrorCode::invalidInput);
    resolved.materials.at("model").policy.alphaMode = static_cast<dust3d::SourceTextureAlphaMode>(99);
    rejects([&] { dust3d::SourceTextureBaker::bake(source, resolved, options()); }, dust3d::TextureBakeErrorCode::invalidInput);
    resolved = inputs();
    resolved.materials.at("model").vertexColorEncoding = static_cast<dust3d::SourceColorEncoding>(99);
    rejects([&] { dust3d::SourceTextureBaker::bake(source, resolved, options()); }, dust3d::TextureBakeErrorCode::invalidInput);
    resolved = inputs();
    resolved.materials.at("model").baseColor[0] = 2;
    rejects([&] { dust3d::SourceTextureBaker::bake(source, resolved, options()); }, dust3d::TextureBakeErrorCode::invalidInput);
    resolved = inputs();
    resolved.materials.at("model").emissive[0] = -1;
    rejects([&] { dust3d::SourceTextureBaker::bake(source, resolved, options()); }, dust3d::TextureBakeErrorCode::invalidInput);
    resolved = inputs();
    resolved.triangleColors[0][0][0] = std::numeric_limits<double>::quiet_NaN();
    rejects([&] { dust3d::SourceTextureBaker::bake(source, resolved, options()); }, dust3d::TextureBakeErrorCode::invalidInput);
    resolved = inputs();
    resolved.triangleColors.clear();
    rejects([&] { dust3d::SourceTextureBaker::bake(source, resolved, options()); }, dust3d::TextureBakeErrorCode::invalidInput);
    resolved = inputs();
    source.triangles[0].packedUvs[0][0] = -.1;
    rejects([&] { dust3d::SourceTextureBaker::bake(source, resolved, options()); }, dust3d::TextureBakeErrorCode::invalidInput);
    source = atlas();
    source.triangles[0].importedChart->width = 0;
    rejects([&] { dust3d::SourceTextureBaker::bake(source, resolved, options()); }, dust3d::TextureBakeErrorCode::invalidInput);
    source = atlas();
    const uint8_t pixel[] { 255, 255, 255, 255 };
    auto sampler = dust3d::ColorTextureSampler::create({ 1, 1, 4, 4, pixel });
    auto moved = std::move(sampler);
    require(moved.width() == 1 && moved.height() == 1, "sampler dimensions retained");
    resolved.materials.at("model").colorTexture = &sampler;
    rejects([&] { dust3d::SourceTextureBaker::bake(source, resolved, options()); }, dust3d::TextureBakeErrorCode::invalidInput);
    resolved.materials.at("model").colorTexture = &moved;
    source.triangles[0].authoredUvs->at(0)[0] = std::numeric_limits<double>::infinity();
    rejects([&] { dust3d::SourceTextureBaker::bake(source, resolved, options()); }, dust3d::TextureBakeErrorCode::invalidInput);
    source = atlas();
    source.triangles[0].authoredUvs->at(0)[0] = -1e308;
    source.triangles[0].authoredUvs->at(1)[0] = 1e308;
    rejects([&] { dust3d::SourceTextureBaker::bake(source, resolved, options()); }, dust3d::TextureBakeErrorCode::invalidInput);
    source = atlas();
    resolved = inputs();
    resolved.legacy.color = dust3d::TextureImageView { 1, 1, 4, 4, pixel };
    rejects([&] { dust3d::SourceTextureBaker::bake(source, resolved, options()); }, dust3d::TextureBakeErrorCode::invalidInput);
    source.triangles[0] = dust3d::SourceUvTriangle {};
    resolved.legacy.color.reset();
    auto unknown = dust3d::SourceTextureBaker::bake(source, resolved, options());
    require(!unknown.trianglePolicies[0] && unknown.color.pixels[3] == 0, "unknown procedural surfaces remain explicit");
    source.legacyChart = dust3d::SourceUvChart {};
    resolved.legacy.color = dust3d::TextureImageView { 1, 1, 3, 4, pixel };
    rejects([&] { dust3d::SourceTextureBaker::bake(source, resolved, options()); }, dust3d::TextureBakeErrorCode::invalidInput);
    resolved.legacy.color = dust3d::TextureImageView { 0, 1, 4, 4, pixel };
    rejects([&] { dust3d::SourceTextureBaker::bake(source, resolved, options()); }, dust3d::TextureBakeErrorCode::invalidInput);
}
void remainingBoundaries()
{
    auto source = atlas();
    auto resolved = inputs();
    for (size_t count : { size_t(0), size_t(100001) }) {
        auto settings = options();
        settings.maximumChartCount = count;
        rejects([&] { dust3d::SourceTextureBaker::bake(source, resolved, settings); }, dust3d::TextureBakeErrorCode::invalidInput);
    }
    source.triangles.push_back(source.triangles[0]);
    resolved.triangleColors.push_back(resolved.triangleColors[0]);
    auto settings = options();
    settings.maximumChartCount = 1;
    rejects([&] { dust3d::SourceTextureBaker::bake(source, resolved, settings); }, dust3d::TextureBakeErrorCode::limitExceeded);
    source = atlas();
    resolved = inputs();
    for (size_t index = 0; index < 10000; ++index)
        resolved.materials.emplace(std::to_string(index), dust3d::SourceTextureMaterial {});
    rejects([&] { dust3d::SourceTextureBaker::bake(source, resolved, options()); }, dust3d::TextureBakeErrorCode::limitExceeded);
    resolved = inputs();
    source.triangles[0].modelKey.clear();
    rejects([&] { dust3d::SourceTextureBaker::bake(source, resolved, options()); }, dust3d::TextureBakeErrorCode::invalidInput);
    source.triangles[0].importedChart.reset();
    source.triangles[0].authoredUvs.reset();
    rejects([&] { dust3d::SourceTextureBaker::bake(source, resolved, options()); }, dust3d::TextureBakeErrorCode::invalidInput);
    const uint8_t pixel[] { 128, 128, 128, 255 };
    resolved.legacy.color = dust3d::TextureImageView { 1, 1, 4, 4, pixel };
    source.legacyChart = dust3d::SourceUvChart { 0, 0, .1, .1, false };
    rejects([&] { dust3d::SourceTextureBaker::bake(source, resolved, options()); }, dust3d::TextureBakeErrorCode::invalidInput);
    for (auto& uv : source.triangles[0].packedUvs)
        uv = uv * .1;
    rejects([&] { dust3d::SourceTextureBaker::bake(source, resolved, options()); }, dust3d::TextureBakeErrorCode::limitExceeded);
    source = atlas();
    resolved = inputs();
    source.triangles[0].packedUvs[2][1] = 1e-323;
    rejects([&] { dust3d::SourceTextureBaker::bake(source, resolved, options()); }, dust3d::TextureBakeErrorCode::invalidInput);
    source = atlas();
    resolved = inputs();
    source.triangles[0].importedChart = dust3d::SourceUvChart { 0, 0, .125, 1, false };
    source.triangles[0].packedUvs = { dust3d::Vector2(0, 0), dust3d::Vector2(.125, 0), dust3d::Vector2(.125, 1) };
    auto adjacent = source.triangles[0];
    adjacent.importedChart = dust3d::SourceUvChart { .125, 0, .5, 1, false };
    adjacent.packedUvs = { dust3d::Vector2(.125, 0), dust3d::Vector2(.625, 0), dust3d::Vector2(.125, 1) };
    source.triangles.push_back(adjacent);
    resolved.triangleColors.push_back(resolved.triangleColors[0]);
    rejects([&] { dust3d::SourceTextureBaker::bake(source, resolved, options()); }, dust3d::TextureBakeErrorCode::limitExceeded);
}

void verifyChartLocalGutters()
{
    auto source = atlas();
    auto resolved = inputs();
    source.triangles[0].importedChart = dust3d::SourceUvChart { 0, 0, .6, 1, false };
    source.triangles[0].packedUvs = { dust3d::Vector2(0, 0), dust3d::Vector2(.6, 0), dust3d::Vector2(0, 1) };
    auto adjacent = source.triangles[0];
    adjacent.modelKey = "blue";
    adjacent.importedChart = dust3d::SourceUvChart { .8, 0, .2, 1, false };
    adjacent.packedUvs = { dust3d::Vector2(.8, 0), dust3d::Vector2(1, 1), dust3d::Vector2(.8, 1) };
    source.triangles.push_back(adjacent);
    resolved.triangleColors.push_back(resolved.triangleColors[0]);
    resolved.materials.emplace("blue", dust3d::SourceTextureMaterial {});
    auto& red = resolved.materials.at("model");
    red.baseColor = { 1, 0, 0, 0 };
    red.policy.alphaMode = dust3d::SourceTextureAlphaMode::blend;
    red.metallic = .25;
    red.roughness = .75;
    auto& blue = resolved.materials.at("blue");
    blue.baseColor = { 0, 0, 1, 1 };
    blue.metallic = .75;
    blue.roughness = .25;
    auto settings = options();
    settings.colorSize = settings.materialSize = 16;
    const auto result = dust3d::SourceTextureBaker::bake(source, resolved, settings);
    const size_t redCorner = (15 * 16 + 9) * 4; // Inside red's rectangle, outside its triangle.
    require(result.color.pixels[redCorner] == 255 && result.color.pixels[redCorner + 2] == 0 && result.color.pixels[redCorner + 3] == 0,
        "uncovered red chart corners must retain hidden red RGB and alpha instead of borrowing blue");
    require(result.metallic.pixels[redCorner] == 64 && result.roughness.pixels[redCorner] == 191, "chart-local material gutters");
    require(result.maximumSamplingLod == 0, "base atlas sampling must not enable a global mixed-chart mip chain");
    const size_t redApron = (15 * 16 + 10) * 4;
    require(result.color.pixels[redApron] == 255 && result.color.pixels[redApron + 2] == 0 && result.color.pixels[redApron + 3] == 0, "explicit bilinear collar belongs to its chart");
    auto insufficient = settings;
    insufficient.colorSize = insufficient.materialSize = 4;
    rejects([&] { dust3d::SourceTextureBaker::bake(source, resolved, insufficient); }, dust3d::TextureBakeErrorCode::limitExceeded);
    auto diagonal = source;
    diagonal.triangles[0].importedChart = dust3d::SourceUvChart { 0, 0, .4375, .4375, false };
    diagonal.triangles[0].packedUvs = { dust3d::Vector2(0, 0), dust3d::Vector2(.4375, 0), dust3d::Vector2(0, .4375) };
    diagonal.triangles[1].importedChart = dust3d::SourceUvChart { .5625, 0, .4375, 1, false };
    diagonal.triangles[1].packedUvs = { dust3d::Vector2(.5625, 0), dust3d::Vector2(1, 1), dust3d::Vector2(.5625, 1) };
    std::swap(diagonal.triangles[0], diagonal.triangles[1]); // Blue wins an unreserved Manhattan tie.
    const auto cornerResult = dust3d::SourceTextureBaker::bake(diagonal, resolved, settings);
    const size_t diagonalApron = (7 * 16 + 7) * 4;
    require(cornerResult.color.pixels[diagonalApron] == 255 && cornerResult.color.pixels[diagonalApron + 2] == 0
            && cornerResult.color.pixels[diagonalApron + 3] == 0,
        "diagonal bilinear collar must be reserved independently of flood tie order");
    const size_t blueCorner = 15 * 4; // Inside blue's rectangle, outside its triangle.
    require(result.color.pixels[blueCorner] == 0 && result.color.pixels[blueCorner + 2] == 255, "uncovered blue chart corners must stay blue");
}

void realGeneratedFixture(const char* directory)
{
    std::ifstream input(std::string(directory) + "/ImportedSurface.ds3", std::ios::binary);
    require(bool(input), "missing imported bake fixture");
    const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)), {});
    std::lock_guard<std::mutex> lock(dust3d::native::engineMutex());
    std::string warning;
    dust3d::native::ImportedModels imports;
    auto snapshot = dust3d::native::readDocument(bytes.data(), bytes.size(), warning, nullptr, &imports);
    auto assets = imports.takeAssets();
    dust3d::SourceTextureBakeInputs resolved;
    std::map<std::string, dust3d::MeshGenerator::ImportedModelData> geometry;
    const uint8_t decodedPixel[] { 128, 128, 128, 255 };
    auto sampler = dust3d::ColorTextureSampler::create({ 1, 1, 4, 4, decodedPixel });
    for (auto& [identity, asset] : assets) {
        dust3d::SourceTextureMaterial material;
        material.baseColor = asset.baseColor;
        material.metallic = asset.metallic;
        material.roughness = asset.roughness;
        material.emissive = asset.emissive;
        material.vertexColorEncoding = asset.model.vertexColors.empty() ? dust3d::SourceColorEncoding::srgb : dust3d::SourceColorEncoding::linear;
        material.policy = { asset.alphaMode == dust3d::native::GlbAlphaMode::Opaque ? dust3d::SourceTextureAlphaMode::opaque : asset.alphaMode == dust3d::native::GlbAlphaMode::Mask ? dust3d::SourceTextureAlphaMode::mask
                                                                                                                                                                                     : dust3d::SourceTextureAlphaMode::blend,
            asset.alphaCutoff, asset.isDoubleSided };
        material.colorTexture = &sampler; // Synthetic decoded image isolates the decoder boundary.
        resolved.materials.emplace(identity, material);
        geometry.emplace(identity, std::move(asset.model));
    }
    dust3d::MeshGenerator generator(snapshot.get());
    snapshot.release(); // The inherited constructor takes ownership.
    generator.setImportedModelData(std::move(geometry));
    generator.generate();
    require(generator.isSuccessful(), "real bake fixture generation failed");
    std::unique_ptr<dust3d::Object> object(generator.takeObject());
    require(object && object->triangles.size() == 4, "real bake source triangle count");
    auto source = dust3d::SourceUvAtlasBuilder::build(*object);
    for (size_t index = 0; index < object->triangles.size(); ++index) {
        std::array<dust3d::Color, 3> colors;
        for (size_t corner = 0; corner < 3; ++corner)
            colors[corner] = dust3d::native::sourceCornerColor(*object, index, corner);
        resolved.triangleColors.push_back(colors);
    }
    auto settings = options();
    settings.colorSize = settings.materialSize = 256;
    auto result = dust3d::SourceTextureBaker::bake(source, resolved, settings);
    require(result.trianglePolicies.size() == 4 && result.color.pixels.size() == 256 * 256 * 4, "real generated source bake ownership");
    for (const auto& policy : result.trianglePolicies)
        require(policy && policy->alphaMode == dust3d::SourceTextureAlphaMode::mask, "real generated source policy alignment");
}
}
int main(int argc, char** argv)
{
    try {
        verifyChartLocalGutters();
        linearFactorsAndPolicies();
        fullNonnegativeAlphaCutoffRange();
        authoredRepeatAndLod();
        legacyComposition();
        failures();
        mixedAndInterpolated();
        additionalFailures();
        remainingBoundaries();
        require(argc == 2, "fixture directory argument required");
        realGeneratedFixture(argv[1]);
        std::cout << "Source texture baker contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
