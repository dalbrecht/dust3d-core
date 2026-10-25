// SPDX-License-Identifier: MIT
#include <algorithm>
#include <iostream>
#include <native/Engine/WorkerSourcedMesh.h>
#include <native/Engine/WorkerWire.h>
#include <stdexcept>

using namespace dust3d::worker;
namespace {
void require(bool value, const char* cause)
{
    if (!value)
        throw std::runtime_error(cause);
}

SourcedMeshPayload makeFixture()
{
    SourcedMeshPayload value;
    value.geometry.vertices = {
        { { 0, 0, 0 }, { 0, 0, 1 }, { 1, 1, 1, 1 } },
        { { 1, 0, 0 }, { 0, 0, 1 }, { 1, 1, 1, 1 } },
        { { 0, 1, 0 }, { 0, 0, 1 }, { 1, 1, 1, 1 } }
    };
    value.geometry.nodeCount = 1;
    value.geometry.partCount = 1;
    value.geometry.warning = "owned fixture";
    value.sources.identities = { { "{11111111-1111-4111-8111-111111111111}", "{aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa}" } };
    value.sources.corners.assign(3, { SourceHintKind::nodeHint, 0, 1 });
    value.sources.influences = { { SourceHintKind::nodeHint, 0, 2.5f } };
    return value;
}

template <typename Error, typename Work, typename Code>
void rejectAs(Work work, Code expected)
{
    try {
        work();
    } catch (const Error& cause) {
        require(cause.code() == expected && cause.what()[0], "Original typed failure and diagnostic survive pairing");
        return;
    }
    throw std::runtime_error("Invalid sourced pair was admitted");
}

void setWireField(std::vector<uint8_t>& bytes, size_t offset, uint64_t value, size_t count)
{
    require(offset <= bytes.size() && count <= bytes.size() - offset, "Codec fixture mutation stays in its owned span");
    detail::writeUnsigned(bytes.data() + offset, value, count);
}

// Assemble independently valid inner values to test outer correspondence rejection.
std::vector<uint8_t> frameInnerValues(const SourcedMeshPayload& value)
{
    const auto mesh = encodeMeshPayload(value.geometry, {});
    const auto sources = encodeSourceHintPayload(value.sources, {});
    std::vector<uint8_t> bytes(sourcedMeshPrefixBytes + mesh.size() + sources.size(), 0);
    setWireField(bytes, 0, 1, 2);
    setWireField(bytes, 2, sourcedMeshPrefixBytes, 2);
    setWireField(bytes, 8, mesh.size(), 8);
    setWireField(bytes, 16, sources.size(), 8);
    std::copy(mesh.begin(), mesh.end(), bytes.begin() + sourcedMeshPrefixBytes);
    std::copy(sources.begin(), sources.end(), bytes.begin() + sourcedMeshPrefixBytes + mesh.size());
    return bytes;
}

void preservesAtomicOwnedPair()
{
    auto original = makeFixture();
    auto bytes = encodeSourcedMeshPayload(original, {});
    const auto decoded = decodeSourcedMeshPayload(bytes.data(), bytes.size(), {});
    require(encodeSourcedMeshPayload(decoded, {}) == bytes, "Atomic source/geometry pairing round trips");
    original.geometry.vertices.clear();
    original.sources.identities.clear();
    bytes.assign(bytes.size(), 0);
    require(decoded.geometry.vertices.size() == 3 && decoded.geometry.vertices.at(2).position.at(1) == 1,
        "Decoded geometry remains independent of input storage");
    require(decoded.sources.corners.size() == 3 && decoded.sources.identities.size() == decoded.geometry.nodeCount
            && decoded.sources.influences.front().weight == 2.5f,
        "Source correspondence and original weight survive ownership transfer");
}

void rejectsMismatchedCorrespondence()
{
    auto value = makeFixture();
    value.sources.corners.resize(6, value.sources.corners.front());
    for (int variant = 0; variant < 2; ++variant) {
        rejectAs<SourcedMeshPayloadError>([&] { encodeSourcedMeshPayload(value, {}); }, SourcedMeshPayloadErrorCode::countMismatch);
        const auto bytes = frameInnerValues(value);
        rejectAs<SourcedMeshPayloadError>([&] { decodeSourcedMeshPayload(bytes.data(), bytes.size(), {}); }, SourcedMeshPayloadErrorCode::countMismatch);
        value = makeFixture();
        value.geometry.nodeCount = 2;
    }
}

void verifiesAggregateAdmission()
{
    const auto value = makeFixture();
    const auto bytes = encodeSourcedMeshPayload(value, {});
    const auto mesh = encodeMeshPayload(value.geometry, {});
    const auto sources = encodeSourceHintPayload(value.sources, {});
    const auto meshSummary = inspectMeshPayload(mesh.data(), mesh.size(), {});
    const auto sourceSummary = inspectSourceHintPayload(sources.data(), sources.size(), {});
    require(meshSummary.nodeCount == 1, "Mesh inspection returns its validated source-node count");
    SourcedMeshPayloadLimits limits;
    limits.maximumWireBytes = bytes.size();
    limits.maximumDecodedBytes = meshSummary.logicalBytes + sourceSummary.logicalBytes;
    require(encodeSourcedMeshPayload(value, limits) == bytes, "Exact paired budgets are admitted");
    require(decodeSourcedMeshPayload(bytes.data(), bytes.size(), limits).sources.corners.size() == 3, "Exact decoded budget is admitted");
    --limits.maximumWireBytes;
    rejectAs<SourcedMeshPayloadError>([&] { encodeSourcedMeshPayload(value, limits); }, SourcedMeshPayloadErrorCode::limitExceeded);
    rejectAs<SourcedMeshPayloadError>([&] { decodeSourcedMeshPayload(bytes.data(), bytes.size(), limits); }, SourcedMeshPayloadErrorCode::limitExceeded);
    limits.maximumWireBytes = bytes.size();
    --limits.maximumDecodedBytes;
    rejectAs<SourcedMeshPayloadError>([&] { encodeSourcedMeshPayload(value, limits); }, SourcedMeshPayloadErrorCode::limitExceeded);
    rejectAs<SourcedMeshPayloadError>([&] { decodeSourcedMeshPayload(bytes.data(), bytes.size(), limits); }, SourcedMeshPayloadErrorCode::limitExceeded);
    limits.maximumDecodedBytes = meshSummary.logicalBytes - 1;
    rejectAs<SourcedMeshPayloadError>([&] { decodeSourcedMeshPayload(bytes.data(), bytes.size(), limits); }, SourcedMeshPayloadErrorCode::limitExceeded);
    for (const uint64_t maximum : { uint64_t(sourcedMeshPrefixBytes - 1), uint64_t(sourcedMeshPrefixBytes + meshPrefixBytes - 1), uint64_t(sourcedMeshPrefixBytes + meshPrefixBytes + sourceHintPrefixBytes - 1) }) {
        limits = {};
        limits.maximumWireBytes = maximum;
        rejectAs<SourcedMeshPayloadError>([&] { encodeSourcedMeshPayload(value, limits); }, SourcedMeshPayloadErrorCode::limitExceeded);
    }
    for (const uint64_t invalid : { uint64_t(0), uint64_t(maximumPayloadBytes + 1) }) {
        limits = {};
        limits.maximumWireBytes = invalid;
        rejectAs<SourcedMeshPayloadError>([&] { validateSourcedMeshPayloadLimits(limits); }, SourcedMeshPayloadErrorCode::invalidLimits);
        limits = {};
        limits.maximumDecodedBytes = invalid;
        rejectAs<SourcedMeshPayloadError>([&] { validateSourcedMeshPayloadLimits(limits); }, SourcedMeshPayloadErrorCode::invalidLimits);
    }
}

void preservesInnerFailuresAndRejectsWire()
{
    const auto value = makeFixture();
    const auto original = encodeSourcedMeshPayload(value, {});
    rejectAs<SourcedMeshPayloadError>([&] { decodeSourcedMeshPayload(nullptr, 0, {}); }, SourcedMeshPayloadErrorCode::invalidInput);
    rejectAs<SourcedMeshPayloadError>([&] { decodeSourcedMeshPayload(original.data(), sourcedMeshPrefixBytes - 1, {}); }, SourcedMeshPayloadErrorCode::truncated);
    auto bytes = original;
    setWireField(bytes, 0, 2, 2);
    rejectAs<SourcedMeshPayloadError>([&] { decodeSourcedMeshPayload(bytes.data(), bytes.size(), {}); }, SourcedMeshPayloadErrorCode::unsupportedVersion);
    for (const auto offset : { size_t(2), size_t(4) }) {
        bytes = original;
        setWireField(bytes, offset, 1, offset == 2 ? 2 : 4);
        rejectAs<SourcedMeshPayloadError>([&] { decodeSourcedMeshPayload(bytes.data(), bytes.size(), {}); }, SourcedMeshPayloadErrorCode::invalidLayout);
    }
    bytes = original;
    bytes.pop_back();
    rejectAs<SourcedMeshPayloadError>([&] { decodeSourcedMeshPayload(bytes.data(), bytes.size(), {}); }, SourcedMeshPayloadErrorCode::truncated);
    bytes = original;
    bytes.push_back(0);
    rejectAs<SourcedMeshPayloadError>([&] { decodeSourcedMeshPayload(bytes.data(), bytes.size(), {}); }, SourcedMeshPayloadErrorCode::trailingData);
    for (const auto offset : { size_t(8), size_t(16) }) {
        bytes = original;
        setWireField(bytes, offset, 1, 8);
        rejectAs<SourcedMeshPayloadError>([&] { decodeSourcedMeshPayload(bytes.data(), bytes.size(), {}); }, SourcedMeshPayloadErrorCode::invalidLayout);
    }
    bytes = original;
    setWireField(bytes, sourcedMeshPrefixBytes, 2, 4);
    rejectAs<MeshPayloadError>([&] { decodeSourcedMeshPayload(bytes.data(), bytes.size(), {}); }, MeshPayloadErrorCode::unsupportedVersion);
    const auto mesh = encodeMeshPayload(value.geometry, {});
    bytes = original;
    setWireField(bytes, sourcedMeshPrefixBytes + mesh.size(), 2, 4);
    rejectAs<SourceHintPayloadError>([&] { decodeSourcedMeshPayload(bytes.data(), bytes.size(), {}); }, SourceHintPayloadErrorCode::unsupportedVersion);
    SourcedMeshPayloadLimits limits;
    limits.geometry.maximumDecodedBytes = 0;
    rejectAs<MeshPayloadError>([&] { encodeSourcedMeshPayload(value, limits); }, MeshPayloadErrorCode::invalidLimits);
    limits = {};
    limits.sources.maximumDecodedBytes = 0;
    rejectAs<SourceHintPayloadError>([&] { encodeSourcedMeshPayload(value, limits); }, SourceHintPayloadErrorCode::invalidLimits);
}
}

int main()
{
    try {
        preservesAtomicOwnedPair();
        rejectsMismatchedCorrespondence();
        verifiesAggregateAdmission();
        preservesInnerFailuresAndRejectsWire();
        std::cout << "Worker sourced geometry contracts passed\n";
        return 0;
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}
