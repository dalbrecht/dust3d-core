// SPDX-License-Identifier: MIT
#include <algorithm>
#include <cmath>
#include <dust3d/uv/color_texture_sampler.h>
#include <limits>

namespace dust3d {
namespace {
    void invalid(const char* message) { throw TextureBakeError(TextureBakeErrorCode::invalidInput, message); }
    void checkCancellation(const ColorTextureSamplingOptions& options)
    {
        if (options.cancellation && options.cancellation->load(std::memory_order_relaxed))
            throw TextureBakeError(TextureBakeErrorCode::cancelled, "Color texture sampling preparation cancelled");
    }
    size_t add(size_t left, size_t right)
    {
        if (right > std::numeric_limits<size_t>::max() - left)
            invalid("Color texture byte span overflow");
        return left + right;
    }
    size_t multiply(size_t left, size_t right)
    {
        if (left && right > std::numeric_limits<size_t>::max() / left)
            invalid("Color texture byte span overflow");
        return left * right;
    }
    void checkQuota(size_t bytes, size_t maximum)
    {
        if (bytes > maximum)
            throw TextureBakeError(TextureBakeErrorCode::limitExceeded, "Color texture retained payload exceeds quota");
    }
    bool validWrap(ColorTextureWrap wrap)
    {
        return wrap == ColorTextureWrap::repeatTexture || wrap == ColorTextureWrap::mirroredRepeat || wrap == ColorTextureWrap::clampToEdge;
    }
    bool usesMips(ColorTextureMinificationFilter filter)
    {
        return filter != ColorTextureMinificationFilter::nearest && filter != ColorTextureMinificationFilter::linear;
    }
    double decode(uint8_t byte)
    {
        double value = byte / 255.0;
        return value <= .04045 ? value / 12.92 : std::pow((value + .055) / 1.055, 2.4);
    }
    uint8_t encode(double value)
    {
        double encoded = value <= .0031308 ? value * 12.92 : 1.055 * std::pow(value, 1.0 / 2.4) - .055;
        return static_cast<uint8_t>(std::round(std::clamp(encoded, 0.0, 1.0) * 255));
    }
    double normalize(double coordinate, ColorTextureWrap wrap)
    {
        if (wrap == ColorTextureWrap::clampToEdge)
            return std::clamp(coordinate, 0.0, 1.0);
        double period = wrap == ColorTextureWrap::mirroredRepeat ? 2 : 1;
        double value = std::fmod(coordinate, period);
        if (value < 0)
            value += period;
        return value > 1 ? 2 - value : value;
    }
    size_t wrapIndex(int index, size_t dimension, ColorTextureWrap wrap)
    {
        int size = static_cast<int>(dimension); // Validated dimensions are at most 8192.
        if (wrap == ColorTextureWrap::clampToEdge)
            return static_cast<size_t>(std::clamp(index, 0, size - 1));
        int period = wrap == ColorTextureWrap::mirroredRepeat ? size * 2 : size;
        int wrapped = index % period;
        if (wrapped < 0)
            wrapped += period;
        return static_cast<size_t>(wrapped >= size ? period - 1 - wrapped : wrapped);
    }
    std::array<double, 4> mix(const std::array<double, 4>& left, const std::array<double, 4>& right, double weight)
    {
        std::array<double, 4> result;
        for (size_t channel = 0; channel < 4; ++channel)
            result[channel] = left[channel] * (1 - weight) + right[channel] * weight;
        return result;
    }
}
ColorTextureSampler ColorTextureSampler::create(const TextureImageView& image, const ColorTextureSamplingOptions& options)
{
    checkCancellation(options);
    if (!image.width || !image.height || image.width > 8192 || image.height > 8192 || !image.pixels)
        invalid("Color texture dimensions or pixels are invalid");
    size_t rowBytes = multiply(image.width, 4);
    if (image.rowStride < rowBytes || image.byteCount < add(multiply(image.height - 1, image.rowStride), rowBytes))
        invalid("Color texture readable storage is too short");
    if (!validWrap(options.wrapS) || !validWrap(options.wrapT)
        || (options.magnification != ColorTextureMagnificationFilter::nearest && options.magnification != ColorTextureMagnificationFilter::linear))
        invalid("Color texture sampler options are invalid");
    switch (options.minification) {
    case ColorTextureMinificationFilter::nearest:
    case ColorTextureMinificationFilter::linear:
    case ColorTextureMinificationFilter::nearestMipmapNearest:
    case ColorTextureMinificationFilter::linearMipmapNearest:
    case ColorTextureMinificationFilter::nearestMipmapLinear:
    case ColorTextureMinificationFilter::linearMipmapLinear:
        break;
    default:
        invalid("Color texture minification filter is invalid");
    }
    if (options.maximumPayloadBytes > 256 * 1024 * 1024)
        invalid("Color texture quota exceeds the supported maximum");
    size_t maximum = options.maximumPayloadBytes ? options.maximumPayloadBytes : 64 * 1024 * 1024;
    size_t levelCount = 0;
    size_t plannedBytes = sizeof(ColorTextureSampler);
    for (size_t width = image.width, height = image.height;; width = std::max<size_t>(1, width / 2), height = std::max<size_t>(1, height / 2)) {
        ++levelCount;
        plannedBytes = add(plannedBytes, add(sizeof(Level), multiply(multiply(width, height), 4)));
        if (!usesMips(options.minification) || (width == 1 && height == 1))
            break;
    }
    checkQuota(plannedBytes, maximum);
    ColorTextureSampler result;
    result.m_wrapS = options.wrapS;
    result.m_wrapT = options.wrapT;
    result.m_magnification = options.magnification;
    result.m_minification = options.minification;
    result.m_levels.reserve(levelCount);
    result.m_payloadBytes = add(sizeof(ColorTextureSampler), multiply(result.m_levels.capacity(), sizeof(Level)));
    for (size_t levelIndex = 0; levelIndex < levelCount; ++levelIndex) {
        checkCancellation(options);
        Level level;
        level.width = levelIndex ? std::max<size_t>(1, result.m_levels.back().width / 2) : image.width;
        level.height = levelIndex ? std::max<size_t>(1, result.m_levels.back().height / 2) : image.height;
        level.pixels.resize(multiply(multiply(level.width, level.height), 4));
        result.m_payloadBytes = add(result.m_payloadBytes, level.pixels.capacity());
        checkQuota(result.m_payloadBytes, maximum);
        for (size_t y = 0; y < level.height; ++y) {
            checkCancellation(options);
            if (!levelIndex) {
                std::copy_n(image.pixels + y * image.rowStride, rowBytes, level.pixels.data() + y * rowBytes);
                continue;
            }
            const auto& source = result.m_levels.back();
            double top = static_cast<double>(y) * source.height / level.height;
            double bottom = static_cast<double>(y + 1) * source.height / level.height;
            for (size_t x = 0; x < level.width; ++x) {
                double left = static_cast<double>(x) * source.width / level.width;
                double right = static_cast<double>(x + 1) * source.width / level.width;
                std::array<double, 4> sum {};
                // Area overlap includes odd trailing rows/columns, unlike a fixed 2x2 truncation.
                for (size_t sourceY = static_cast<size_t>(std::floor(top)); sourceY < static_cast<size_t>(std::ceil(bottom)) && sourceY < source.height; ++sourceY) {
                    double height = std::min(bottom, static_cast<double>(sourceY + 1)) - std::max(top, static_cast<double>(sourceY));
                    for (size_t sourceX = static_cast<size_t>(std::floor(left)); sourceX < static_cast<size_t>(std::ceil(right)) && sourceX < source.width; ++sourceX) {
                        double weight = height * (std::min(right, static_cast<double>(sourceX + 1)) - std::max(left, static_cast<double>(sourceX)));
                        size_t offset = (sourceY * source.width + sourceX) * 4;
                        for (size_t channel = 0; channel < 4; ++channel)
                            sum[channel] += weight * (channel == 3 ? source.pixels[offset + channel] / 255.0 : decode(source.pixels[offset + channel]));
                    }
                }
                double area = (right - left) * (bottom - top);
                size_t offset = (y * level.width + x) * 4;
                for (size_t channel = 0; channel < 4; ++channel)
                    level.pixels[offset + channel] = channel == 3 ? static_cast<uint8_t>(std::round(std::clamp(sum[channel] / area, 0.0, 1.0) * 255)) : encode(sum[channel] / area);
            }
        }
        result.m_levels.push_back(std::move(level));
    }
    checkCancellation(options);
    return result;
}
std::array<double, 4> ColorTextureSampler::sampleLevel(size_t levelIndex, double u, double v, bool linear) const
{
    const auto& level = m_levels[levelIndex];
    double x = normalize(u, m_wrapS) * level.width - (linear ? .5 : 0);
    double y = normalize(v, m_wrapT) * level.height - (linear ? .5 : 0);
    int left = static_cast<int>(std::floor(x));
    int top = static_cast<int>(std::floor(y));
    auto texel = [&](int column, int row) {
        size_t offset = (wrapIndex(row, level.height, m_wrapT) * level.width + wrapIndex(column, level.width, m_wrapS)) * 4;
        return std::array<double, 4> { decode(level.pixels[offset]), decode(level.pixels[offset + 1]), decode(level.pixels[offset + 2]), level.pixels[offset + 3] / 255.0 };
    };
    if (!linear)
        return texel(left, top);
    return mix(mix(texel(left, top), texel(left + 1, top), x - left),
        mix(texel(left, top + 1), texel(left + 1, top + 1), x - left), y - top);
}
std::array<double, 4> ColorTextureSampler::sampleLinear(double u, double v, double lod) const
{
    if (!std::isfinite(u) || !std::isfinite(v) || !std::isfinite(lod))
        invalid("Color texture UV and LOD must be finite");
    if (lod <= 0)
        return sampleLevel(0, u, v, m_magnification == ColorTextureMagnificationFilter::linear);
    bool linear = m_minification == ColorTextureMinificationFilter::linear || m_minification == ColorTextureMinificationFilter::linearMipmapNearest || m_minification == ColorTextureMinificationFilter::linearMipmapLinear;
    if (!usesMips(m_minification))
        return sampleLevel(0, u, v, linear);
    double bounded = std::min(lod, static_cast<double>(m_levels.size() - 1));
    if (m_minification == ColorTextureMinificationFilter::nearestMipmapNearest || m_minification == ColorTextureMinificationFilter::linearMipmapNearest)
        return sampleLevel(static_cast<size_t>(std::round(bounded)), u, v, linear);
    size_t first = static_cast<size_t>(std::floor(bounded));
    size_t second = std::min(first + 1, m_levels.size() - 1);
    return mix(sampleLevel(first, u, v, linear), sampleLevel(second, u, v, linear), bounded - first);
}
}
