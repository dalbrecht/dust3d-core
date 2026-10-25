// SPDX-License-Identifier: MIT
#include <cmath>
#include <iostream>
#include <native/Engine/WorkerSourceHints.h>
#include <native/Engine/WorkerWire.h>
#include <stdexcept>

using namespace dust3d::worker;
namespace {
void require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}

SourceHintPayload fixture()
{
    SourceHintPayload value;
    value.identities = { { "node-\xc3\xa9", "part-a" }, { "node-e\xcc\x81", "part-b" } };
    value.corners = {
        { SourceHintKind::nodeHint, 0, 1 },
        { SourceHintKind::weightedHint, 1, 2 },
        { SourceHintKind::unknown, 3, 0 }
    };
    value.influences = {
        { SourceHintKind::nodeHint, 0, 1.0f },
        { SourceHintKind::nodeHint, 1, 2.5f },
        { SourceHintKind::ambiguous, noSourceIdentity, 0.5f }
    };
    return value;
}

template <typename Work>
void rejects(Work work, SourceHintPayloadErrorCode expected)
{
    try {
        work();
    } catch (const SourceHintPayloadError& cause) {
        require(cause.code() == expected && cause.what()[0], "Original typed codec rejection and cause survive");
        return;
    }
    throw std::runtime_error("Invalid source codec values were admitted");
}

void set(std::vector<uint8_t>& bytes, size_t offset, uint64_t value, size_t count)
{
    require(offset <= bytes.size() && count <= bytes.size() - offset, "Test mutation is inside a complete fixture span");
    detail::writeUnsigned(bytes.data() + offset, value, count);
}

void quotasAndPolicies()
{
    const auto value = fixture();
    const auto bytes = encodeSourceHintPayload(value, {});
    const auto size = measureSourceHintPayload(value, {});
    require(size.wireBytes == bytes.size(), "Measured wire charge matches complete output");
    SourceHintPayloadLimits limits;
    limits.maximumWireBytes = size.wireBytes;
    limits.maximumDecodedBytes = size.decodedBytes;
    require(encodeSourceHintPayload(value, limits) == bytes, "Exact logical and wire quotas are admitted");
    require(decodeSourceHintPayload(bytes.data(), bytes.size(), limits).corners.size() == 3, "Exact decode quotas are admitted");
    --limits.maximumWireBytes;
    rejects([&] { encodeSourceHintPayload(value, limits); }, SourceHintPayloadErrorCode::limitExceeded);
    rejects([&] { decodeSourceHintPayload(bytes.data(), bytes.size(), limits); }, SourceHintPayloadErrorCode::limitExceeded);
    limits.maximumWireBytes = size.wireBytes;
    --limits.maximumDecodedBytes;
    rejects([&] { encodeSourceHintPayload(value, limits); }, SourceHintPayloadErrorCode::limitExceeded);
    rejects([&] { decodeSourceHintPayload(bytes.data(), bytes.size(), limits); }, SourceHintPayloadErrorCode::limitExceeded);
    limits = {};
    limits.maximumTextBytes = 1;
    rejects([&] { encodeSourceHintPayload(value, limits); }, SourceHintPayloadErrorCode::limitExceeded);
    rejects([&] { decodeSourceHintPayload(bytes.data(), bytes.size(), limits); }, SourceHintPayloadErrorCode::limitExceeded);
    limits = {};
    limits.maximumIdentityBytes = 1;
    rejects([&] { encodeSourceHintPayload(value, limits); }, SourceHintPayloadErrorCode::invalidIdentity);
    rejects([&] { decodeSourceHintPayload(bytes.data(), bytes.size(), limits); }, SourceHintPayloadErrorCode::invalidIdentity);
    using Member = uint64_t SourceHintPayloadLimits::*;
    for (const Member member : { &SourceHintPayloadLimits::maximumWireBytes, &SourceHintPayloadLimits::maximumDecodedBytes,
             &SourceHintPayloadLimits::maximumIdentities, &SourceHintPayloadLimits::maximumCorners,
             &SourceHintPayloadLimits::maximumInfluences, &SourceHintPayloadLimits::maximumTextBytes,
             &SourceHintPayloadLimits::maximumIdentityBytes }) {
        for (const uint64_t invalid : { uint64_t(0), std::numeric_limits<uint64_t>::max() }) {
            limits = {};
            limits.*member = invalid;
            rejects([&] { validateSourceHintPayloadLimits(limits); }, SourceHintPayloadErrorCode::invalidLimits);
        }
    }
    for (const Member member : { &SourceHintPayloadLimits::maximumIdentities, &SourceHintPayloadLimits::maximumCorners, &SourceHintPayloadLimits::maximumInfluences }) {
        limits = {};
        limits.*member = 1;
        rejects([&] { encodeSourceHintPayload(value, limits); }, SourceHintPayloadErrorCode::limitExceeded);
        rejects([&] { decodeSourceHintPayload(bytes.data(), bytes.size(), limits); }, SourceHintPayloadErrorCode::limitExceeded);
    }
}

void unknownsAndExactText()
{
    SourceHintPayload unknown;
    unknown.corners.resize(3);
    auto bytes = encodeSourceHintPayload(unknown, {});
    require(decodeSourceHintPayload(bytes.data(), bytes.size(), {}).identities.empty(), "Unknown corners need no invented identities");
    unknown.influences = { { SourceHintKind::unknown, noSourceIdentity, -0.0f } };
    unknown.corners.at(0) = { SourceHintKind::unknown, 0, 1 };
    bytes = encodeSourceHintPayload(unknown, {});
    auto result = decodeSourceHintPayload(bytes.data(), bytes.size(), {});
    require(std::signbit(result.influences.front().weight), "Negative zero weight bits are retained");
    auto value = fixture();
    value.identities.front() = { "node-\xe2\x82\xac", "part-\xf0\x9f\x98\x80" };
    value.corners.at(2) = { SourceHintKind::ambiguous, 2, 1 };
    value.influences.at(0).kind = static_cast<SourceHintKind>(17);
    value.corners.at(0).kind = static_cast<SourceHintKind>(17);
    bytes = encodeSourceHintPayload(value, {});
    result = decodeSourceHintPayload(bytes.data(), bytes.size(), {});
    require(encodeSourceHintPayload(result, {}) == bytes, "Three/four-byte UTF-8 and opaque future kinds round trip");
    for (const std::string text : { std::string(), std::string(1, '\0'), std::string("\x80"), std::string("\xc3"),
             std::string("\xe0\x80\x80"), std::string("\xed\xa0\x80"), std::string("\xf4\x90\x80\x80"), std::string("\xe2\x28\xa1") }) {
        value = fixture();
        value.identities.front().node = text;
        rejects([&] { encodeSourceHintPayload(value, {}); }, SourceHintPayloadErrorCode::invalidIdentity);
    }
}

void semanticRejections()
{
    auto value = fixture();
    for (const float weight : { -1.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN() }) {
        value = fixture();
        value.influences.front().weight = weight;
        rejects([&] { encodeSourceHintPayload(value, {}); }, SourceHintPayloadErrorCode::invalidWeight);
    }
    value = fixture();
    value.influences.front().identityIndex = noSourceIdentity;
    rejects([&] { encodeSourceHintPayload(value, {}); }, SourceHintPayloadErrorCode::invalidReference);
    value.influences.front() = { SourceHintKind::unknown, 0, 1 };
    rejects([&] { encodeSourceHintPayload(value, {}); }, SourceHintPayloadErrorCode::invalidReference);
    value.influences.front() = { SourceHintKind::weightedHint, 0, 1 };
    rejects([&] { encodeSourceHintPayload(value, {}); }, SourceHintPayloadErrorCode::invalidReference);
    value.influences.front() = { static_cast<SourceHintKind>(17), 2, 1 };
    rejects([&] { encodeSourceHintPayload(value, {}); }, SourceHintPayloadErrorCode::invalidReference);
    value = fixture();
    value.corners.front().firstInfluence = noSourceIdentity;
    rejects([&] { encodeSourceHintPayload(value, {}); }, SourceHintPayloadErrorCode::invalidReference);
    value.corners.front() = { SourceHintKind::nodeHint, 0, noSourceIdentity };
    rejects([&] { encodeSourceHintPayload(value, {}); }, SourceHintPayloadErrorCode::invalidReference);
    value.corners.front() = { SourceHintKind::nodeHint, 1, 2 };
    rejects([&] { encodeSourceHintPayload(value, {}); }, SourceHintPayloadErrorCode::invalidCorner);
    value.corners.front() = { SourceHintKind::ambiguous, 0, 1 };
    rejects([&] { encodeSourceHintPayload(value, {}); }, SourceHintPayloadErrorCode::invalidCorner);
    value.corners.front() = { SourceHintKind::unknown, 0, 1 };
    rejects([&] { encodeSourceHintPayload(value, {}); }, SourceHintPayloadErrorCode::invalidCorner);
    value.corners.front() = { SourceHintKind::unknown, 0, 2 };
    rejects([&] { encodeSourceHintPayload(value, {}); }, SourceHintPayloadErrorCode::invalidCorner);
    value.corners.front() = { SourceHintKind::weightedHint, 0, 0 };
    rejects([&] { encodeSourceHintPayload(value, {}); }, SourceHintPayloadErrorCode::invalidCorner);
    value.corners.clear();
    rejects([&] { encodeSourceHintPayload(value, {}); }, SourceHintPayloadErrorCode::invalidCorner);
    value.corners.resize(2);
    rejects([&] { encodeSourceHintPayload(value, {}); }, SourceHintPayloadErrorCode::invalidCorner);
}

void rejectsInconsistentWire()
{
    const auto original = encodeSourceHintPayload(fixture(), {});
    rejects([&] { decodeSourceHintPayload(nullptr, 0, {}); }, SourceHintPayloadErrorCode::invalidInput);
    rejects([&] { decodeSourceHintPayload(original.data(), sourceHintPrefixBytes - 1, {}); }, SourceHintPayloadErrorCode::invalidInput);
    auto bytes = original;
    set(bytes, 0, 2, 4);
    rejects([&] { decodeSourceHintPayload(bytes.data(), bytes.size(), {}); }, SourceHintPayloadErrorCode::unsupportedVersion);
    bytes = original;
    set(bytes, 4, 1, 4);
    rejects([&] { decodeSourceHintPayload(bytes.data(), bytes.size(), {}); }, SourceHintPayloadErrorCode::invalidLayout);
    bytes = original;
    bytes.push_back(0);
    rejects([&] { decodeSourceHintPayload(bytes.data(), bytes.size(), {}); }, SourceHintPayloadErrorCode::invalidLayout);
    bytes = original;
    bytes.pop_back();
    rejects([&] { decodeSourceHintPayload(bytes.data(), bytes.size(), {}); }, SourceHintPayloadErrorCode::invalidLayout);
    bytes = original;
    set(bytes, sourceHintPrefixBytes, 0, 4);
    rejects([&] { decodeSourceHintPayload(bytes.data(), bytes.size(), {}); }, SourceHintPayloadErrorCode::invalidIdentity);
    bytes = original;
    set(bytes, sourceHintPrefixBytes, 1024, 4);
    rejects([&] { decodeSourceHintPayload(bytes.data(), bytes.size(), {}); }, SourceHintPayloadErrorCode::invalidLayout);
    const auto cornerOffset = sourceHintPrefixBytes + 2 * 8 + static_cast<size_t>(detail::readUnsigned(original.data() + 32, 8));
    const auto influenceOffset = cornerOffset + 3 * sourceHintCornerBytes;
    // A first identity cannot consume the next record's required length fields.
    bytes = original;
    set(bytes, sourceHintPrefixBytes, 17, 4);
    set(bytes, sourceHintPrefixBytes + 4, 17, 4);
    for (size_t index = 0; index < 34; ++index)
        bytes.at(sourceHintPrefixBytes + 8 + index) = 'A';
    rejects([&] { decodeSourceHintPayload(bytes.data(), bytes.size(), {}); }, SourceHintPayloadErrorCode::invalidLayout);
    // The last identity must consume its complete declared text span.
    bytes = original;
    const auto value = fixture();
    const auto secondIdentity = sourceHintPrefixBytes + 8 + value.identities.front().node.size() + value.identities.front().part.size();
    set(bytes, secondIdentity + 4, value.identities.back().part.size() - 1, 4);
    rejects([&] { decodeSourceHintPayload(bytes.data(), bytes.size(), {}); }, SourceHintPayloadErrorCode::invalidLayout);
    for (const auto offset : { cornerOffset + 4, influenceOffset + 4, influenceOffset + 20 }) {
        bytes = original;
        set(bytes, offset, 1, 4);
        rejects([&] { decodeSourceHintPayload(bytes.data(), bytes.size(), {}); }, SourceHintPayloadErrorCode::invalidLayout);
    }
    bytes = original;
    set(bytes, influenceOffset + 16, 0x7f800000, 4);
    rejects([&] { decodeSourceHintPayload(bytes.data(), bytes.size(), {}); }, SourceHintPayloadErrorCode::invalidWeight);
}

void preservesOwnedSourceAssociations()
{
    auto original = fixture();
    auto bytes = encodeSourceHintPayload(original, {});
    const auto decoded = decodeSourceHintPayload(bytes.data(), bytes.size(), {});
    require(encodeSourceHintPayload(decoded, {}) == bytes, "Source hint round trip preserves canonical wire bytes");
    original.identities.front().node = "changed";
    bytes.assign(bytes.size(), 0);
    require(decoded.identities.size() == 2 && decoded.identities.at(0).node == "node-\xc3\xa9"
            && decoded.identities.at(1).node == "node-e\xcc\x81",
        "Byte-distinct Unicode identity rows remain independent owned values");
    require(decoded.corners.size() == 3 && decoded.corners.at(1).kind == SourceHintKind::weightedHint
            && decoded.corners.at(1).firstInfluence == 1 && decoded.corners.at(1).influenceCount == 2,
        "Per-corner association ranges survive transfer");
    require(decoded.influences.at(1).identityIndex == 1 && decoded.influences.at(1).weight == 2.5f,
        "Inherited nonnegative weights are not normalized or clamped to one");
    require(decoded.influences.at(2).kind == SourceHintKind::ambiguous
            && decoded.influences.at(2).identityIndex == noSourceIdentity,
        "Ambiguous hints retain the absent-identity sentinel");
}
}

int main()
{
    try {
        preservesOwnedSourceAssociations();
        quotasAndPolicies();
        unknownsAndExactText();
        semanticRejections();
        rejectsInconsistentWire();
        std::cout << "Worker source hint contracts passed\n";
        return 0;
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}
