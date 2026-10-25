// SPDX-License-Identifier: MIT
#include <algorithm>
#include <iostream>
#include <native/Engine/WorkerUvRequest.h>
#include <stdexcept>
using namespace dust3d::worker;
namespace {
void require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}
template <typename Action>
void rejects(UvRequestErrorCode code, Action action)
{
    try {
        action();
    } catch (const UvRequestError& error) {
        require(error.code() == code && error.what()[0], "Typed request cause preserved");
        return;
    }
    throw std::runtime_error("Invalid UV request accepted");
}
UvRequest sample()
{
    UvRequest value;
    value.archive = { 0, 255, 42 };
    value.images = { { "{ABCDEF01-2345-6789-abcd-0123456789ab}", 17, 8192 } };
    return value;
}
void roundTrip()
{
    auto source = sample();
    validateUvRequest(source, {});
    auto wire = encodeUvRequest(source, {});
    const std::vector<uint8_t> prefix { 0, 1, 0, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3 };
    require(wire.size() == 83 && std::equal(prefix.begin(), prefix.end(), wire.begin()), "Independent v1 prefix");
    require(wire[32] == 38 && wire[71] == 0 && wire[75] == 17 && wire[78] == 32 && wire[79] == 0, "Image fixed record uses explicit big endian fields");
    UvRequestLimits limits;
    limits.maximumWireBytes = wire.size();
    limits.maximumDecodedBytes = 3 + sizeof(UvRequestImage) + 38;
    auto copy = decodeUvRequest(wire.data(), wire.size(), limits);
    require(encodeUvRequest(copy, limits) == wire, "Exact archive and identity bytes round trip");
    source = {};
    std::fill(wire.begin(), wire.end(), 0);
    require(copy.archive == std::vector<uint8_t>({ 0, 255, 42 }) && copy.images[0].identity == "{ABCDEF01-2345-6789-abcd-0123456789ab}", "Independent ownership");
    auto exact = encodeUvRequest(copy, limits);
    --limits.maximumDecodedBytes;
    rejects(UvRequestErrorCode::limitExceeded, [&] { decodeUvRequest(exact.data(), exact.size(), limits); });
    rejects(UvRequestErrorCode::limitExceeded, [&] { encodeUvRequest(copy, limits); });
    limits = {};
    limits.maximumWireBytes = exact.size() - 1;
    rejects(UvRequestErrorCode::limitExceeded, [&] { decodeUvRequest(exact.data(), exact.size(), limits); });
    rejects(UvRequestErrorCode::limitExceeded, [&] { encodeUvRequest(copy, limits); });
    UvRequest emptyImages;
    emptyImages.archive = { 1 };
    require(decodeUvRequest(encodeUvRequest(emptyImages, {}).data(), 33, {}).images.empty(), "Default options without images");
}
void invalidValues()
{
    auto source = sample();
    auto wire = encodeUvRequest(source, {});
    rejects(UvRequestErrorCode::invalidInput, [&] { decodeUvRequest(nullptr, wire.size(), {}); });
    for (size_t size : { size_t(0), size_t(31), size_t(82) })
        rejects(UvRequestErrorCode::truncated, [&] { decodeUvRequest(wire.data(), size, {}); });
    auto change = [&](size_t offset, uint8_t byte, UvRequestErrorCode cause) {
        auto altered = wire;
        altered[offset] = byte;
        rejects(cause, [&] { decodeUvRequest(altered.data(), altered.size(), {}); });
    };
    change(1, 2, UvRequestErrorCode::unsupportedVersion);
    change(3, 31, UvRequestErrorCode::invalidLayout);
    change(7, 1, UvRequestErrorCode::invalidLayout);
    change(32, 39, UvRequestErrorCode::invalidOptions);
    change(71, 1, UvRequestErrorCode::invalidLayout);
    change(33, '!', UvRequestErrorCode::invalidOptions);
    change(78, 33, UvRequestErrorCode::invalidOptions);
    change(15, 2, UvRequestErrorCode::truncated);
    change(31, 0, UvRequestErrorCode::invalidOptions);
    auto trailing = wire;
    trailing.push_back(0);
    rejects(UvRequestErrorCode::trailingData, [&] { decodeUvRequest(trailing.data(), trailing.size(), {}); });
    source.images.push_back({ "abcdef01-2345-6789-ABCD-0123456789AB", 1, 1 });
    rejects(UvRequestErrorCode::invalidOptions, [&] { encodeUvRequest(source, {}); });
    for (const auto& id : { "", "00000000-0000-0000-0000-000000000000", "abcdef0122345-6789-abcd-0123456789ab", "[abcdef01-2345-6789-abcd-0123456789ab]" }) {
        source = sample();
        source.images[0].identity = id;
        rejects(UvRequestErrorCode::invalidOptions, [&] { encodeUvRequest(source, {}); });
    }
    for (uint32_t dimension : { 0U, 8193U }) {
        source = sample();
        source.images[0].width = dimension;
        rejects(UvRequestErrorCode::invalidOptions, [&] { encodeUvRequest(source, {}); });
    }
    source = sample();
    source.textureSize = 8193;
    rejects(UvRequestErrorCode::invalidOptions, [&] { encodeUvRequest(source, {}); });
    source = sample();
    source.metadataBudget = maximumPayloadBytes + 1;
    rejects(UvRequestErrorCode::invalidOptions, [&] { encodeUvRequest(source, {}); });
    source = sample();
    source.archive.clear();
    rejects(UvRequestErrorCode::invalidOptions, [&] { encodeUvRequest(source, {}); });
    UvRequestLimits limits;
    limits.maximumImages = 0;
    rejects(UvRequestErrorCode::invalidLimits, [&] { encodeUvRequest(sample(), limits); });
    rejects(UvRequestErrorCode::invalidLimits, [&] { decodeUvRequest(wire.data(), wire.size(), limits); });
    source = sample();
    source.images.push_back({ "abcdef01-2345-6789-abcd-0123456789ac", 1, 1 });
    limits = {};
    limits.maximumImages = 1;
    rejects(UvRequestErrorCode::limitExceeded, [&] { encodeUvRequest(source, limits); });
}
void validationBoundaries()
{
    const auto source = sample();
    const auto wire = encodeUvRequest(source, {});
    for (int field = 0; field < 3; ++field) {
        for (bool above : { false, true }) {
            UvRequestLimits limits;
            if (field == 0)
                limits.maximumWireBytes = above ? maximumPayloadBytes + 1 : 0;
            if (field == 1)
                limits.maximumDecodedBytes = above ? maximumPayloadBytes + 1 : 0;
            if (field == 2)
                limits.maximumImages = above ? 10001 : 0;
            rejects(UvRequestErrorCode::invalidLimits, [&] { encodeUvRequest(source, limits); });
            rejects(UvRequestErrorCode::invalidLimits, [&] { decodeUvRequest(wire.data(), wire.size(), limits); });
        }
    }
    auto limit = UvRequestLimits {};
    limit.maximumDecodedBytes = 2;
    rejects(UvRequestErrorCode::limitExceeded, [&] { encodeUvRequest(source, limit); });
    rejects(UvRequestErrorCode::limitExceeded, [&] { decodeUvRequest(wire.data(), wire.size(), limit); });
    for (const auto& text : { "}abcdef01-2345-6789-abcd-0123456789ab}", "{abcdef01-2345-6789-abcd-0123456789ab]",
             "abcdef01-2345-6789-abcd-0123456789a/", "abcdef01-2345-6789-abcd-0123456789a:",
             "abcdef01-2345-6789-abcd-0123456789aG", "abcdef01-2345-6789-abcd-0123456789ag" }) {
        auto value = source;
        value.images[0].identity = text;
        rejects(UvRequestErrorCode::invalidOptions, [&] { encodeUvRequest(value, {}); });
    }
    for (uint32_t height : { 0U, 8193U }) {
        auto value = source;
        value.images[0].height = height;
        rejects(UvRequestErrorCode::invalidOptions, [&] { encodeUvRequest(value, {}); });
    }
    auto positive = source;
    positive.textureSize = 8192;
    positive.metadataBudget = maximumPayloadBytes;
    positive.images.push_back({ "abcdef01-2345-6789-abcd-0123456789ac", 8192, 1 });
    auto encoded = encodeUvRequest(positive, {});
    require(encodeUvRequest(decodeUvRequest(encoded.data(), encoded.size(), {}), {}) == encoded, "Nondefault options and distinct unbraced UUIDs retained");
    auto duplicate = encoded;
    const std::string alias = "abcdef01-2345-6789-ABCD-0123456789AB";
    std::copy(alias.begin(), alias.end(), duplicate.begin() + 81);
    rejects(UvRequestErrorCode::invalidOptions, [&] { decodeUvRequest(duplicate.data(), duplicate.size(), {}); });
    auto countLimit = UvRequestLimits {};
    countLimit.maximumImages = 1;
    rejects(UvRequestErrorCode::limitExceeded, [&] { decodeUvRequest(encoded.data(), encoded.size(), countLimit); });
    auto invalidLength = wire;
    invalidLength[32] = 37;
    rejects(UvRequestErrorCode::invalidOptions, [&] { decodeUvRequest(invalidLength.data(), invalidLength.size(), {}); });
}

}
int main()
{
    try {
        roundTrip();
        invalidValues();
        validationBoundaries();
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}
