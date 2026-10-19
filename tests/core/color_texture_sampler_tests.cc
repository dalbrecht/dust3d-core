// SPDX-License-Identifier: MIT
#include <cmath>
#include <dust3d/uv/color_texture_sampler.h>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
void close(double actual, double expected, const char* message)
{
    require(std::abs(actual - expected) < 0.000001, message);
}
template <typename Operation>
void rejects(Operation operation, dust3d::TextureBakeErrorCode code)
{
    try {
        operation();
    } catch (const dust3d::TextureBakeError& error) {
        require(error.code() == code, "wrong typed error");
        return;
    }
    throw std::runtime_error("expected typed failure");
}
void ownershipAndColor()
{
    std::vector<uint8_t> pixels { 128, 0, 0, 255, 0, 255, 0, 255, 9, 9, 9, 9,
        0, 0, 255, 0, 255, 255, 255, 64 };
    dust3d::ColorTextureSamplingOptions options;
    options.magnification = dust3d::ColorTextureMagnificationFilter::nearest;
    options.minification = dust3d::ColorTextureMinificationFilter::nearest;
    auto sampler = dust3d::ColorTextureSampler::create({ 2, 2, 12, pixels.size(), pixels.data() }, options);
    pixels.clear();
    pixels.shrink_to_fit();
    close(sampler.sampleLinear(.25, .25)[0], .21586050011389926, "decode source sRGB");
    close(sampler.sampleLinear(.75, .25)[1], 1, "top row orientation");
    close(sampler.sampleLinear(.25, .75)[2], 1, "retain hidden RGB");
    close(sampler.sampleLinear(.25, .75)[3], 0, "straight alpha");
    close(sampler.sampleLinear(.75, .75)[3], 64.0 / 255, "linear alpha");
    require(sampler.mipLevelCount() == 1, "no unnecessary mips");

    const uint8_t colors[] { 255, 0, 0, 0, 0, 0, 255, 255 };
    options.magnification = dust3d::ColorTextureMagnificationFilter::linear;
    auto linear = dust3d::ColorTextureSampler::create({ 2, 1, 8, 8, colors }, options);
    auto middle = linear.sampleLinear(.5, .5);
    close(middle[0], .5, "linear red independent of alpha");
    close(middle[2], .5, "linear blue");
    close(middle[3], .5, "linear alpha interpolation");
    const uint8_t gray[] { 128, 128, 128, 255, 0, 0, 0, 255 };
    auto graySampler = dust3d::ColorTextureSampler::create({ 2, 1, 8, 8, gray }, options);
    close(graySampler.sampleLinear(.5, .5)[0], .10793025005694963, "decode before filtering");
}
void wrappingAndFilters()
{
    const uint8_t colors[] { 0, 0, 0, 255, 255, 255, 255, 255 };
    dust3d::ColorTextureSamplingOptions options;
    options.magnification = dust3d::ColorTextureMagnificationFilter::nearest;
    options.minification = dust3d::ColorTextureMinificationFilter::nearest;
    for (auto wrap : { dust3d::ColorTextureWrap::repeatTexture, dust3d::ColorTextureWrap::mirroredRepeat, dust3d::ColorTextureWrap::clampToEdge }) {
        options.wrapS = options.wrapT = wrap;
        auto sampler = dust3d::ColorTextureSampler::create({ 2, 1, 8, 8, colors }, options);
        auto vertical = dust3d::ColorTextureSampler::create({ 1, 2, 4, 8, colors }, options);
        close(vertical.sampleLinear(.5, -.25)[0], wrap == dust3d::ColorTextureWrap::repeatTexture ? 1 : 0, "negative vertical wrap");
        close(vertical.sampleLinear(.5, 1.25)[0], wrap == dust3d::ColorTextureWrap::repeatTexture ? 0 : 1, "positive vertical wrap");
        close(sampler.sampleLinear(-.25, -.25)[0], wrap == dust3d::ColorTextureWrap::repeatTexture ? 1 : 0, "negative wrap");
        close(sampler.sampleLinear(1.25, 1.25)[0], wrap == dust3d::ColorTextureWrap::repeatTexture ? 0 : 1, "positive wrap");
        require(std::isfinite(sampler.sampleLinear(std::numeric_limits<double>::max(), -std::numeric_limits<double>::max())[0]), "huge finite UV");
    }
    options.wrapS = options.wrapT = dust3d::ColorTextureWrap::repeatTexture;
    options.magnification = dust3d::ColorTextureMagnificationFilter::linear;
    close(dust3d::ColorTextureSampler::create({ 2, 1, 8, 8, colors }, options).sampleLinear(0, 0)[0], .5, "repeat bilinear seam");
    options.wrapS = dust3d::ColorTextureWrap::clampToEdge;
    close(dust3d::ColorTextureSampler::create({ 2, 1, 8, 8, colors }, options).sampleLinear(0, 0)[0], 0, "clamped bilinear edge");
    options.wrapS = dust3d::ColorTextureWrap::mirroredRepeat;
    close(dust3d::ColorTextureSampler::create({ 2, 1, 8, 8, colors }, options).sampleLinear(0, 0)[0], 0, "mirrored bilinear edge");
    options.magnification = dust3d::ColorTextureMagnificationFilter::nearest;
    const double mip = .5028864580325687;
    const double expected[] { 1, .5, 1, .5, .75 + .25 * mip, .375 + .25 * mip };
    size_t index = 0;
    for (auto filter : { dust3d::ColorTextureMinificationFilter::nearest, dust3d::ColorTextureMinificationFilter::linear,
             dust3d::ColorTextureMinificationFilter::nearestMipmapNearest, dust3d::ColorTextureMinificationFilter::linearMipmapNearest,
             dust3d::ColorTextureMinificationFilter::nearestMipmapLinear, dust3d::ColorTextureMinificationFilter::linearMipmapLinear }) {
        options.minification = filter;
        auto sampler = dust3d::ColorTextureSampler::create({ 2, 1, 8, 8, colors }, options);
        close(sampler.sampleLinear(.5, .5, .25)[0], expected[index], "minification policy");
        close(sampler.sampleLinear(.5, .5, -1)[0], 1, "magnification selected for negative LOD");
        if (index >= 2) {
            close(sampler.sampleLinear(.5, .5, std::numeric_limits<double>::max())[0], mip, "bounded high LOD");
            require(sampler.mipLevelCount() == 2, "mip chain");
            if (index == 2 || index == 3)
                close(sampler.sampleLinear(.5, .5, .75)[0], mip, "nearest mip rounds upward");
        }
        ++index;
    }
}
void oddMipsAndLimits()
{
    const uint8_t colors[] { 255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255 };
    for (bool vertical : { false, true }) {
        auto sampler = dust3d::ColorTextureSampler::create({ vertical ? 1u : 3u, vertical ? 3u : 1u, vertical ? 4u : 12u, 12, colors });
        auto sample = sampler.sampleLinear(.5, .5, 1);
        for (size_t channel = 0; channel < 3; ++channel)
            require(std::abs(sample[channel] - 1.0 / 3) < .004, "odd mip must include every texel");
    }
    std::vector<uint8_t> constant(2 * 4 * 4, 128);
    for (size_t index = 3; index < constant.size(); index += 4)
        constant[index] = 255;
    auto nonSquare = dust3d::ColorTextureSampler::create({ 2, 4, 8, constant.size(), constant.data() });
    require(nonSquare.mipLevelCount() == 3, "non-square complete mip chain");
    close(nonSquare.sampleLinear(.3, .7, 2)[0], .21586050011389926, "constant color survives multiple mips");
    const uint8_t pixel[] { 128, 128, 128, 255 };
    dust3d::TextureImageView view { 1, 1, 4, 4, pixel };
    auto sampler = dust3d::ColorTextureSampler::create(view);
    auto independent = [&] {
        std::atomic_bool temporaryCancellation { false };
        dust3d::ColorTextureSamplingOptions temporaryOptions;
        temporaryOptions.cancellation = &temporaryCancellation;
        return dust3d::ColorTextureSampler::create(view, temporaryOptions);
    }();
    close(independent.sampleLinear(.5, .5)[0], .21586050011389926, "outlive cancellation flag storage");
    dust3d::ColorTextureSamplingOptions options;
    options.maximumPayloadBytes = sampler.payloadBytes();
    require(dust3d::ColorTextureSampler::create(view, options).payloadBytes() == sampler.payloadBytes(), "exact quota");
    --options.maximumPayloadBytes;
    rejects([&] { dust3d::ColorTextureSampler::create(view, options); }, dust3d::TextureBakeErrorCode::limitExceeded);
    options.maximumPayloadBytes = 256 * 1024 * 1024 + 1;
    rejects([&] { dust3d::ColorTextureSampler::create(view, options); }, dust3d::TextureBakeErrorCode::invalidInput);
    options = {};
    std::atomic_bool cancellation { true };
    options.cancellation = &cancellation;
    rejects([&] { dust3d::ColorTextureSampler::create(view, options); }, dust3d::TextureBakeErrorCode::cancelled);
    cancellation = false;
    auto owned = dust3d::ColorTextureSampler::create(view, options);
    cancellation = true;
    close(owned.sampleLinear(0, 0)[0], .21586050011389926, "no retained cancellation reference");
    for (auto invalid : { dust3d::TextureImageView { 0, 1, 4, 4, pixel }, { 1, 0, 4, 4, pixel }, { 8193, 1, 4, 4, pixel },
             { 1, 1, 3, 4, pixel }, { 1, 1, 4, 3, pixel }, { 1, 1, 4, 4, nullptr },
             { 1, 2, std::numeric_limits<size_t>::max(), 4, pixel }, { 1, 3, std::numeric_limits<size_t>::max(), 4, pixel } })
        rejects([&] { dust3d::ColorTextureSampler::create(invalid); }, dust3d::TextureBakeErrorCode::invalidInput);
    for (double invalid : { std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN() }) {
        rejects([&] { sampler.sampleLinear(invalid, 0); }, dust3d::TextureBakeErrorCode::invalidInput);
        rejects([&] { sampler.sampleLinear(0, invalid); }, dust3d::TextureBakeErrorCode::invalidInput);
        rejects([&] { sampler.sampleLinear(0, 0, invalid); }, dust3d::TextureBakeErrorCode::invalidInput);
    }
    options = {};
    options.wrapS = static_cast<dust3d::ColorTextureWrap>(99);
    rejects([&] { dust3d::ColorTextureSampler::create(view, options); }, dust3d::TextureBakeErrorCode::invalidInput);
    options = {};
    options.wrapT = static_cast<dust3d::ColorTextureWrap>(99);
    rejects([&] { dust3d::ColorTextureSampler::create(view, options); }, dust3d::TextureBakeErrorCode::invalidInput);
    options = {};
    options.magnification = static_cast<dust3d::ColorTextureMagnificationFilter>(99);
    rejects([&] { dust3d::ColorTextureSampler::create(view, options); }, dust3d::TextureBakeErrorCode::invalidInput);
    options = {};
    options.minification = static_cast<dust3d::ColorTextureMinificationFilter>(99);
    rejects([&] { dust3d::ColorTextureSampler::create(view, options); }, dust3d::TextureBakeErrorCode::invalidInput);
}
}
int main()
{
    try {
        ownershipAndColor();
        wrappingAndFilters();
        oddMipsAndLimits();
        std::cout << "Color texture sampler contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
