// SPDX-License-Identifier: MIT
#include <cmath>
#include <iostream>
#include <limits>
#include <native/Engine/WorkerSourceBaked.h>
#include <native/Engine/WorkerUtf8.h>
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
void reject(SourceBakedErrorCode code, Action action)
{
    try {
        action();
    } catch (const SourceBakedError& cause) {
        if (cause.code() != code)
            std::cerr << int(cause.code()) << ": " << cause.what() << '\n';
        require(cause.code() == code && cause.what()[0], "Original baked rejection code/cause survive");
        return;
    }
    throw std::runtime_error("Invalid source-baked value accepted");
}
SourceBakedPayload sample()
{
    SourceBakedPayload value;
    value.geometry.vertices = { { { -0.0F, 0, 0 }, { 0, 0, 1 }, { 1, 1, 1, 1 } },
        { { 1, 0, 0 }, { 0, 0, 1 }, { 1, 1, 1, 1 } }, { { 0, 1, 0 }, { 0, 0, 1 }, { 1, 1, 1, 1 } } };
    value.geometry.nodeCount = 3;
    value.geometry.partCount = 1;
    value.geometry.warning = "original κ";
    value.surface.models = { "é", "é" };
    value.surface.components = { { "Original", 1 }, { "Generated", 2 } };
    BakedSurfaceTriangle triangle;
    triangle.uv = { { { -0.0, 0 }, { 1, 0 }, { 0, 1 } } };
    triangle.uvKnown = triangle.policyKnown = triangle.tangentKnown = triangle.isDoubleSided = 1;
    triangle.alphaMode = 2;
    triangle.alphaCutoff = 1.5;
    triangle.tangent = { -0.0F, 1, 0 };
    triangle.handedness = -1;
    triangle.modelIndex = 1;
    triangle.componentIndex = 0;
    value.surface.triangles = { triangle };
    value.textures.maps[0] = { { 2, 2, std::vector<uint8_t>(16, 17) }, { 1, 1, { 1, 2, 3, 0 } } };
    value.textures.maximumSamplingLod = 0.5;
    return value;
}
void set(std::vector<uint8_t>& wire, size_t offset, uint64_t value, size_t width)
{
    require(offset <= wire.size() && width <= wire.size() - offset, "Pure wire edit stays bounded");
    detail::writeUnsigned(wire.data() + offset, value, width);
}
void roundTrip()
{
    auto value = sample();
    auto wire = encodeSourceBakedPayload(value, {});
    const auto saved = wire;
    const auto shape = inspectSourceBakedPayload(wire.data(), wire.size(), {});
    require(shape.vertexCount == 3 && shape.triangleCount == 1 && shape.pixelBytes == 20, "Matching geometry/records/textures inspect as one output");
    SourceBakedLimits exact;
    exact.maximumWireBytes = wire.size();
    exact.maximumDecodedBytes = shape.logicalBytes;
    auto copy = decodeSourceBakedPayload(wire.data(), wire.size(), exact);
    require(encodeSourceBakedPayload(copy, exact) == saved && copy.surface.models[0] != copy.surface.models[1], "Wire bits and Unicode alias keys retain exact spelling");
    require(std::signbit(copy.surface.triangles[0].uv[0][0]) && std::signbit(copy.surface.triangles[0].tangent[0])
            && copy.surface.triangles[0].alphaCutoff == 1.5 && copy.surface.triangles[0].alphaMode == 2,
        "Packed UV/tangent bits and unbounded nonnegative cutoff survive");
    wire.assign(wire.size(), 0);
    value = {};
    require(encodeSourceBakedPayload(copy, {}) == saved, "All decoded storage outlives original inputs");
    for (int field = 0; field < 2; ++field) {
        auto limits = exact;
        if (field == 0)
            --limits.maximumWireBytes;
        else
            --limits.maximumDecodedBytes;
        reject(SourceBakedErrorCode::limitExceeded, [&] { encodeSourceBakedPayload(copy, limits); });
        reject(SourceBakedErrorCode::limitExceeded, [&] { decodeSourceBakedPayload(saved.data(), saved.size(), limits); });
    }
    auto unknown = sample();
    unknown.surface.models.clear();
    unknown.surface.components.clear();
    unknown.surface.triangles = { BakedSurfaceTriangle {} };
    auto bytes = encodeSourceBakedPayload(unknown, {});
    auto output = decodeSourceBakedPayload(bytes.data(), bytes.size(), {});
    require(output.surface.triangles[0].modelIndex == noBakedIdentity && output.surface.triangles[0].componentIndex == noBakedIdentity
            && !output.surface.triangles[0].uvKnown && !output.surface.triangles[0].policyKnown && !output.surface.triangles[0].tangentKnown,
        "Unknown metadata remains explicit with independent empty catalogs");
}
void metadata()
{
    const auto original = sample();
    auto invalid = [&](SourceBakedPayload value, SourceBakedErrorCode code) { reject(code, [&] { encodeSourceBakedPayload(value, {}); }); };
    auto value = original;
    value.surface.triangles.clear();
    invalid(value, SourceBakedErrorCode::countMismatch);
    value = original;
    value.surface.models.push_back(value.surface.models[0]);
    invalid(value, SourceBakedErrorCode::invalidIdentity);
    value = original;
    value.surface.components.push_back(value.surface.components[0]);
    invalid(value, SourceBakedErrorCode::invalidIdentity);
    for (const auto& key : { std::string(), std::string(39, 'a'), std::string("a\0b", 3), std::string("\xff", 1) }) {
        value = original;
        value.surface.models[0] = key;
        invalid(value, SourceBakedErrorCode::invalidIdentity);
        value = original;
        value.surface.components[0].id = key;
        invalid(value, SourceBakedErrorCode::invalidIdentity);
    }
    for (uint32_t kind : { uint32_t(0), uint32_t(3) }) {
        value = original;
        value.surface.components[0].kind = kind;
        invalid(value, SourceBakedErrorCode::invalidMetadata);
    }
    for (int flag = 0; flag < 4; ++flag) {
        value = original;
        auto& t = value.surface.triangles[0];
        if (flag == 0)
            t.uvKnown = 2;
        if (flag == 1)
            t.policyKnown = 2;
        if (flag == 2)
            t.tangentKnown = 2;
        if (flag == 3)
            t.isDoubleSided = 2;
        invalid(value, SourceBakedErrorCode::invalidMetadata);
    }
    value = original;
    value.surface.triangles[0].modelIndex = 2;
    invalid(value, SourceBakedErrorCode::invalidMetadata);
    value = original;
    value.surface.triangles[0].componentIndex = 2;
    invalid(value, SourceBakedErrorCode::invalidMetadata);
    for (double coordinate : { -0.01, 1.01, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN() }) {
        value = original;
        value.surface.triangles[0].uv[0][0] = coordinate;
        invalid(value, SourceBakedErrorCode::invalidMetadata);
    }
    value = original;
    value.surface.triangles[0].uvKnown = 0;
    invalid(value, SourceBakedErrorCode::invalidMetadata);
    value = original;
    value.surface.triangles[0].uv = {};
    value.surface.triangles[0].uvKnown = 0;
    invalid(value, SourceBakedErrorCode::invalidMetadata);
    value = original;
    value.surface.triangles[0].tangent = { 0, 2, 0 };
    invalid(value, SourceBakedErrorCode::invalidMetadata);
    value = original;
    value.surface.triangles[0].tangent[0] = std::numeric_limits<float>::infinity();
    invalid(value, SourceBakedErrorCode::invalidMetadata);
    value = original;
    value.surface.triangles[0].handedness = 0;
    invalid(value, SourceBakedErrorCode::invalidMetadata);
    value = original;
    value.surface.triangles[0].alphaMode = 3;
    invalid(value, SourceBakedErrorCode::invalidMetadata);
    for (double cutoff : { -0.01, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN() }) {
        value = original;
        value.surface.triangles[0].alphaCutoff = cutoff;
        invalid(value, SourceBakedErrorCode::invalidMetadata);
    }
    value = original;
    value.surface.triangles[0].policyKnown = 0;
    require(!encodeSourceBakedPayload(value, {}).empty(), "Explicit unknown policy retains bounded opaque policy fields");
}
void textAndPolicies()
{
    for (const auto& text : { std::string(), std::string("a\1b", 3), std::string("\xc2\x80"), std::string("\xe0\xa0\x80"), std::string("\xf0\x90\x80\x80") })
        require(detail::isIdentityUtf8(text), "Exact valid non-NUL UTF8 is accepted without normalization");
    for (const auto& text : { std::string("\0", 1), std::string("\x80"), std::string("\xc0\x80"), std::string("\xf5\x80\x80\x80"),
             std::string("\xe0\xa0"), std::string("\xe0\x41\x80"), std::string("\xe0\x80\x80"), std::string("\xf4\x90\x80\x80"), std::string("\xed\xa0\x80") })
        require(!detail::isIdentityUtf8(text), "Truncated, overlong, surrogate, out-of-range and non-UTF8 keys reject");
    const auto original = sample();
    const auto bytes = encodeSourceBakedPayload(original, {});
    for (int field = 0; field < 6; ++field) {
        SourceBakedLimits limits;
        if (field == 0)
            limits.surface.maximumWireBytes = 1;
        if (field == 1)
            limits.surface.maximumDecodedBytes = 1;
        if (field == 2)
            limits.surface.maximumModels = 1;
        if (field == 3)
            limits.surface.maximumComponents = 1;
        if (field == 4)
            limits.surface.maximumTextBytes = 1;
        if (field == 5)
            limits.maximumWireBytes = 1;
        reject(SourceBakedErrorCode::limitExceeded, [&] { encodeSourceBakedPayload(original, limits); });
        reject(SourceBakedErrorCode::limitExceeded, [&] { decodeSourceBakedPayload(bytes.data(), bytes.size(), limits); });
    }
    auto doubled = original;
    doubled.geometry.vertices.insert(doubled.geometry.vertices.end(), original.geometry.vertices.begin(), original.geometry.vertices.end());
    doubled.surface.triangles.push_back(doubled.surface.triangles[0]);
    SourceBakedLimits narrow;
    narrow.surface.maximumTriangles = 1;
    reject(SourceBakedErrorCode::limitExceeded, [&] { encodeSourceBakedPayload(doubled, narrow); });
    for (int field = 0; field < 6; ++field)
        for (uint64_t amount : { uint64_t(0), maximumPayloadBytes + 1 }) {
            SourceBakedLimits limits;
            if (field == 0)
                limits.surface.maximumWireBytes = amount;
            if (field == 1)
                limits.surface.maximumDecodedBytes = amount;
            if (field == 2)
                limits.surface.maximumTriangles = amount;
            if (field == 3)
                limits.surface.maximumModels = amount;
            if (field == 4)
                limits.surface.maximumComponents = amount;
            if (field == 5)
                limits.surface.maximumTextBytes = amount;
            reject(SourceBakedErrorCode::invalidLimits, [&] { encodeSourceBakedPayload(original, limits); });
        }
    auto value = original;
    value.surface.triangles[0].tangent = { std::numeric_limits<float>::max(), 0, 0 };
    reject(SourceBakedErrorCode::invalidMetadata, [&] { encodeSourceBakedPayload(value, {}); });
    value = original;
    value.surface.triangles[0].policyKnown = 0;
    value.surface.triangles[0].alphaMode = UINT32_MAX;
    value.surface.triangles[0].alphaCutoff = std::numeric_limits<double>::infinity();
    auto opaque = encodeSourceBakedPayload(value, {});
    require(encodeSourceBakedPayload(decodeSourceBakedPayload(opaque.data(), opaque.size(), {}), {}) == opaque, "Unknown policy fields remain opaque and byte-preserved");
}
void framing()
{
    const auto wire = encodeSourceBakedPayload(sample(), {});
    reject(SourceBakedErrorCode::invalidInput, [&] { inspectSourceBakedPayload(nullptr, wire.size(), {}); });
    for (size_t size = 0; size < wire.size(); ++size)
        reject(SourceBakedErrorCode::truncated, [&] { decodeSourceBakedPayload(wire.data(), size, {}); });
    auto bytes = wire;
    bytes.push_back(0);
    reject(SourceBakedErrorCode::trailingData, [&] { decodeSourceBakedPayload(bytes.data(), bytes.size(), {}); });
    auto mutation = [&](size_t offset, uint64_t value, size_t width, SourceBakedErrorCode code) {
        auto changed = wire;
        set(changed, offset, value, width);
        reject(code, [&] { decodeSourceBakedPayload(changed.data(), changed.size(), {}); });
    };
    mutation(0, 2, 2, SourceBakedErrorCode::unsupportedVersion);
    mutation(2, 1, 2, SourceBakedErrorCode::invalidLayout);
    mutation(4, 1, 4, SourceBakedErrorCode::invalidLayout);
    mutation(8, UINT64_MAX, 8, SourceBakedErrorCode::limitExceeded);
    mutation(8, meshPrefixBytes - 1, 8, SourceBakedErrorCode::invalidLayout);
    mutation(16, bakedSurfacePrefixBytes - 1, 8, SourceBakedErrorCode::invalidLayout);
    mutation(24, bakedTexturesPrefixBytes - 1, 8, SourceBakedErrorCode::invalidLayout);
    const size_t surface = 32 + detail::readUnsigned(wire.data() + 8, 8);
    mutation(surface, 2, 2, SourceBakedErrorCode::unsupportedVersion);
    mutation(surface + 2, 1, 2, SourceBakedErrorCode::invalidLayout);
    mutation(surface + 4, 1, 4, SourceBakedErrorCode::invalidLayout);
    mutation(surface + 8, 2, 8, SourceBakedErrorCode::countMismatch);
    mutation(surface + 16, UINT64_MAX, 8, SourceBakedErrorCode::limitExceeded);
    mutation(surface + 40 + 88, 2, 1, SourceBakedErrorCode::invalidMetadata);
    const auto text = detail::readUnsigned(wire.data() + surface + 32, 8);
    mutation(surface + 32, text + 1, 8, SourceBakedErrorCode::truncated);
    mutation(surface + 32, text - 1, 8, SourceBakedErrorCode::trailingData);
    mutation(surface + 40 + 96, UINT32_MAX, 4, SourceBakedErrorCode::truncated);
    auto catalogOnly = sample();
    catalogOnly.surface.models = { "abcdefgh" };
    catalogOnly.surface.components.clear();
    catalogOnly.surface.triangles[0].modelIndex = 0;
    catalogOnly.surface.triangles[0].componentIndex = noBakedIdentity;
    auto catalogWire = encodeSourceBakedPayload(catalogOnly, {});
    set(catalogWire, surface + 16, 2, 8);
    set(catalogWire, surface + 32, 4, 8);
    reject(SourceBakedErrorCode::truncated, [&] { decodeSourceBakedPayload(catalogWire.data(), catalogWire.size(), {}); });
    catalogWire = encodeSourceBakedPayload(catalogOnly, {});
    set(catalogWire, surface + 24, 1, 8);
    set(catalogWire, surface + 32, 0, 8);
    reject(SourceBakedErrorCode::truncated, [&] { decodeSourceBakedPayload(catalogWire.data(), catalogWire.size(), {}); });
    catalogOnly.surface.models.clear();
    catalogOnly.surface.components = { { "abcdefgh", 1 } };
    catalogOnly.surface.triangles[0].modelIndex = noBakedIdentity;
    catalogOnly.surface.triangles[0].componentIndex = 0;
    catalogWire = encodeSourceBakedPayload(catalogOnly, {});
    auto surfaceBytes = detail::readUnsigned(catalogWire.data() + 16, 8);
    const size_t end = surface + surfaceBytes;
    catalogWire.insert(catalogWire.begin() + end, { 0, 0, 0, 1 });
    set(catalogWire, 16, surfaceBytes + 4, 8);
    set(catalogWire, surface + 24, 2, 8);
    set(catalogWire, surface + 32, 4, 8);
    reject(SourceBakedErrorCode::truncated, [&] { decodeSourceBakedPayload(catalogWire.data(), catalogWire.size(), {}); });
    catalogWire = encodeSourceBakedPayload(catalogOnly, {});
    set(catalogWire, surface + 40 + 96 + 4, 7, 4);
    reject(SourceBakedErrorCode::invalidLayout, [&] { decodeSourceBakedPayload(catalogWire.data(), catalogWire.size(), {}); });
    for (int field = 0; field < 2; ++field)
        for (uint64_t quota : { uint64_t(0), maximumPayloadBytes + 1 }) {
            SourceBakedLimits limits;
            if (field == 0)
                limits.maximumWireBytes = quota;
            else
                limits.maximumDecodedBytes = quota;
            reject(SourceBakedErrorCode::invalidLimits, [&] { validateSourceBakedLimits(limits); });
            reject(SourceBakedErrorCode::invalidLimits, [&] { decodeSourceBakedPayload(wire.data(), wire.size(), limits); });
        }
}
}
int main()
{
    try {
        roundTrip();
        metadata();
        textAndPolicies();
        framing();
        std::cout << "Source-baked worker contracts passed\n";
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}
