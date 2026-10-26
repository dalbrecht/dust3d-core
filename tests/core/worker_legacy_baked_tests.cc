// SPDX-License-Identifier: MIT
#include <cmath>
#include <iostream>
#include <limits>
#include <native/Engine/WorkerLegacyBaked.h>
#include <native/Engine/WorkerUvValidation.h>
#include <native/Engine/WorkerWire.h>
#include <stdexcept>
using namespace dust3d::worker;
namespace {
void require(bool value, const char* cause)
{
    if (!value)
        throw std::runtime_error(cause);
}
template <typename Error = LegacyBakedError, typename Code, typename Action>
void reject(Code code, Action action)
{
    try {
        action();
    } catch (const Error& cause) {
        if (cause.code() != code)
            std::cerr << cause.what() << '\n';
        require(cause.code() == code && cause.what()[0], "Legacy rejection preserves original typed cause");
        return;
    }
    throw std::runtime_error("Invalid legacy bake accepted");
}
LegacyBakedPayload sample()
{
    LegacyBakedPayload value;
    value.geometry.vertices = { { { -0.0F, 0, 0 }, { 0, 0, 1 }, { 1, 0, 0, 1 } },
        { { 1, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0, 1 } }, { { 0, 1, 0 }, { 0, 0, 1 }, { 0, 0, 1, 1 } } };
    const auto first = value.geometry.vertices;
    value.geometry.vertices.insert(value.geometry.vertices.end(), first.begin(), first.end());
    value.geometry.nodeCount = 3;
    value.geometry.partCount = 1;
    value.geometry.warning = "original κ";
    UvTriangle mapped;
    mapped.uv = { { { -0.0, 0 }, { 1, 0 }, { 0, 1 } } };
    mapped.known = 1;
    mapped.association = UvAssociation::component;
    mapped.componentIndex = 0;
    value.uv.triangles = { mapped, UvTriangle {} };
    value.uv.components = { "original-key" };
    value.uv.packedTextureSize = 512;
    UvChart chart;
    chart.width = chart.height = 1;
    chart.color = { 1, 0.5, 0, 1 };
    chart.flipped = 1;
    chart.association = UvAssociation::component;
    chart.componentIndex = 0;
    chart.imageID = "{eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee}";
    value.uv.charts = { chart };
    UvGradient gradient;
    gradient.largeColor = { 1, 0, 0, 1 };
    gradient.smallColor = { 0, 0, 1, 0.5 };
    gradient.resolution = 64;
    value.uv.gradients = { gradient };
    value.tangents = { { { -0.0F, 1, 0 }, -1, 1 }, { { 0, 0, 0 }, 1, 0 } };
    value.textures.maps[0] = { { 1, 1, { 17, 33, 49, 0 } } };
    value.textures.maps[1] = { { 1, 1, { 128, 128, 255, 255 } } };
    return value;
}
void set(std::vector<uint8_t>& bytes, size_t offset, uint64_t value, size_t width)
{
    require(offset <= bytes.size() && width <= bytes.size() - offset, "Pure legacy wire mutation stays bounded");
    detail::writeUnsigned(bytes.data() + offset, value, width);
}
void roundTrip()
{
    auto value = sample();
    auto wire = encodeLegacyBakedPayload(value, {});
    const auto expected = wire;
    const auto shape = inspectLegacyBakedPayload(wire.data(), wire.size(), {});
    require(shape.vertexCount == 6 && shape.triangleCount == 2, "Legacy geometry and per-triangle tangents retain exact counts");
    LegacyBakedLimits exact;
    exact.maximumWireBytes = wire.size();
    exact.maximumDecodedBytes = shape.logicalBytes;
    exact.maximumTangents = 2;
    auto copy = decodeLegacyBakedPayload(wire.data(), wire.size(), exact);
    require(encodeLegacyBakedPayload(copy, exact) == expected, "Entire coherent bake round trips at exact aggregate quotas");
    require(copy.uv.components == value.uv.components && copy.uv.charts[0].imageID == value.uv.charts[0].imageID
            && copy.uv.gradients[0].resolution == 64 && copy.uv.gradients[0].smallColor[3] == 0.5,
        "Charts, gradients and image/component keys survive");
    require(std::signbit(copy.tangents[0].direction[0]) && copy.tangents[0].handedness == -1 && !copy.tangents[1].known,
        "Tangent bits, handedness and unknown mapping remain exact");
    value = {};
    wire.assign(wire.size(), 0);
    require(encodeLegacyBakedPayload(copy, {}) == expected && copy.textures.maps[0][0].pixels[3] == 0,
        "Owned metadata and transparent RGB pixels outlive input storage");
    for (int field = 0; field < 3; ++field) {
        auto limits = exact;
        if (field == 0)
            --limits.maximumWireBytes;
        if (field == 1)
            --limits.maximumDecodedBytes;
        if (field == 2)
            --limits.maximumTangents;
        reject(LegacyBakedErrorCode::limitExceeded, [&] { encodeLegacyBakedPayload(copy, limits); });
        reject(LegacyBakedErrorCode::limitExceeded, [&] { decodeLegacyBakedPayload(expected.data(), expected.size(), limits); });
    }
    LegacyBakedLimits tiny;
    tiny.maximumWireBytes = 1;
    reject(LegacyBakedErrorCode::limitExceeded, [&] { encodeLegacyBakedPayload(copy, tiny); });
    reject(LegacyBakedErrorCode::limitExceeded, [&] { decodeLegacyBakedPayload(expected.data(), expected.size(), tiny); });
    copy.textures.maps[1].clear();
    require(!encodeLegacyBakedPayload(copy, {}).empty(), "Optional normal and scalar maps remain explicit");
}
void values()
{
    const auto original = sample();
    auto invalid = [&](LegacyBakedPayload value, LegacyBakedErrorCode code) { reject(code, [&] { encodeLegacyBakedPayload(value, {}); }); };
    auto value = original;
    value.tangents.clear();
    invalid(value, LegacyBakedErrorCode::countMismatch);
    value = original;
    value.tangents.pop_back();
    invalid(value, LegacyBakedErrorCode::countMismatch);
    value = original;
    value.tangents[0].known = 2;
    invalid(value, LegacyBakedErrorCode::invalidTangent);
    value = original;
    value.tangents[0].handedness = 0;
    invalid(value, LegacyBakedErrorCode::invalidTangent);
    value = original;
    value.tangents[0].direction = { 0, 2, 0 };
    invalid(value, LegacyBakedErrorCode::invalidTangent);
    for (float coordinate : { std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::max() }) {
        value = original;
        value.tangents[0].direction[0] = coordinate;
        invalid(value, LegacyBakedErrorCode::invalidTangent);
    }
    value = original;
    value.tangents[1] = { { 0, 1, 0 }, 1, 1 };
    invalid(value, LegacyBakedErrorCode::invalidTangent);
    value = original;
    value.uv.triangles[0].known = 0;
    value.uv.triangles[0].uv = {};
    invalid(value, LegacyBakedErrorCode::invalidTangent);
    value = original;
    value.textures.maps[0] = { { 2, 2, std::vector<uint8_t>(16, 0) }, { 1, 1, { 1, 2, 3, 4 } } };
    invalid(value, LegacyBakedErrorCode::invalidTextures);
    value = original;
    value.geometry.vertices[0].color[0] = 2;
    reject<MeshPayloadError>(MeshPayloadErrorCode::invalidGeometry, [&] { encodeLegacyBakedPayload(value, {}); });
    value = original;
    value.uv.gradients[0].chartIndex = 2;
    reject<UvPayloadError>(UvPayloadErrorCode::invalidMetadata, [&] { encodeLegacyBakedPayload(value, {}); });
    value = original;
    value.textures.maps[0][0].pixels.pop_back();
    reject<BakedTexturesError>(BakedTexturesErrorCode::invalidImage, [&] { encodeLegacyBakedPayload(value, {}); });
    for (int field = 0; field < 3; ++field)
        for (uint64_t amount : { uint64_t(0), maximumPayloadBytes + 1 }) {
            LegacyBakedLimits limits;
            if (field == 0)
                limits.maximumWireBytes = amount;
            if (field == 1)
                limits.maximumDecodedBytes = amount;
            if (field == 2)
                limits.maximumTangents = amount;
            reject(LegacyBakedErrorCode::invalidLimits, [&] { validateLegacyBakedLimits(limits); });
        }
}
void sharedPreflightRules()
{
    const auto value = sample();
    detail::validateUvTriangle(value.uv.triangles[0], 1, 1);
    detail::validateUvChart(value.uv.charts[0], 1);
    detail::validateUvGradient(value.uv.gradients[0], 1);
    detail::validateUvTextureSize(value.uv.packedTextureSize, 1);
    std::unordered_set<std::string_view> keys;
    detail::validateUvComponentKey(value.uv.components[0], {}, keys);
    reject<UvPayloadError>(UvPayloadErrorCode::invalidMetadata, [&] { detail::validateUvComponentKey(value.uv.components[0], {}, keys); });
    auto gradient = value.uv.gradients[0];
    gradient.chartIndex = 1;
    reject<UvPayloadError>(UvPayloadErrorCode::invalidMetadata, [&] { detail::validateUvGradient(gradient, 1); });
}
void framing()
{
    const auto wire = encodeLegacyBakedPayload(sample(), {});
    reject(LegacyBakedErrorCode::invalidInput, [&] { decodeLegacyBakedPayload(nullptr, wire.size(), {}); });
    for (size_t size = 0; size < wire.size(); ++size)
        reject(LegacyBakedErrorCode::truncated, [&] { inspectLegacyBakedPayload(wire.data(), size, {}); });
    auto trailing = wire;
    trailing.push_back(0);
    reject(LegacyBakedErrorCode::trailingData, [&] { decodeLegacyBakedPayload(trailing.data(), trailing.size(), {}); });
    auto mutation = [&](size_t offset, uint64_t value, size_t width, LegacyBakedErrorCode code) {
        auto bytes = wire;
        set(bytes, offset, value, width);
        reject(code, [&] { decodeLegacyBakedPayload(bytes.data(), bytes.size(), {}); });
    };
    mutation(0, 2, 2, LegacyBakedErrorCode::unsupportedVersion);
    mutation(2, 1, 2, LegacyBakedErrorCode::invalidLayout);
    mutation(4, 1, 4, LegacyBakedErrorCode::invalidLayout);
    mutation(40, 20, 4, LegacyBakedErrorCode::invalidLayout);
    mutation(44, 1, 4, LegacyBakedErrorCode::invalidLayout);
    mutation(8, UINT64_MAX, 8, LegacyBakedErrorCode::limitExceeded);
    mutation(24, 0, 8, LegacyBakedErrorCode::countMismatch);
    mutation(24, UINT64_MAX, 8, LegacyBakedErrorCode::limitExceeded);
    mutation(8, meshPrefixBytes - 1, 8, LegacyBakedErrorCode::invalidLayout);
    mutation(16, uvPrefixBytes - 1, 8, LegacyBakedErrorCode::invalidLayout);
    mutation(32, bakedTexturesPrefixBytes - 1, 8, LegacyBakedErrorCode::invalidLayout);
    const size_t tangents = 48 + detail::readUnsigned(wire.data() + 8, 8) + detail::readUnsigned(wire.data() + 16, 8);
    mutation(tangents + 16, 2, 1, LegacyBakedErrorCode::invalidTangent);
    for (size_t byte = 17; byte < 24; ++byte)
        mutation(tangents + byte, 1, 1, LegacyBakedErrorCode::invalidLayout);
    auto mipValue = sample().textures;
    mipValue.maps[0] = { { 2, 2, std::vector<uint8_t>(16, 0) }, { 1, 1, { 1, 2, 3, 4 } } };
    const auto mipBytes = encodeBakedTexturesPayload(mipValue, {});
    auto withMips = wire;
    const size_t textures = tangents + 48;
    withMips.resize(textures);
    withMips.insert(withMips.end(), mipBytes.begin(), mipBytes.end());
    set(withMips, 32, mipBytes.size(), 8);
    reject(LegacyBakedErrorCode::invalidTextures, [&] { decodeLegacyBakedPayload(withMips.data(), withMips.size(), {}); });
    auto mismatch = wire;
    set(mismatch, 24, 1, 8);
    mismatch.erase(mismatch.begin() + tangents + 24, mismatch.begin() + tangents + 48);
    reject(LegacyBakedErrorCode::countMismatch, [&] { decodeLegacyBakedPayload(mismatch.data(), mismatch.size(), {}); });
}
}
int main()
{
    try {
        roundTrip();
        values();
        sharedPreflightRules();
        framing();
        std::cout << "Legacy baked worker contracts passed\n";
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}
