// SPDX-License-Identifier: MIT
#include <algorithm>
#include <array>
#include <iostream>
#include <native/Engine/WorkerUvMesh.h>
#include <stdexcept>

using namespace dust3d::worker;
namespace {
void require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}
template <typename Error, typename Code, typename Action>
void rejects(Code code, Action action)
{
    try {
        action();
    } catch (const Error& cause) {
        require(cause.code() == code && cause.what()[0], "Original paired-payload cause preserved");
        return;
    }
    throw std::runtime_error("Invalid paired payload admitted");
}
UvMeshPayload sample()
{
    UvMeshPayload value;
    value.geometry.warning = "ok\n";
    value.geometry.nodeCount = 2;
    value.geometry.partCount = 1;
    const MeshVertex a { { 0, 0, 0 }, { 0, 0, 1 }, { 1, 0, 0, 1 } };
    const MeshVertex b { { 1, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0, 1 } };
    const MeshVertex c { { 0, 1, 0 }, { 0, 0, 1 }, { 0, 0, 1, 1 } };
    value.geometry.vertices = { a, b, c, a, b, c };
    value.uv.triangles.resize(2);
    value.uv.components = { "key" };
    return value;
}
void wireAndAggregateQuotas()
{
    auto source = sample();
    auto wire = encodeUvMeshPayload(source, {});
    const std::array<uint8_t, 24> prefix { 0, 1, 0, 24, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 27, 0, 0, 0, 0, 0, 0, 0, 191 };
    require(wire.size() == 498 && std::equal(prefix.begin(), prefix.end(), wire.begin()), "Independent paired prefix and subspan lengths required");
    const auto geometry = inspectMeshPayload(wire.data() + 24, 283, {});
    require(geometry.vertexCount == 6 && geometry.logicalBytes == 6 * sizeof(MeshVertex) + 3, "Geometry validation-only inspection has exact logical charge");
    const auto uvBytes = inspectUvPayload(wire.data() + 307, 191, 2, {});
    require(uvBytes == 2 * sizeof(UvTriangle) + sizeof(std::string) + 3, "UV inspection has exact independent charge");
    UvMeshPayloadLimits exact;
    exact.maximumWireBytes = wire.size();
    exact.maximumDecodedBytes = geometry.logicalBytes + uvBytes;
    auto decoded = decodeUvMeshPayload(wire.data(), wire.size(), exact);
    require(encodeUvMeshPayload(decoded, exact) == wire, "Full pair survives independent decode");
    auto under = exact;
    --under.maximumWireBytes;
    rejects<UvMeshPayloadError>(UvMeshPayloadErrorCode::limitExceeded, [&] { encodeUvMeshPayload(source, under); });
    rejects<UvMeshPayloadError>(UvMeshPayloadErrorCode::limitExceeded, [&] { decodeUvMeshPayload(wire.data(), wire.size(), under); });
    under = exact;
    --under.maximumDecodedBytes;
    rejects<UvMeshPayloadError>(UvMeshPayloadErrorCode::limitExceeded, [&] { encodeUvMeshPayload(source, under); });
    rejects<UvMeshPayloadError>(UvMeshPayloadErrorCode::limitExceeded, [&] { decodeUvMeshPayload(wire.data(), wire.size(), under); });
    source = {};
    std::fill(wire.begin(), wire.end(), 0);
    require(decoded.geometry.vertices.size() == 6 && decoded.geometry.warning == "ok\n" && decoded.uv.components == std::vector<std::string>({ "key" }), "Returned pair independently owns geometry, UVs and text");
}
void rejectedPairsAreAtomic()
{
    auto source = sample();
    auto wire = encodeUvMeshPayload(source, {});
    rejects<UvMeshPayloadError>(UvMeshPayloadErrorCode::invalidInput, [&] { decodeUvMeshPayload(nullptr, wire.size(), {}); });
    rejects<UvMeshPayloadError>(UvMeshPayloadErrorCode::truncated, [&] { decodeUvMeshPayload(wire.data(), 23, {}); });
    rejects<UvMeshPayloadError>(UvMeshPayloadErrorCode::truncated, [&] { decodeUvMeshPayload(wire.data(), wire.size() - 1, {}); });
    auto trailing = wire;
    trailing.push_back(0);
    rejects<UvMeshPayloadError>(UvMeshPayloadErrorCode::trailingData, [&] { decodeUvMeshPayload(trailing.data(), trailing.size(), {}); });
    for (auto item : { std::pair<size_t, uint8_t> { 1, 2 }, { 3, 23 }, { 7, 1 }, { 14, 0 }, { 23, 0 }, { 8, 255 } }) {
        auto malformed = wire;
        malformed[item.first] = item.second;
        auto code = item.first == 1 ? UvMeshPayloadErrorCode::unsupportedVersion : item.first == 8 ? UvMeshPayloadErrorCode::limitExceeded
                                                                                                   : UvMeshPayloadErrorCode::invalidLayout;
        rejects<UvMeshPayloadError>(code, [&] { decodeUvMeshPayload(malformed.data(), malformed.size(), {}); });
    }
    auto mismatch = wire;
    mismatch[307 + 15] = 1;
    const auto before = mismatch;
    rejects<UvPayloadError>(UvPayloadErrorCode::invalidLayout, [&] { decodeUvMeshPayload(mismatch.data(), mismatch.size(), {}); });
    require(mismatch == before, "Atomic rejection never mutates borrowed source bytes");
    auto lateGeometry = wire;
    std::copy(wire.begin() + 227, wire.begin() + 267, lateGeometry.begin() + 267);
    rejects<MeshPayloadError>(MeshPayloadErrorCode::invalidGeometry, [&] { decodeUvMeshPayload(lateGeometry.data(), lateGeometry.size(), {}); });
    auto lateUv = wire;
    lateUv[475] = 2;
    rejects<UvPayloadError>(UvPayloadErrorCode::invalidMetadata, [&] { decodeUvMeshPayload(lateUv.data(), lateUv.size(), {}); });
    auto invalidUv = wire;
    invalidUv[307 + 56 + 48] = 2;
    rejects<UvPayloadError>(UvPayloadErrorCode::invalidMetadata, [&] { decodeUvMeshPayload(invalidUv.data(), invalidUv.size(), {}); });
    auto inner = UvMeshPayloadLimits {};
    inner.geometry.maximumVertices = 3;
    rejects<MeshPayloadError>(MeshPayloadErrorCode::limitExceeded, [&] { decodeUvMeshPayload(wire.data(), wire.size(), inner); });
    inner = {};
    inner.uv.maximumTriangles = 1;
    rejects<UvPayloadError>(UvPayloadErrorCode::limitExceeded, [&] { decodeUvMeshPayload(wire.data(), wire.size(), inner); });
    auto mismatchedSource = source;
    mismatchedSource.uv.triangles.pop_back();
    rejects<UvPayloadError>(UvPayloadErrorCode::invalidLayout, [&] { encodeUvMeshPayload(mismatchedSource, {}); });
    auto invalidGeometry = wire;
    invalidGeometry[67] = 0x7f;
    invalidGeometry[68] = 0x80;
    rejects<MeshPayloadError>(MeshPayloadErrorCode::invalidGeometry, [&] { decodeUvMeshPayload(invalidGeometry.data(), invalidGeometry.size(), {}); });
    auto tiny = UvMeshPayloadLimits {};
    tiny.maximumWireBytes = 1;
    rejects<UvMeshPayloadError>(UvMeshPayloadErrorCode::limitExceeded, [&] { encodeUvMeshPayload(source, tiny); });
    tiny = {};
    tiny.maximumDecodedBytes = 1;
    rejects<UvMeshPayloadError>(UvMeshPayloadErrorCode::limitExceeded, [&] { decodeUvMeshPayload(wire.data(), wire.size(), tiny); });
    auto invalidInner = UvMeshPayloadLimits {};
    invalidInner.geometry.maximumVertices = 0;
    rejects<MeshPayloadError>(MeshPayloadErrorCode::invalidLimits, [&] { encodeUvMeshPayload(source, invalidInner); });
    invalidInner = {};
    invalidInner.uv.maximumTriangles = 0;
    rejects<UvPayloadError>(UvPayloadErrorCode::invalidLimits, [&] { decodeUvMeshPayload(wire.data(), wire.size(), invalidInner); });
    for (auto limit : { uint64_t { 0 }, maximumPayloadBytes + 1 }) {
        auto invalid = UvMeshPayloadLimits {};
        invalid.maximumWireBytes = limit;
        rejects<UvMeshPayloadError>(UvMeshPayloadErrorCode::invalidLimits, [&] { encodeUvMeshPayload(source, invalid); });
        invalid = {};
        invalid.maximumDecodedBytes = limit;
        rejects<UvMeshPayloadError>(UvMeshPayloadErrorCode::invalidLimits, [&] { decodeUvMeshPayload(wire.data(), wire.size(), invalid); });
    }
}
}
int main()
{
    try {
        wireAndAggregateQuotas();
        rejectedPairsAreAtomic();
        std::cout << "Paired worker geometry/UV contracts passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
