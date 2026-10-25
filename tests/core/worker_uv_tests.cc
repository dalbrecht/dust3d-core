// SPDX-License-Identifier: MIT
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <native/Engine/WorkerUv.h>
#include <stdexcept>

using namespace dust3d::worker;
namespace {
void require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}
template <typename Action>
void rejects(UvPayloadErrorCode code, Action action)
{
    try {
        action();
    } catch (const UvPayloadError& cause) {
        if (cause.code() != code)
            throw std::runtime_error("UV rejection code mismatch: expected=" + std::to_string(static_cast<int>(code)) + " actual=" + std::to_string(static_cast<int>(cause.code())));
        require(cause.what()[0], "Original typed UV payload cause required");
        return;
    }
    throw std::runtime_error("Invalid UV payload admitted");
}
UvPayload sample()
{
    UvPayload result;
    result.components = { "opaque<&", "cafe\xcc\x81" };
    result.packedTextureSize = 64;
    UvTriangle mapped;
    mapped.uv = { std::array<double, 2> { 0, 0 }, { 1, 0 }, { 0, 1 } };
    mapped.known = 1;
    mapped.uv[0][0] = -0.0;
    result.triangles = { mapped, { {}, 0, UvAssociation::ambiguous, noUvComponent } };
    result.charts = { { 0, 0, 1, 1, { 1, 0.5, 0, 1 }, 1, UvAssociation::component, 0, "" } };
    result.gradients = { { 0, { 1, 0, 0, 1 }, { 0, 0, 1, 1 }, 16 } };
    return result;
}
void same(const UvPayload& first, const UvPayload& second)
{
    require(first.components == second.components && first.packedTextureSize == second.packedTextureSize && first.triangles.size() == second.triangles.size() && first.charts.size() == second.charts.size() && first.gradients.size() == second.gradients.size(), "UV counts/keys/texture scale preserved");
    for (size_t i = 0; i < first.triangles.size(); ++i) {
        const auto& a = first.triangles[i];
        const auto& b = second.triangles[i];
        require(std::memcmp(a.uv.data(), b.uv.data(), 6 * sizeof(double)) == 0 && a.known == b.known && a.association == b.association && a.componentIndex == b.componentIndex, "Triangle corner bits and independent mapping/association preserved");
    }
    for (size_t i = 0; i < first.charts.size(); ++i) {
        const auto& a = first.charts[i];
        const auto& b = second.charts[i];
        require(a.left == b.left && a.top == b.top && a.width == b.width && a.height == b.height && a.color == b.color && a.flipped == b.flipped && a.association == b.association && a.componentIndex == b.componentIndex && a.imageID == b.imageID, "Complete chart metadata preserved");
    }
    for (size_t i = 0; i < first.gradients.size(); ++i) {
        const auto& a = first.gradients[i];
        const auto& b = second.gradients[i];
        require(a.chartIndex == b.chartIndex && a.largeColor == b.largeColor && a.smallColor == b.smallColor && a.resolution == b.resolution, "Gradient metadata preserved without sampling");
    }
}
void wireAndOwnership()
{
    auto source = sample();
    auto bytes = encodeUvPayload(source, 2, {});
    const std::array<uint8_t, 56> prefix {
        0, 1, 0, 56, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 2,
        0, 0, 0, 0, 0, 0, 0, 2,
        0, 0, 0, 0, 0, 0, 0, 1,
        0, 0, 0, 0, 0, 0, 0, 1,
        0, 0, 0, 0, 0, 0, 0, 14,
        0x40, 0x50, 0, 0, 0, 0, 0, 0
    };
    require(bytes.size() == 406 && std::equal(prefix.begin(), prefix.end(), bytes.begin()), "Independent UV v1 prefix and total span required");
    require(bytes[56] == 0x80 && bytes[72] == 0x3f && bytes[73] == 0xf0 && bytes[104] == 1 && bytes[105] == 0 && bytes[112] == 255 && bytes[119] == 255, "Explicit double bytes, known mapping and unknown association sentinel required");
    require(bytes[248] == 1 && bytes[249] == 1 && bytes[264] == 0 && bytes[387] == 8 && bytes[388] == 'o', "Independent chart flags and component text order required");
    auto decoded = decodeUvPayload(bytes.data(), bytes.size(), 2, {});
    same(source, decoded);
    source = {};
    std::fill(bytes.begin(), bytes.end(), 0);
    same(sample(), decoded);
    auto image = sample();
    image.charts[0].imageID = "{11111111-1111-4111-8111-111111111111}";
    image.gradients.clear();
    auto imageBytes = encodeUvPayload(image, 2, {});
    same(image, decodeUvPayload(imageBytes.data(), imageBytes.size(), 2, {}));
}
void textAndMetadataBoundaries()
{
    for (const auto& text : { std::string("\0", 1), std::string("\x7f", 1), std::string("\x01", 1),
             std::string("\xc2", 1), std::string("\xc2\x41", 2), std::string("\xe0\x80\x80", 3),
             std::string("\xed\xa0\x80", 3), std::string("\xf0\x80\x80\x80", 4), std::string("\xf4\x90\x80\x80", 4), std::string("\xf5\x80\x80\x80", 4) }) {
        auto input = sample();
        input.components[0] = text;
        rejects(UvPayloadErrorCode::invalidText, [&] { encodeUvPayload(input, 2, {}); });
        auto wire = encodeUvPayload(sample(), 2, {});
        std::copy(text.begin(), text.end(), wire.begin() + 388);
        rejects(UvPayloadErrorCode::invalidText, [&] { decodeUvPayload(wire.data(), wire.size(), 2, {}); });
    }
    auto international = sample();
    international.components = { "", "\xc2\xa9", "\xe4\xb8\xad", "\xf0\x9f\x98\x80", "\xf4\x8f\xbf\xbf", "tab\tline\nreturn\r" };
    auto wire = encodeUvPayload(international, 2, {});
    same(international, decodeUvPayload(wire.data(), wire.size(), 2, {}));
    for (int field = 0; field < 17; ++field) {
        auto input = sample();
        if (field == 0)
            input.triangles[0].uv[0][0] = -1;
        if (field == 1)
            input.triangles[0].uv[0][0] = std::numeric_limits<double>::infinity();
        if (field == 2)
            input.triangles[0].association = static_cast<UvAssociation>(99);
        if (field == 3)
            input.triangles[1].componentIndex = 0;
        if (field == 4)
            input.charts[0].left = -1;
        if (field == 5)
            input.charts[0].left = 0.5;
        if (field == 6)
            input.charts[0].top = 0.5;
        if (field == 7)
            input.charts[0].height = 2;
        if (field == 8)
            input.charts[0].color[2] = 2;
        if (field == 9)
            input.charts[0].flipped = 2;
        if (field == 10)
            input.gradients[0].resolution = 0;
        if (field == 11)
            input.gradients[0].largeColor[0] = -1;
        if (field == 12)
            input.gradients[0].smallColor[0] = 2;
        if (field == 13)
            input.packedTextureSize = 0;
        if (field == 14)
            input.packedTextureSize = -1;
        if (field == 15)
            input.packedTextureSize = std::numeric_limits<double>::infinity();
        if (field == 16) {
            input.charts.clear();
            input.gradients.clear();
        }
        rejects(UvPayloadErrorCode::invalidMetadata, [&] { encodeUvPayload(input, 2, {}); });
    }
    auto unmapped = sample();
    unmapped.charts.clear();
    unmapped.gradients.clear();
    unmapped.packedTextureSize = 0;
    unmapped.triangles[0].known = 0;
    unmapped.triangles[0].uv = {};
    wire = encodeUvPayload(unmapped, 2, {});
    same(unmapped, decodeUvPayload(wire.data(), wire.size(), 2, {}));
    for (const auto& image : { std::string("bad"), std::string("[11111111-1111-4111-8111-111111111111]"), std::string("11111111_1111-4111-8111-111111111111"), std::string("g1111111-1111-4111-8111-111111111111"), std::string("00000000-0000-0000-0000-000000000000") }) {
        auto input = sample();
        input.charts[0].imageID = image;
        rejects(UvPayloadErrorCode::invalidText, [&] { encodeUvPayload(input, 2, {}); });
    }
    auto image = sample();
    image.charts[0].imageID = "aBcDeF01-2345-4aBC-8Def-123456789AbC";
    wire = encodeUvPayload(image, 2, {});
    same(image, decodeUvPayload(wire.data(), wire.size(), 2, {}));
    for (int field = 0; field < 8; ++field) {
        auto limits = UvPayloadLimits {};
        if (field == 0)
            limits.maximumWireBytes = 0;
        if (field == 1)
            limits.maximumDecodedBytes = maximumPayloadBytes + 1;
        if (field == 2)
            limits.maximumTriangles = 0;
        if (field == 3)
            limits.maximumComponents = maximumUvRecords + 1;
        if (field == 4)
            limits.maximumCharts = 0;
        if (field == 5)
            limits.maximumGradients = maximumUvRecords + 1;
        if (field == 6)
            limits.maximumTextBytes = 0;
        if (field == 7)
            limits.maximumKeyBytes = maximumPayloadBytes + 1;
        rejects(UvPayloadErrorCode::invalidLimits, [&] { encodeUvPayload(sample(), 2, limits); });
        rejects(UvPayloadErrorCode::invalidLimits, [&] { decodeUvPayload(wire.data(), wire.size(), 2, limits); });
    }
    for (auto mutation : { std::pair<size_t, uint8_t> { 3, 55 }, { 264, 39 }, { 250, 1 }, { 387, 15 }, { 399, 4 } }) {
        auto malformed = encodeUvPayload(sample(), 2, {});
        malformed[mutation.first] = mutation.second;
        rejects(UvPayloadErrorCode::invalidLayout, [&] { decodeUvPayload(malformed.data(), malformed.size(), 2, {}); });
    }
    for (int category = 0; category < 3; ++category) {
        auto input = sample();
        auto limits = UvPayloadLimits {};
        if (category == 0)
            limits.maximumComponents = 1;
        if (category == 1) {
            input.charts.push_back(input.charts[0]);
            limits.maximumCharts = 1;
        }
        if (category == 2) {
            input.gradients.push_back(input.gradients[0]);
            limits.maximumGradients = 1;
        }
        auto oversized = encodeUvPayload(input, 2, {});
        rejects(UvPayloadErrorCode::limitExceeded, [&] { encodeUvPayload(input, 2, limits); });
        rejects(UvPayloadErrorCode::limitExceeded, [&] { decodeUvPayload(oversized.data(), oversized.size(), 2, limits); });
    }
    for (int field = 0; field < 2; ++field) {
        auto input = sample();
        if (field == 0)
            input.charts[0].top = -1;
        if (field == 1)
            input.charts[0].width = 2;
        rejects(UvPayloadErrorCode::invalidMetadata, [&] { encodeUvPayload(input, 2, {}); });
    }
    auto emptyKeys = unmapped;
    emptyKeys.components.clear();
    wire = encodeUvPayload(emptyKeys, 2, {});
    same(emptyKeys, decodeUvPayload(wire.data(), wire.size(), 2, {}));
    auto wrongBrace = sample();
    wrongBrace.charts[0].imageID = "{11111111-1111-4111-8111-111111111111]";
    rejects(UvPayloadErrorCode::invalidText, [&] { encodeUvPayload(wrongBrace, 2, {}); });
    rejects(UvPayloadErrorCode::invalidInput, [&] { encodeUvPayload(sample(), maximumUvRecords + 1, {}); });
    auto tiny = UvPayloadLimits {};
    tiny.maximumWireBytes = 1;
    wire = encodeUvPayload(sample(), 2, {});
    rejects(UvPayloadErrorCode::limitExceeded, [&] { encodeUvPayload(sample(), 2, tiny); });
    rejects(UvPayloadErrorCode::limitExceeded, [&] { decodeUvPayload(wire.data(), wire.size(), 2, tiny); });
    auto invalidDigit = sample();
    invalidDigit.charts[0].imageID = "/1111111-1111-4111-8111-111111111111";
    rejects(UvPayloadErrorCode::invalidText, [&] { encodeUvPayload(invalidDigit, 2, {}); });
    auto nonfinite = encodeUvPayload(sample(), 2, {});
    nonfinite[56] = 0x7f;
    nonfinite[57] = 0xf0;
    rejects(UvPayloadErrorCode::invalidMetadata, [&] { decodeUvPayload(nonfinite.data(), nonfinite.size(), 2, {}); });
}
void quotasAndRejection()
{
    auto source = sample();
    auto bytes = encodeUvPayload(source, 2, {});
    UvPayloadLimits exact;
    exact.maximumWireBytes = bytes.size();
    exact.maximumDecodedBytes = sizeof(UvTriangle) * 2 + sizeof(UvChart) + sizeof(UvGradient) + sizeof(std::string) * 2 + 14;
    exact.maximumTriangles = 2;
    exact.maximumComponents = 2;
    exact.maximumCharts = 1;
    exact.maximumGradients = 1;
    exact.maximumTextBytes = 14;
    exact.maximumKeyBytes = 8;
    same(source, decodeUvPayload(bytes.data(), bytes.size(), 2, exact));
    require(encodeUvPayload(source, 2, exact) == bytes, "Exact independent quotas accepted");
    for (int field = 0; field < 5; ++field) {
        auto limits = exact;
        if (field == 0)
            --limits.maximumWireBytes;
        if (field == 1)
            --limits.maximumDecodedBytes;
        if (field == 2)
            --limits.maximumTextBytes;
        if (field == 3)
            --limits.maximumTriangles;
        if (field == 4)
            --limits.maximumKeyBytes;
        rejects(UvPayloadErrorCode::limitExceeded, [&] { encodeUvPayload(source, 2, limits); });
        rejects(UvPayloadErrorCode::limitExceeded, [&] { decodeUvPayload(bytes.data(), bytes.size(), 2, limits); });
    }
    rejects(UvPayloadErrorCode::invalidInput, [&] { decodeUvPayload(nullptr, 1, 2, {}); });
    rejects(UvPayloadErrorCode::invalidInput, [&] { encodeUvPayload(source, 0, {}); });
    rejects(UvPayloadErrorCode::invalidLayout, [&] { decodeUvPayload(bytes.data(), bytes.size(), 1, {}); });
    rejects(UvPayloadErrorCode::truncated, [&] { decodeUvPayload(bytes.data(), 55, 2, {}); });
    rejects(UvPayloadErrorCode::truncated, [&] { decodeUvPayload(bytes.data(), bytes.size() - 1, 2, {}); });
    auto extra = bytes;
    extra.push_back(0);
    rejects(UvPayloadErrorCode::trailingData, [&] { decodeUvPayload(extra.data(), extra.size(), 2, {}); });
    for (auto mutation : { std::pair<size_t, uint8_t> { 1, 2 }, { 7, 1 }, { 106, 1 }, { 104, 2 }, { 105, 99 }, { 265, 1 } }) {
        auto bad = bytes;
        bad[mutation.first] = mutation.second;
        const auto code = mutation.first == 1 ? UvPayloadErrorCode::unsupportedVersion : mutation.first == 104 || mutation.first == 105 ? UvPayloadErrorCode::invalidMetadata
                                                                                                                                        : UvPayloadErrorCode::invalidLayout;
        rejects(code, [&] { decodeUvPayload(bad.data(), bad.size(), 2, {}); });
    }
    source.triangles[0].association = UvAssociation::component;
    source.triangles[0].componentIndex = 2;
    rejects(UvPayloadErrorCode::invalidMetadata, [&] { encodeUvPayload(source, 2, {}); });
    source = sample();
    source.triangles[0].known = 0;
    rejects(UvPayloadErrorCode::invalidMetadata, [&] { encodeUvPayload(source, 2, {}); });
    source = sample();
    source.gradients[0].chartIndex = 1;
    rejects(UvPayloadErrorCode::invalidMetadata, [&] { encodeUvPayload(source, 2, {}); });
    source = sample();
    source.components[1] = source.components[0];
    rejects(UvPayloadErrorCode::invalidMetadata, [&] { encodeUvPayload(source, 2, {}); });
    source = sample();
    source.components[0] = std::string("\xc0\x80", 2);
    rejects(UvPayloadErrorCode::invalidText, [&] { encodeUvPayload(source, 2, {}); });
}
}
int main()
{
    try {
        wireAndOwnership();
        quotasAndRejection();
        textAndMetadataBoundaries();
        std::cout << "Worker UV value-only contracts passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
