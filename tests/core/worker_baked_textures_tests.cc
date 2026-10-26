// SPDX-License-Identifier: MIT
#include <cmath>
#include <iostream>
#include <limits>
#include <native/Engine/WorkerBakedTextureViews.h>
#include <native/Engine/WorkerBakedTextures.h>
#include <native/Engine/WorkerWire.h>
#include <stdexcept>
using namespace dust3d::worker;
namespace {
void require(bool condition, const char* cause)
{
    if (!condition)
        throw std::runtime_error(cause);
}
template <typename Action>
void reject(BakedTexturesErrorCode code, Action action)
{
    try {
        action();
    } catch (const BakedTexturesError& cause) {
        if (cause.code() != code)
            std::cerr << "Expected " << int(code) << " got " << int(cause.code()) << ": " << cause.what() << '\n';
        require(cause.code() == code && cause.what()[0], "Texture rejection preserves typed original cause");
        return;
    }
    throw std::runtime_error("Invalid baked textures accepted");
}
TextureLevel level(uint32_t width, uint32_t height, uint8_t seed)
{
    TextureLevel result { width, height, {} };
    for (size_t index = 0; index < size_t(width) * height * 4; ++index)
        result.pixels.push_back(static_cast<uint8_t>(seed + index));
    result.pixels.at(3) = 0;
    return result;
}
BakedTexturesPayload sample()
{
    BakedTexturesPayload value;
    for (size_t role = 0; role < value.maps.size(); ++role)
        value.maps[role] = { level(4, 2, uint8_t(role + 1)), level(2, 1, uint8_t(role + 20)), level(1, 1, 60) };
    value.maximumSamplingLod = 0.75;
    return value;
}
void set(std::vector<uint8_t>& bytes, size_t offset, uint64_t value, size_t width)
{
    require(offset <= bytes.size() && width <= bytes.size() - offset, "Wire test mutation is bounded");
    detail::writeUnsigned(bytes.data() + offset, value, width);
}
void roundTrip()
{
    auto value = sample();
    auto wire = encodeBakedTexturesPayload(value, {});
    const auto expected = wire;
    const auto summary = inspectBakedTexturesPayload(wire.data(), wire.size(), {});
    require(summary.levelCount == 15 && summary.pixelBytes == 220 && summary.logicalBytes == 15 * sizeof(TextureLevel) + 220, "All roles/levels have exact retained charges");
    BakedTexturesLimits exact;
    exact.maximumWireBytes = wire.size();
    exact.maximumDecodedBytes = summary.logicalBytes;
    exact.maximumPixelBytes = summary.pixelBytes;
    auto owned = decodeBakedTexturesPayload(wire.data(), wire.size(), exact);
    require(encodeBakedTexturesPayload(owned, exact) == expected && owned.maximumSamplingLod == 0.75, "Bytes and producer's fractional chart cap survive exact quotas");
    require(owned.maps[0][0].pixels[0] == 1 && owned.maps[0][0].pixels[3] == 0, "Hidden RGB under zero alpha is preserved");
    wire.assign(wire.size(), 0);
    value = {};
    require(encodeBakedTexturesPayload(owned, {}) == expected, "Decoded pixels own storage after input release");
    require(expected[1] == 1 && expected[3] == 64 && expected[28] == 0 && expected[31] == 15, "Version, prefix and counts use explicit big endian");
    for (int quota = 0; quota < 3; ++quota) {
        auto limits = exact;
        if (quota == 0)
            --limits.maximumWireBytes;
        if (quota == 1)
            --limits.maximumDecodedBytes;
        if (quota == 2)
            --limits.maximumPixelBytes;
        reject(BakedTexturesErrorCode::limitExceeded, [&] { encodeBakedTexturesPayload(owned, limits); });
        reject(BakedTexturesErrorCode::limitExceeded, [&] { decodeBakedTexturesPayload(expected.data(), expected.size(), limits); });
    }
    BakedTexturesLimits tiny;
    tiny.maximumWireBytes = 1;
    reject(BakedTexturesErrorCode::limitExceeded, [&] { encodeBakedTexturesPayload(owned, tiny); });
    reject(BakedTexturesErrorCode::limitExceeded, [&] { decodeBakedTexturesPayload(expected.data(), expected.size(), tiny); });
    tiny = {};
    tiny.maximumDecodedBytes = 1;
    reject(BakedTexturesErrorCode::limitExceeded, [&] { encodeBakedTexturesPayload(owned, tiny); });
    BakedTexturesPayload skinny;
    skinny.maps[0] = { level(1, 4, 1), level(1, 2, 2), level(1, 1, 3) };
    skinny.maximumSamplingLod = 2;
    const auto skinnyWire = encodeBakedTexturesPayload(skinny, {});
    require(decodeBakedTexturesPayload(skinnyWire.data(), skinnyWire.size(), {}).maps[0].size() == 3, "A one-pixel width still permits shrinking height");
    BakedTexturesPayload sparse;
    sparse.maps[0] = { level(3, 1, 17), level(1, 1, 18) };
    sparse.maximumSamplingLod = 1;
    auto sparseWire = encodeBakedTexturesPayload(sparse, {});
    auto copy = decodeBakedTexturesPayload(sparseWire.data(), sparseWire.size(), {});
    require(copy.maps[1].empty() && copy.maps[0][1].width == 1 && copy.maximumSamplingLod == 1, "Optional absent roles and odd halved dimensions are exact");
    sparse.maps[0].resize(1);
    sparse.maximumSamplingLod = -0.0;
    sparseWire = encodeBakedTexturesPayload(sparse, {});
    require(std::signbit(decodeBakedTexturesPayload(sparseWire.data(), sparseWire.size(), {}).maximumSamplingLod), "Valid negative zero cap retains its bits");
}
void values()
{
    const auto original = sample();
    auto invalid = [&](BakedTexturesPayload value, BakedTexturesErrorCode code) { reject(code, [&] { encodeBakedTexturesPayload(value, {}); }); };
    auto value = original;
    value.maps[0].clear();
    invalid(value, BakedTexturesErrorCode::invalidLayout);
    value = original;
    value.maps[1].resize(15);
    invalid(value, BakedTexturesErrorCode::invalidLayout);
    for (auto dimension : { uint32_t(0), uint32_t(8193), UINT32_MAX }) {
        value = original;
        value.maps[0][0].width = dimension;
        invalid(value, BakedTexturesErrorCode::invalidImage);
        value = original;
        value.maps[0][0].height = dimension;
        invalid(value, BakedTexturesErrorCode::invalidImage);
    }
    value = original;
    value.maps[0][0].pixels.pop_back();
    invalid(value, BakedTexturesErrorCode::invalidImage);
    value = original;
    value.maps[0][0].pixels.push_back(0);
    invalid(value, BakedTexturesErrorCode::invalidImage);
    value = original;
    value.maps[0][1] = level(1, 1, 0);
    invalid(value, BakedTexturesErrorCode::invalidChain);
    value = original;
    value.maps[0][1] = level(2, 2, 0);
    invalid(value, BakedTexturesErrorCode::invalidChain);
    value = original;
    value.maps[0].push_back(level(1, 1, 0));
    invalid(value, BakedTexturesErrorCode::invalidChain);
    for (double cap : { -0.1, 2.01, std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN() }) {
        value = original;
        value.maximumSamplingLod = cap;
        invalid(value, BakedTexturesErrorCode::invalidSamplingLod);
    }
    value = original;
    value.maps[4].resize(1);
    invalid(value, BakedTexturesErrorCode::invalidSamplingLod);
    value.maximumSamplingLod = 0;
    require(!encodeBakedTexturesPayload(value, {}).empty(), "Unequal present chains allow only shared safe cap");
    const auto wire = encodeBakedTexturesPayload(original, {});
    for (int field = 0; field < 3; ++field)
        for (uint64_t amount : { uint64_t(0), maximumPayloadBytes + 1 }) {
            BakedTexturesLimits limits;
            if (field == 0)
                limits.maximumWireBytes = amount;
            if (field == 1)
                limits.maximumDecodedBytes = amount;
            if (field == 2)
                limits.maximumPixelBytes = amount;
            reject(BakedTexturesErrorCode::invalidLimits, [&] { validateBakedTexturesLimits(limits); });
            reject(BakedTexturesErrorCode::invalidLimits, [&] { encodeBakedTexturesPayload(original, limits); });
            reject(BakedTexturesErrorCode::invalidLimits, [&] { inspectBakedTexturesPayload(wire.data(), wire.size(), limits); });
        }
}
void borrowedViews()
{
    auto value = sample();
    detail::BakedTextureViews views;
    views.maximumSamplingLod = value.maximumSamplingLod;
    for (size_t role = 0; role < value.maps.size(); ++role) {
        views.counts[role] = uint32_t(value.maps[role].size());
        for (size_t level = 0; level < value.maps[role].size(); ++level) {
            const auto& image = value.maps[role][level];
            views.maps[role][level] = { image.width, image.height, image.pixels.size(), image.pixels.data() };
        }
    }
    const auto wire = encodeBakedTexturesPayload(value, {});
    const auto shape = detail::inspectBakedTextureViews(views, {});
    require(shape.wireBytes == wire.size() && shape.logicalBytes == inspectBakedTexturesPayload(wire.data(), wire.size(), {}).logicalBytes, "Borrowed and wire rules share exact measures");
    auto owned = detail::copyBakedTextureViews(views, {});
    value.maps[0][0].pixels.assign(32, 0);
    require(encodeBakedTexturesPayload(owned, {}) == wire, "Borrowed views are copied independently");
    auto invalid = views;
    invalid.maps[0][0].pixels = nullptr;
    reject(BakedTexturesErrorCode::invalidImage, [&] { detail::copyBakedTextureViews(invalid, {}); });
    invalid = views;
    invalid.counts[0] = 15;
    reject(BakedTexturesErrorCode::invalidLayout, [&] { detail::inspectBakedTextureViews(invalid, {}); });
    invalid = views;
    invalid.counts[0] = 0;
    reject(BakedTexturesErrorCode::invalidLayout, [&] { detail::inspectBakedTextureViews(invalid, {}); });
}
void framing()
{
    auto wire = encodeBakedTexturesPayload(sample(), {});
    reject(BakedTexturesErrorCode::invalidInput, [&] { decodeBakedTexturesPayload(nullptr, wire.size(), {}); });
    for (size_t size = 0; size < wire.size(); ++size)
        reject(BakedTexturesErrorCode::truncated, [&] { inspectBakedTexturesPayload(wire.data(), size, {}); });
    auto changed = wire;
    changed.push_back(0);
    reject(BakedTexturesErrorCode::trailingData, [&] { decodeBakedTexturesPayload(changed.data(), changed.size(), {}); });
    auto mutation = [&](size_t offset, uint64_t value, size_t width, BakedTexturesErrorCode code) {
        auto bytes = wire;
        set(bytes, offset, value, width);
        reject(code, [&] { decodeBakedTexturesPayload(bytes.data(), bytes.size(), {}); });
    };
    mutation(0, 2, 2, BakedTexturesErrorCode::unsupportedVersion);
    for (auto pair : { std::pair<size_t, size_t>(2, 2), { 4, 4 }, { 24, 4 }, { 52, 4 }, { 56, 8 } })
        mutation(pair.first, 1, pair.second, BakedTexturesErrorCode::invalidLayout);
    mutation(28, 14, 4, BakedTexturesErrorCode::invalidLayout);
    mutation(32, 0, 4, BakedTexturesErrorCode::invalidLayout);
    mutation(36, 15, 4, BakedTexturesErrorCode::invalidLayout);
    mutation(8, 219, 8, BakedTexturesErrorCode::invalidLayout);
    mutation(8, UINT64_MAX, 8, BakedTexturesErrorCode::limitExceeded);
    for (uint64_t bits : { 0x7ff0000000000000ULL, 0xfff0000000000000ULL, 0x7ff0000000000001ULL, 0xbff0000000000000ULL, 0x4008000000000000ULL })
        mutation(16, bits, 8, BakedTexturesErrorCode::invalidSamplingLod);
    mutation(64, 0, 4, BakedTexturesErrorCode::invalidImage);
    mutation(68, 8193, 4, BakedTexturesErrorCode::invalidImage);
    mutation(72, 31, 8, BakedTexturesErrorCode::invalidImage);
    mutation(112, 1, 4, BakedTexturesErrorCode::invalidImage);
    mutation(116, 2, 4, BakedTexturesErrorCode::invalidImage);
    changed = wire;
    set(changed, 112, 1, 4);
    set(changed, 120, 4, 8);
    reject(BakedTexturesErrorCode::invalidChain, [&] { decodeBakedTexturesPayload(changed.data(), changed.size(), {}); });
}
}
int main()
{
    try {
        roundTrip();
        values();
        borrowedViews();
        framing();
        std::cout << "Baked texture worker contracts passed\n";
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}
