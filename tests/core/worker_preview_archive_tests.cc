// SPDX-License-Identifier: MIT
#include <iostream>
#include <limits>
#include <native/Engine/WorkerPreviewArchive.h>
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
        require(cause.code() == code && cause.what()[0], "Original typed archive cause survives");
        return;
    }
    throw std::runtime_error("Invalid preview archive accepted");
}
std::vector<uint8_t> frame(const PreviewChunkIdentity& identity, const PreviewChunkPolicy& policy, const std::vector<uint8_t>& body)
{
    std::vector<uint8_t> result;
    PreviewChunkWriter writer(identity, body.size(), policy, [&](const uint8_t* bytes, size_t size) { result.insert(result.end(), bytes, bytes + size); });
    writer.start();
    writer.append(body.data(), body.size());
    writer.finish();
    return result;
}
}
int main()
{
    try {
        PreviewChunkIdentity identity;
        identity.request.document[0] = 1;
        identity.request.request = 2;
        identity.connectionGeneration = 3;
        const PreviewChunkPolicy policy { 1024, 4 };
        const std::vector<uint8_t> archive { 4, 5, 6, 7, 8, 9, 10 };
        PreviewArchiveContext context { archive.size(), 0, 0 };
        auto payload = encodeSourceRequestView({ archive.data(), archive.size(), 0 }, {});
        auto wire = frame(identity, policy, payload);
        PreviewArchiveCollector collector(identity, context, policy);
        reject<PreviewChunkError>(PreviewChunkErrorCode::invalidSequence, [&] { collector.view(); });
        require(!collector.isFailed(), "Reading an incomplete view does not mutate collection");
        collector.append(nullptr, 0);
        for (size_t offset = 0; offset < wire.size(); ++offset)
            collector.append(wire.data() + offset, 1);
        reject<PreviewChunkError>(PreviewChunkErrorCode::invalidSequence, [&] { collector.view(); });
        require(!collector.isComplete(), "End marker alone cannot expose an archive");
        collector.finishAtEOF();
        auto view = collector.view();
        std::fill(payload.begin(), payload.end(), 0);
        std::fill(wire.begin(), wire.end(), 0);
        require(collector.isComplete() && !collector.isFailed() && view.source.archiveBytes == archive.size()
                && std::equal(archive.begin(), archive.end(), view.source.archive) && !view.source.maximumSourceBytes && !view.maximumIndexedBytes,
            "Archive is independently owned and original zero policies survive real EOF");
        reject<PreviewChunkError>(PreviewChunkErrorCode::invalidSequence, [&] { collector.finishAtEOF(); });
        require(collector.isFailed() && std::equal(archive.begin(), archive.end(), view.source.archive), "A later rejected operation does not invalidate already borrowed storage");
        reject<PreviewChunkError>(PreviewChunkErrorCode::invalidSequence, [&] { collector.view(); });
        const auto largeIndexed = maximumPayloadBytes + 1;
        const auto prefix = encodeSourceRequestPrefix({ maximumPayloadBytes, maximumPayloadBytes });
        const auto metadata = inspectSourceRequestPrefix(prefix.data(), prefix.size());
        require(metadata.archiveBytes == maximumPayloadBytes && metadata.maximumSourceBytes == maximumPayloadBytes, "Native maximum archive prefix is representable without allocating its body");
        reject<SourceRequestError>(SourceRequestErrorCode::limitExceeded, [&] { decodeSourceRequest(prefix.data(), prefix.size(), { maximumPayloadBytes, maximumPayloadBytes }); });
        PreviewArchiveCollector range(identity, { maximumPayloadBytes, maximumPayloadBytes, largeIndexed }, { maximumPayloadBytes + sourceRequestPrefixBytes, 4 });
        require(!range.isComplete() && !range.isFailed(), "Full native archive range and positive indexed policies above default need no large allocation at construction");
        for (const auto chunkSize : { 1U, 24U, 64U }) {
            const PreviewChunkPolicy splitPolicy { 1024, chunkSize };
            const auto bytes = encodeSourceRequestView({ archive.data(), archive.size(), maximumPayloadBytes }, {});
            const auto framed = frame(identity, splitPolicy, bytes);
            PreviewArchiveContext captured { archive.size(), maximumPayloadBytes, largeIndexed };
            PreviewArchiveCollector split(identity, captured, splitPolicy);
            captured = { 1, 2, 3 };
            split.append(framed.data(), framed.size());
            split.finishAtEOF();
            const auto borrowed = split.view();
            require(borrowed.maximumIndexedBytes == largeIndexed && borrowed.source.maximumSourceBytes == maximumPayloadBytes
                    && std::equal(archive.begin(), archive.end(), borrowed.source.archive),
                "Prefix and archive work split across chunks or combined inside one callback");
        }
        reject<SourceRequestError>(SourceRequestErrorCode::invalidInput, [&] { encodeSourceRequestPrefix({ 0, 0 }); });
        reject<SourceRequestError>(SourceRequestErrorCode::invalidOptions, [&] { encodeSourceRequestPrefix({ 1, maximumPayloadBytes + 1 }); });
        reject<SourceRequestError>(SourceRequestErrorCode::limitExceeded, [&] { encodeSourceRequestPrefix({ std::numeric_limits<uint64_t>::max(), 0 }); });
        reject<SourceRequestError>(SourceRequestErrorCode::invalidInput, [&] { inspectSourceRequestPrefix(nullptr, prefix.size()); });
        reject<SourceRequestError>(SourceRequestErrorCode::truncated, [&] { inspectSourceRequestPrefix(prefix.data(), prefix.size() - 1); });
        auto extra = std::vector<uint8_t>(prefix.begin(), prefix.end());
        extra.push_back(0);
        reject<SourceRequestError>(SourceRequestErrorCode::trailingData, [&] { inspectSourceRequestPrefix(extra.data(), extra.size()); });
        auto changed = prefix;
        changed[3] = 2;
        reject<SourceRequestError>(SourceRequestErrorCode::unsupportedVersion, [&] { inspectSourceRequestPrefix(changed.data(), changed.size()); });
        changed = prefix;
        changed[7] = 1;
        reject<SourceRequestError>(SourceRequestErrorCode::invalidLayout, [&] { inspectSourceRequestPrefix(changed.data(), changed.size()); });
        reject<PreviewChunkError>(PreviewChunkErrorCode::invalidLimits, [&] { PreviewArchiveCollector invalid(identity, { 0, 0, 0 }, policy); });
        reject<PreviewChunkError>(PreviewChunkErrorCode::invalidLimits, [&] { PreviewArchiveCollector invalid(identity, { maximumPayloadBytes + 1, 0, 0 }, policy); });
        reject<PreviewChunkError>(PreviewChunkErrorCode::invalidLimits, [&] { PreviewArchiveCollector invalid(identity, { 1, maximumPayloadBytes + 1, 0 }, policy); });
        auto foreign = identity;
        foreign.direction = PreviewChunkDirection::session;
        reject<PreviewChunkError>(PreviewChunkErrorCode::invalidIdentity, [&] { PreviewArchiveCollector invalid(foreign, context, policy); });
        reject<PreviewChunkError>(PreviewChunkErrorCode::limitExceeded, [&] { PreviewArchiveCollector invalid(identity, context, { sourceRequestPrefixBytes, 4 }); });
        reject<PreviewChunkError>(PreviewChunkErrorCode::invalidLimits, [&] { PreviewArchiveCollector invalid(identity, context, { 1024, 65537 }); });
        const auto small = encodeSourceRequestView({ archive.data(), archive.size(), 0 }, {});
        const auto compatiblePrefix = encodeSourceRequestPrefix({ archive.size(), 0 });
        require(std::equal(compatiblePrefix.begin(), compatiblePrefix.end(), small.begin()), "Standalone prefix remains byte-compatible with the legacy source request");
        const auto validWire = frame(identity, policy, small);
        PreviewArchiveCollector truncated(identity, context, policy);
        truncated.append(validWire.data(), validWire.size() - 1);
        reject<PreviewChunkError>(PreviewChunkErrorCode::invalidSequence, [&] { truncated.finishAtEOF(); });
        reject<PreviewChunkError>(PreviewChunkErrorCode::invalidSequence, [&] { truncated.view(); });
        PreviewArchiveCollector missing(identity, context, policy);
        reject<PreviewChunkError>(PreviewChunkErrorCode::invalidInput, [&] { missing.append(nullptr, 1); });
        reject<PreviewChunkError>(PreviewChunkErrorCode::invalidInput, [&] { missing.finishAtEOF(); });
        reject<PreviewChunkError>(PreviewChunkErrorCode::invalidInput, [&] { missing.append(validWire.data(), validWire.size()); });
        auto trailing = validWire;
        trailing.push_back(1);
        PreviewArchiveCollector overlong(identity, context, policy);
        reject<PreviewChunkError>(PreviewChunkErrorCode::invalidSequence, [&] { overlong.append(trailing.data(), trailing.size()); });
        auto otherIdentity = identity;
        ++otherIdentity.connectionGeneration;
        PreviewArchiveCollector wrongPeerScope(identity, context, policy);
        const auto wrongWire = frame(otherIdentity, policy, small);
        reject<PreviewChunkError>(PreviewChunkErrorCode::identityMismatch, [&] { wrongPeerScope.append(wrongWire.data(), wrongWire.size()); });
        PreviewArchiveCollector wrongTotal(identity, context, policy);
        const auto shorter = frame(identity, policy, { 1 });
        reject<PreviewChunkError>(PreviewChunkErrorCode::lengthMismatch, [&] { wrongTotal.append(shorter.data(), shorter.size()); });
        auto wrongCount = small;
        wrongCount[23] = 6;
        PreviewArchiveCollector mismatchedArchive(identity, context, policy);
        const auto wrongCountWire = frame(identity, policy, wrongCount);
        reject<PreviewChunkError>(PreviewChunkErrorCode::payloadMismatch, [&] { mismatchedArchive.append(wrongCountWire.data(), wrongCountWire.size()); });
        auto wrongBudget = small;
        wrongBudget[15] = 1;
        PreviewArchiveCollector mismatchedBudget(identity, context, policy);
        const auto wrongBudgetWire = frame(identity, policy, wrongBudget);
        reject<PreviewChunkError>(PreviewChunkErrorCode::payloadMismatch, [&] { mismatchedBudget.append(wrongBudgetWire.data(), wrongBudgetWire.size()); });
        auto badVersion = small;
        badVersion[3] = 2;
        PreviewArchiveCollector unsupported(identity, context, policy);
        const auto badVersionWire = frame(identity, policy, badVersion);
        reject<SourceRequestError>(SourceRequestErrorCode::unsupportedVersion, [&] { unsupported.append(badVersionWire.data(), badVersionWire.size()); });
        reject<SourceRequestError>(SourceRequestErrorCode::unsupportedVersion, [&] { unsupported.finishAtEOF(); });
        reject<SourceRequestError>(SourceRequestErrorCode::unsupportedVersion, [&] { unsupported.view(); });
        require(unsupported.isFailed() && !unsupported.isComplete(), "Semantic prefix failure permanently seals the archive owner with its original cause");
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
    return 0;
}
