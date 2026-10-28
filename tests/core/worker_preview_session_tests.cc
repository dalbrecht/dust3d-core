// SPDX-License-Identifier: MIT
#include <algorithm>
#include <iostream>
#include <limits>
#include <native/Engine/WorkerPreviewSession.h>
#include <native/Engine/WorkerWire.h>
#include <stdexcept>
using namespace dust3d::worker;
namespace {
void require(bool value, const char* cause)
{
    if (!value)
        throw std::runtime_error(cause);
}
template <class Error, class Code, class Action>
void reject(Code code, Action action)
{
    try {
        action();
    } catch (const Error& cause) {
        require(cause.code() == code && cause.what()[0], "Specific preview semantic cause survives");
        return;
    }
    throw std::runtime_error("Invalid preview session accepted");
}
PreviewChunkIdentity identity()
{
    PreviewChunkIdentity result;
    result.request.document[0] = 1;
    result.request.request = 2;
    result.request.revision = 7;
    result.connectionGeneration = 3;
    result.direction = PreviewChunkDirection::session;
    return result;
}
const PreviewArchiveContext context { 37, 0, 0 };
const PreviewChunkPolicy policy { 32768, 4 };
std::vector<uint8_t> frame(const std::vector<uint8_t>& body, PreviewChunkPolicy chunking = policy)
{
    std::vector<uint8_t> result;
    PreviewChunkWriter writer(identity(), body.size(), chunking, [&](const uint8_t* bytes, size_t count) { result.insert(result.end(), bytes, bytes + count); });
    writer.start();
    writer.append(body.data(), body.size());
    writer.finish();
    return result;
}
template <class Error, class Code>
void rejectBody(Code code, const std::vector<uint8_t>& body)
{
    const auto wire = frame(body);
    PreviewSessionCollector owner(identity(), context, policy);
    reject<Error>(code, [&] {
        owner.append(wire.data(), wire.size());
        owner.finishAtEOF();
        owner.take();
    });
    require(owner.isFailed() && !owner.isComplete(), "Rejected semantic body cannot publish a partial result");
    reject<Error>(code, [&] { owner.take(); });
}
SessionOutput roundtrip(const SessionOutput& value, bool isLegacyCompatible = true, PreviewChunkPolicy chunking = policy, PreviewArchiveContext captured = context)
{
    std::vector<uint8_t> wire;
    writePreviewSession(identity(), captured, value, chunking, [&](const uint8_t* bytes, size_t count) { wire.insert(wire.end(), bytes, bytes + count); });
    if (isLegacyCompatible)
        require(wire == frame(encodeSessionOutput(value, {}), chunking), "Canonical small bytes match the independent maintained legacy encoder");
    PreviewSessionCollector owner(identity(), captured, chunking);
    reject<PreviewChunkError>(PreviewChunkErrorCode::invalidSequence, [&] { owner.take(); });
    require(!owner.isFailed(), "Premature ownership query does not mutate collection");
    owner.append(nullptr, 0);
    if (chunking.chunkPayloadBytes == 4) {
        for (size_t index = 0; index < wire.size(); ++index)
            owner.append(wire.data() + index, 1);
    } else
        owner.append(wire.data(), wire.size());
    require(!owner.isComplete(), "Semantic completion requires actual EOF as well as end");
    owner.finishAtEOF();
    require(owner.isComplete() && !owner.isFailed(), "Whole semantic result remains private until framing plus EOF");
    auto result = owner.take();
    std::fill(wire.begin(), wire.end(), 0);
    reject<PreviewChunkError>(PreviewChunkErrorCode::invalidSequence, [&] { owner.take(); });
    require(!owner.isComplete(), "Ownership transfers exactly once");
    if (isLegacyCompatible)
        require(encodeSessionOutput(result, {}) == encodeSessionOutput(value, {}), "Owned exact values survive input overwrite and transfer");
    return result;
}
IndexedMeshPayload mesh()
{
    IndexedMeshPayload result;
    result.vertices = { { { 0, 0, 0 }, { 0, 0, 1 }, { 1, 0, 0, 1 } }, { { 1, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0, 1 } }, { { 0, 1, 0 }, { 0, 0, 1 }, { 0, 0, 1, 1 } } };
    result.indices = { 0, 1, 2 };
    result.nodeCount = result.partCount = 1;
    result.warning = "keep original";
    return result;
}
IndexedSourcesPayload sourced()
{
    SourceHintPayload hints;
    hints.identities = { { "opaque-κ", "part" } };
    hints.influences = { { SourceHintKind::nodeHint, 0, 2.5F }, { SourceHintKind::unknown, noSourceIdentity, 0.25F }, { SourceHintKind::ambiguous, noSourceIdentity, 0.5F } };
    hints.corners = { { SourceHintKind::nodeHint, 0, 1 }, { SourceHintKind::weightedHint, 0, 2 }, { SourceHintKind::ambiguous, 2, 1 } };
    return { mesh(), hints };
}
}
int main()
{
    try {
        SessionOutput value;
        require(std::holds_alternative<std::monostate>(roundtrip(value).outcome), "Successful emptiness does not fabricate a mesh");
        value.statistics = GenerationStatistics {};
        require(roundtrip(value).statistics.has_value(), "Measured zero remains distinct from unavailable statistics");
        value.outcome = mesh();
        value.statistics = GenerationStatistics { 1024, 512, 2, 3, 4, 2, -3, { 0, 255, 'c' } };
        roundtrip(value);
        value.outcome = sourced();
        roundtrip(value);
        for (const auto chunkSize : { 1U, 64U, maximumPreviewChunkPayloadBytes })
            roundtrip(value, true, { policy.maximumTransferBytes, chunkSize });
        auto explicitNative = context;
        explicitNative.maximumIndexedBytes = std::numeric_limits<size_t>::max();
        roundtrip(value, true, policy, explicitNative);
        auto unknown = sourced();
        unknown.geometry.warning.clear();
        unknown.sources.identities.clear();
        unknown.geometry.nodeCount = 0;
        unknown.sources.influences.clear();
        unknown.sources.corners.assign(3, { SourceHintKind::unknown, 0, 0 });
        value.outcome = unknown;
        roundtrip(value);
        auto opaqueKind = sourced();
        opaqueKind.sources.influences[0].kind = static_cast<SourceHintKind>(99);
        opaqueKind.sources.corners[0].kind = static_cast<SourceHintKind>(99);
        value.outcome = opaqueKind;
        roundtrip(value);
        auto multiple = sourced();
        multiple.sources.identities.push_back({ "second-node", "second-part" });
        multiple.geometry.nodeCount = multiple.geometry.partCount = 2;
        multiple.sources.influences[0].identityIndex = 1;
        multiple.geometry.vertices.push_back({ { -1, 1, 0 }, { 0, 0, 1 }, { 1, 1, 1, 1 } });
        multiple.geometry.indices.insert(multiple.geometry.indices.end(), { 0, 2, 3 });
        const auto moreCorners = multiple.sources.corners;
        multiple.sources.corners.insert(multiple.sources.corners.end(), moreCorners.begin(), moreCorners.end());
        value.outcome = multiple;
        roundtrip(value);

        FailurePayload sourceIssue { FailureStatus::generationFailed, { 0, 255, 's' }, FailureDetail { FailureDomain::native, 12, 3, 1 } };
        value.outcome = mesh();
        sourceIssue.context = FailureResourceContext { FailureResourceRole::proceduralImage, "{00000000-0000-0000-0000-000000000001}" };
        value.sourceIssue = sourceIssue;
        roundtrip(value);
        value.statistics.reset();
        value.statisticsIssue = FailurePayload { FailureStatus::generationFailed, { 'o', 'b', 's' }, FailureDetail { FailureDomain::generationStatistics, 7, 3, 0 } };
        roundtrip(value);
        value.outcome = FailurePayload { FailureStatus::unsupportedAsset, { 'o', 'r', 'i', 'g' } };
        roundtrip(value);
        value.outcome = std::monostate {};
        roundtrip(value);
        value = {};
        auto wideSource = sourced();
        wideSource.sources.identities[0].node = std::string(1500, 'n');
        value.outcome = wideSource;
        require(std::get<IndexedSourcesPayload>(roundtrip(value, false).outcome).sources.identities[0].node == wideSource.sources.identities[0].node, "Source text follows native budget rather than incidental1024-byte identity cap");
        auto wideWarning = mesh();
        wideWarning.warning = std::string(5000, 'w');
        value.outcome = wideWarning;
        require(std::get<IndexedMeshPayload>(roundtrip(value, false).outcome).warning == wideWarning.warning, "Original warning text has no incidental4096-byte truncation");
        // Pure scalar accounting only: never allocate these record counts.
        PreviewGeometryShape large { 6000000, 6000000, 0, 0, 0, std::nullopt };
        require(planPreviewRecords(large, {}).nativeIndexedBytes <= maximumPayloadBytes, "Native default profile admits counts above incidental worker defaults");
        std::vector<uint8_t> untouched;
        value.outcome = mesh();
        auto tiny = context;
        tiny.maximumIndexedBytes = 1;
        reject<PreviewRangeError>(PreviewRangeErrorCode::nativeBudgetExceeded, [&] { writePreviewSession(identity(), tiny, value, policy, [&](const uint8_t* bytes, size_t count) { untouched.insert(untouched.end(), bytes, bytes + count); }); });
        require(untouched.empty(), "Invalid native budget rejects before the first callback");
        value.outcome = FailurePayload { FailureStatus::noGeometry, {} };
        reject<SessionOutputError>(SessionOutputErrorCode::invalidOutcome, [&] { writePreviewSession(identity(), context, value, policy, [](const uint8_t*, size_t) { return; }); });
        value.outcome = sourced();
        value.sourceIssue = sourceIssue;
        reject<SessionOutputError>(SessionOutputErrorCode::invalidOutcome, [&] { writePreviewSession(identity(), context, value, policy, [](const uint8_t*, size_t) { return; }); });
        value = {};
        value.outcome = mesh();
        auto body = encodeSessionOutput(value, {});
        body[1] = 2;
        auto badWire = frame(body);
        PreviewSessionCollector unsupported(identity(), context, policy);
        reject<SessionOutputError>(SessionOutputErrorCode::unsupportedVersion, [&] { unsupported.append(badWire.data(), badWire.size()); });
        reject<SessionOutputError>(SessionOutputErrorCode::unsupportedVersion, [&] { unsupported.finishAtEOF(); });
        reject<SessionOutputError>(SessionOutputErrorCode::unsupportedVersion, [&] { unsupported.take(); });
        require(unsupported.isFailed(), "Original semantic failure permanently seals owner");
        body = encodeSessionOutput(value, {});
        detail::writeUnsigned(body.data() + sessionOutputPrefixBytes + indexedMeshPrefixBytes + mesh().warning.size() + mesh().vertices.size() * meshVertexBytes, 99, 4);
        badWire = frame(body);
        PreviewSessionCollector invalidIndex(identity(), context, policy);
        reject<IndexedMeshPayloadError>(IndexedMeshPayloadErrorCode::invalidIndex, [&] { invalidIndex.append(badWire.data(), badWire.size()); });
        body = encodeSessionOutput(value, {});
        body[63] = 1;
        badWire = frame(body);
        PreviewSessionCollector reserved(identity(), context, policy);
        reject<SessionOutputError>(SessionOutputErrorCode::invalidLayout, [&] { reserved.append(badWire.data(), badWire.size()); });
        const auto validWire = frame(encodeSessionOutput(value, {}));
        PreviewSessionCollector truncated(identity(), context, policy);
        truncated.append(validWire.data(), validWire.size() - 1);
        reject<PreviewChunkError>(PreviewChunkErrorCode::invalidSequence, [&] { truncated.finishAtEOF(); });
        PreviewSessionCollector missing(identity(), context, policy);
        reject<PreviewChunkError>(PreviewChunkErrorCode::invalidInput, [&] { missing.append(nullptr, 1); });
        rejectBody<SessionOutputError>(SessionOutputErrorCode::truncated, { 1 });
        const auto ordinary = encodeSessionOutput(value, {});
        body = ordinary;
        detail::writeUnsigned(body.data() + 8, 1, 8);
        rejectBody<SessionOutputError>(SessionOutputErrorCode::invalidLayout, body);
        body = ordinary;
        detail::writeUnsigned(body.data() + 16, 4097, 8);
        body.resize(body.size() + 4097, 0);
        rejectBody<SessionOutputError>(SessionOutputErrorCode::limitExceeded, body);
        body = ordinary;
        detail::writeUnsigned(body.data() + 24, 4097, 8);
        body.resize(body.size() + 4097 - generationStatisticsPrefixBytes, 0);
        rejectBody<SessionOutputError>(SessionOutputErrorCode::limitExceeded, body);
        body = ordinary;
        detail::writeUnsigned(body.data() + 8, std::numeric_limits<uint64_t>::max(), 8);
        rejectBody<PreviewRangeError>(PreviewRangeErrorCode::arithmeticOverflow, body);
        body = ordinary;
        detail::writeUnsigned(body.data() + 4, 99, 4);
        rejectBody<SessionOutputError>(SessionOutputErrorCode::invalidLayout, body);
        body = ordinary;
        detail::writeUnsigned(body.data() + sessionOutputPrefixBytes + 24, 100, 8);
        rejectBody<PreviewRangeError>(PreviewRangeErrorCode::invalidCounts, body);
        body = ordinary;
        detail::writeUnsigned(body.data() + sessionOutputPrefixBytes, 2, 2);
        rejectBody<IndexedMeshPayloadError>(IndexedMeshPayloadErrorCode::unsupportedVersion, body);
        body = ordinary;
        detail::writeUnsigned(body.data() + sessionOutputPrefixBytes + 40, 1, 4);
        rejectBody<IndexedMeshPayloadError>(IndexedMeshPayloadErrorCode::invalidLayout, body);
        body = ordinary;
        detail::writeUnsigned(body.data() + sessionOutputPrefixBytes + 8, 6000000, 8);
        detail::writeUnsigned(body.data() + sessionOutputPrefixBytes + 16, 6000000, 8);
        rejectBody<IndexedMeshPayloadError>(IndexedMeshPayloadErrorCode::invalidLayout, body);
        body = ordinary;
        detail::writeUnsigned(body.data() + sessionOutputPrefixBytes + indexedMeshPrefixBytes + mesh().warning.size(), 0x7f800000U, 4);
        rejectBody<MeshPayloadError>(MeshPayloadErrorCode::invalidGeometry, body);
        body = ordinary;
        body[sessionOutputPrefixBytes + indexedMeshPrefixBytes] = 0;
        rejectBody<MeshPayloadError>(MeshPayloadErrorCode::invalidText, body);
        value.outcome = sourced();
        const auto paired = encodeSessionOutput(value, {});
        const size_t pairOffset = sessionOutputPrefixBytes;
        const size_t sourceOffset = pairOffset + indexedSourcesPrefixBytes + encodeIndexedMeshPayload(sourced().geometry, {}).size();
        const size_t cornerOffset = sourceOffset + sourceHintPrefixBytes + 8 + sourced().sources.identities[0].node.size() + sourced().sources.identities[0].part.size();
        const size_t influenceOffset = cornerOffset + 3 * sourceHintCornerBytes;
        body = paired;
        detail::writeUnsigned(body.data() + pairOffset, 3, 2);
        rejectBody<IndexedSourcesPayloadError>(IndexedSourcesPayloadErrorCode::unsupportedVersion, body);
        body = paired;
        body[pairOffset + 7] = 1;
        rejectBody<IndexedSourcesPayloadError>(IndexedSourcesPayloadErrorCode::invalidLayout, body);
        body = paired;
        detail::writeUnsigned(body.data() + pairOffset + 8, 1, 8);
        rejectBody<IndexedSourcesPayloadError>(IndexedSourcesPayloadErrorCode::invalidLayout, body);
        body = paired;
        body[sourceOffset + 3] = 2;
        rejectBody<SourceHintPayloadError>(SourceHintPayloadErrorCode::unsupportedVersion, body);
        body = paired;
        body[sourceOffset + 7] = 1;
        rejectBody<SourceHintPayloadError>(SourceHintPayloadErrorCode::invalidLayout, body);
        body = paired;
        detail::writeUnsigned(body.data() + sourceOffset + 32, 1, 8);
        rejectBody<SourceHintPayloadError>(SourceHintPayloadErrorCode::invalidLayout, body);
        SessionOutput noIdentifiers;
        noIdentifiers.outcome = unknown;
        body = encodeSessionOutput(noIdentifiers, {});
        const auto noIdsSource = sessionOutputPrefixBytes + indexedSourcesPrefixBytes + encodeIndexedMeshPayload(unknown.geometry, {}).size();
        detail::writeUnsigned(body.data() + 8, detail::readUnsigned(body.data() + 8, 8) + 1, 8);
        detail::writeUnsigned(body.data() + sessionOutputPrefixBytes + 16, detail::readUnsigned(body.data() + sessionOutputPrefixBytes + 16, 8) + 1, 8);
        detail::writeUnsigned(body.data() + noIdsSource + 32, 1, 8);
        body.insert(body.begin() + noIdsSource + sourceHintPrefixBytes, 0);
        rejectBody<SourceHintPayloadError>(SourceHintPayloadErrorCode::invalidLayout, body);
        body = paired;
        detail::writeUnsigned(body.data() + sourceOffset + sourceHintPrefixBytes, 0, 4);
        rejectBody<SourceHintPayloadError>(SourceHintPayloadErrorCode::invalidIdentity, body);
        body = paired;
        detail::writeUnsigned(body.data() + sourceOffset + sourceHintPrefixBytes, 100, 4);
        rejectBody<SourceHintPayloadError>(SourceHintPayloadErrorCode::invalidLayout, body);
        body = paired;
        body[sourceOffset + sourceHintPrefixBytes + 8] = 0;
        rejectBody<SourceHintPayloadError>(SourceHintPayloadErrorCode::invalidIdentity, body);
        body = paired;
        body[cornerOffset + 7] = 1;
        rejectBody<SourceHintPayloadError>(SourceHintPayloadErrorCode::invalidLayout, body);
        body = paired;
        detail::writeUnsigned(body.data() + cornerOffset + 8, 99, 8);
        rejectBody<SourceHintPayloadError>(SourceHintPayloadErrorCode::invalidReference, body);
        body = paired;
        body[influenceOffset + 7] = 1;
        rejectBody<SourceHintPayloadError>(SourceHintPayloadErrorCode::invalidLayout, body);
        body = paired;
        detail::writeUnsigned(body.data() + influenceOffset + 16, 0x7f800000U, 4);
        rejectBody<SourceHintPayloadError>(SourceHintPayloadErrorCode::invalidWeight, body);
        body = paired;
        detail::writeUnsigned(body.data() + influenceOffset + 16, 0xbf800000U, 4);
        rejectBody<SourceHintPayloadError>(SourceHintPayloadErrorCode::invalidWeight, body);
        body = paired;
        detail::writeUnsigned(body.data() + influenceOffset, static_cast<uint32_t>(SourceHintKind::weightedHint), 4);
        rejectBody<SourceHintPayloadError>(SourceHintPayloadErrorCode::invalidReference, body);
        body = paired;
        detail::writeUnsigned(body.data() + cornerOffset + 16, 0, 8);
        rejectBody<SourceHintPayloadError>(SourceHintPayloadErrorCode::invalidCorner, body);
        body = paired;
        detail::writeUnsigned(body.data() + influenceOffset + 8, 99, 8);
        rejectBody<SourceHintPayloadError>(SourceHintPayloadErrorCode::invalidReference, body);
        value = {};
        value.statistics = GenerationStatistics {};
        value.statisticsIssue = FailurePayload {};
        reject<SessionOutputError>(SessionOutputErrorCode::invalidOutcome, [&] { writePreviewSession(identity(), context, value, policy, [](const uint8_t*, size_t) { return; }); });
        value = {};
        value.outcome = mesh();
        value.sourceIssue = FailurePayload { FailureStatus::unsupportedAsset, {} };
        reject<SessionOutputError>(SessionOutputErrorCode::invalidOutcome, [&] { writePreviewSession(identity(), context, value, policy, [](const uint8_t*, size_t) { return; }); });
        value.sourceIssue.reset();
        auto wrongDirection = identity();
        wrongDirection.direction = PreviewChunkDirection::archive;
        reject<PreviewChunkError>(PreviewChunkErrorCode::invalidIdentity, [&] { PreviewSessionCollector invalid(wrongDirection, context, policy); });
        reject<PreviewChunkError>(PreviewChunkErrorCode::invalidIdentity, [&] { writePreviewSession(wrongDirection, context, value, policy, [](const uint8_t*, size_t) { return; }); });
        struct SinkError : std::runtime_error {
            using std::runtime_error::runtime_error;
        };
        try {
            writePreviewSession(identity(), context, value, policy, [](const uint8_t*, size_t) { throw SinkError("original preview sink"); });
            throw std::runtime_error("Sink failure swallowed");
        } catch (const SinkError& cause) {
            require(std::string(cause.what()) == "original preview sink", "Original callback cause survives without OS failure probes");
        }

    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
    return 0;
}
