// SPDX-License-Identifier: MIT
#include <iostream>
#include <limits>
#include <native/Engine/WorkerPreviewChunk.h>
#include <native/Engine/WorkerSessionOutput.h>
#include <stdexcept>
using namespace dust3d::worker;
namespace {
void require(bool value, const char* cause)
{
    if (!value)
        throw std::runtime_error(cause);
}
template <class Action>
void reject(PreviewChunkErrorCode expected, Action action)
{
    try {
        action();
    } catch (const PreviewChunkError& cause) {
        require(cause.code() == expected && cause.what()[0], "Specific chunk cause survives");
        return;
    }
    throw std::runtime_error("Invalid chunk accepted");
}
}
int main()
{
    try {
        PreviewChunkIdentity identity;
        identity.request.document[0] = 1;
        identity.request.request = 2;
        identity.request.revision = 3;
        identity.connectionGeneration = 4;
        identity.direction = PreviewChunkDirection::archive;
        PreviewChunkPolicy policy { 512ULL * 1024 * 1024, 4 };
        PreviewChunkHeader begin { PreviewChunkKind::begin, identity, 0, 9, 0 };
        const auto encoded = encodePreviewChunkHeader(begin, policy);
        const auto decoded = decodePreviewChunkHeader(encoded.data(), encoded.size(), policy);
        require(decoded.identity == identity && decoded.totalBytes == 9 && decoded.offset == 0 && decoded.payloadBytes == 0 && decoded.kind == PreviewChunkKind::begin, "Explicit big-endian header round trip");
        require(encoded.size() == 80 && encoded[31] == 0 && encoded[39] == 2 && encoded[47] == 3 && encoded[55] == 4 && encoded[71] == 9, "Stable independent field offsets");
        const auto size = measurePreviewChunking(9, policy);
        require(size.dataChunks == 3 && size.frameCount == 5 && size.wireBytes == 409, "Canonical chunks include begin/end and every header");
        PreviewChunkSequence sequence(identity, policy);
        sequence.consume(begin, 0);
        for (uint64_t offset : { 0, 4, 8 }) {
            const uint32_t count = offset == 8 ? 1 : 4;
            PreviewChunkHeader data { PreviewChunkKind::data, identity, offset, 9, count };
            const auto bytes = encodePreviewChunkHeader(data, policy);
            sequence.consume(decodePreviewChunkHeader(bytes.data(), bytes.size(), policy), count);
        }
        sequence.consume({ PreviewChunkKind::end, identity, 9, 9, 0 }, 0);
        require(!sequence.isFramed() && sequence.receivedBytes() == 9, "Terminal marker alone is not actual EOF");
        sequence.finishAtEOF();
        require(sequence.isFramed() && !sequence.isFailed(), "Actual EOF completes framing only");
        reject(PreviewChunkErrorCode::invalidSequence, [&] { sequence.consume(begin, 0); });
        require(sequence.isFailed() && !sequence.isFramed(), "Trailing input seals publication state");
        auto large = begin;
        large.totalBytes = 256ULL * 1024 * 1024 + 24;
        require(decodePreviewChunkHeader(encodePreviewChunkHeader(large, policy).data(), 80, policy).totalBytes == large.totalBytes, "Large scalar payload is not narrowed to legacy frame cap");
        const auto largePlan = measurePreviewChunking(large.totalBytes, { policy.maximumTransferBytes, 65536 });
        require(largePlan.dataChunks == 4097 && largePlan.frameCount == 4099, "Full native archive plus prefix accounted without a large allocation");

        for (size_t offset : { 10, 11, 12, 13, 14, 15, 76, 77, 78, 79 }) {
            auto bad = encoded;
            bad[offset] = 1;
            reject(PreviewChunkErrorCode::invalidHeader, [&] { decodePreviewChunkHeader(bad.data(), bad.size(), policy); });
        }
        auto badBytes = encoded;
        badBytes[0] = 0;
        reject(PreviewChunkErrorCode::invalidHeader, [&] { decodePreviewChunkHeader(badBytes.data(), 80, policy); });
        badBytes = encoded;
        badBytes[5] = 2;
        reject(PreviewChunkErrorCode::unsupportedVersion, [&] { decodePreviewChunkHeader(badBytes.data(), 80, policy); });
        reject(PreviewChunkErrorCode::invalidHeader, [&] { decodePreviewChunkHeader(nullptr, 0, policy); });
        reject(PreviewChunkErrorCode::invalidHeader, [&] { decodePreviewChunkHeader(encoded.data(), 79, policy); });
        auto bad = begin;
        bad.kind = static_cast<PreviewChunkKind>(99);
        reject(PreviewChunkErrorCode::unsupportedKind, [&] { encodePreviewChunkHeader(bad, policy); });
        bad = begin;
        bad.identity.direction = static_cast<PreviewChunkDirection>(99);
        reject(PreviewChunkErrorCode::unsupportedDirection, [&] { encodePreviewChunkHeader(bad, policy); });
        bad = begin;
        bad.identity.request.document = {};
        reject(PreviewChunkErrorCode::invalidIdentity, [&] { encodePreviewChunkHeader(bad, policy); });
        bad = begin;
        bad.identity.request.request = 0;
        reject(PreviewChunkErrorCode::invalidIdentity, [&] { encodePreviewChunkHeader(bad, policy); });
        bad = begin;
        bad.identity.connectionGeneration = 0;
        reject(PreviewChunkErrorCode::invalidIdentity, [&] { encodePreviewChunkHeader(bad, policy); });
        reject(PreviewChunkErrorCode::invalidLimits, [&] { measurePreviewChunking(9, { 0, 4 }); });
        reject(PreviewChunkErrorCode::invalidLimits, [&] { measurePreviewChunking(9, { 9, 0 }); });
        reject(PreviewChunkErrorCode::invalidLimits, [&] { measurePreviewChunking(9, { 9, 65537 }); });
        reject(PreviewChunkErrorCode::invalidChunk, [&] { measurePreviewChunking(0, policy); });
        reject(PreviewChunkErrorCode::limitExceeded, [&] { measurePreviewChunking(policy.maximumTransferBytes + 1, policy); });
        for (const auto& header : {
                 PreviewChunkHeader { PreviewChunkKind::begin, identity, 1, 9, 0 },
                 PreviewChunkHeader { PreviewChunkKind::begin, identity, 0, 9, 1 },
                 PreviewChunkHeader { PreviewChunkKind::data, identity, 0, 9, 1 },
                 PreviewChunkHeader { PreviewChunkKind::data, identity, 1, 9, 4 },
                 PreviewChunkHeader { PreviewChunkKind::data, identity, 9, 9, 1 },
                 PreviewChunkHeader { PreviewChunkKind::end, identity, 8, 9, 0 },
                 PreviewChunkHeader { PreviewChunkKind::end, identity, 9, 9, 1 } })
            reject(PreviewChunkErrorCode::invalidChunk, [&] { encodePreviewChunkHeader(header, policy); });
        for (int field = 0; field < 5; ++field) {
            auto foreign = begin;
            if (field == 0)
                foreign.identity.request.document[0] = 2;
            if (field == 1)
                ++foreign.identity.request.request;
            if (field == 2)
                ++foreign.identity.request.revision;
            if (field == 3)
                ++foreign.identity.connectionGeneration;
            if (field == 4)
                foreign.identity.direction = PreviewChunkDirection::session;
            PreviewChunkSequence current(identity, policy);
            reject(PreviewChunkErrorCode::identityMismatch, [&] { current.consume(foreign, 0); });
            require(current.isFailed(), "Foreign transfer seals sequence");
        }
        PreviewChunkSequence gap(identity, policy);
        gap.consume(begin, 0);
        reject(PreviewChunkErrorCode::invalidSequence, [&] { gap.consume({ PreviewChunkKind::data, identity, 4, 9, 4 }, 4); });
        reject(PreviewChunkErrorCode::invalidSequence, [&] { gap.consume(begin, 0); });
        PreviewChunkSequence changed(identity, policy);
        changed.consume(begin, 0);
        reject(PreviewChunkErrorCode::lengthMismatch, [&] { changed.consume({ PreviewChunkKind::data, identity, 0, 10, 4 }, 4); });
        PreviewChunkSequence truncated(identity, policy);
        truncated.consume(begin, 0);
        reject(PreviewChunkErrorCode::payloadMismatch, [&] { truncated.consume({ PreviewChunkKind::data, identity, 0, 9, 4 }, 3); });
        PreviewChunkSequence missingBegin(identity, policy);
        reject(PreviewChunkErrorCode::invalidSequence, [&] { missingBegin.consume({ PreviewChunkKind::data, identity, 0, 9, 4 }, 4); });
        PreviewChunkSequence duplicateBegin(identity, policy);
        duplicateBegin.consume(begin, 0);
        reject(PreviewChunkErrorCode::invalidSequence, [&] { duplicateBegin.consume(begin, 0); });
        PreviewChunkSequence earlyEnd(identity, policy);
        earlyEnd.consume(begin, 0);
        reject(PreviewChunkErrorCode::invalidSequence, [&] { earlyEnd.consume({ PreviewChunkKind::end, identity, 9, 9, 0 }, 0); });
        PreviewChunkSequence duplicateData(identity, policy);
        duplicateData.consume(begin, 0);
        duplicateData.consume({ PreviewChunkKind::data, identity, 0, 9, 4 }, 4);
        reject(PreviewChunkErrorCode::invalidSequence, [&] { duplicateData.consume({ PreviewChunkKind::data, identity, 0, 9, 4 }, 4); });
        PreviewChunkSequence afterEnd(identity, policy);
        afterEnd.consume({ PreviewChunkKind::begin, identity, 0, 4, 0 }, 0);
        afterEnd.consume({ PreviewChunkKind::data, identity, 0, 4, 4 }, 4);
        afterEnd.consume({ PreviewChunkKind::end, identity, 4, 4, 0 }, 0);
        reject(PreviewChunkErrorCode::invalidSequence, [&] { afterEnd.consume(begin, 0); });
        PreviewChunkSequence early(identity, policy);
        reject(PreviewChunkErrorCode::invalidSequence, [&] { early.finishAtEOF(); });
        PreviewChunkSequence unfinished(identity, policy);
        unfinished.consume(begin, 0);
        reject(PreviewChunkErrorCode::invalidSequence, [&] { unfinished.finishAtEOF(); });
        if (sizeof(size_t) == 8) {
            reject(PreviewChunkErrorCode::arithmeticOverflow, [&] { measurePreviewChunking(std::numeric_limits<uint64_t>::max() / 2, { std::numeric_limits<uint64_t>::max(), 1 }); });
            reject(PreviewChunkErrorCode::arithmeticOverflow, [&] { measurePreviewChunking(std::numeric_limits<uint64_t>::max() - 1, { std::numeric_limits<uint64_t>::max(), 1 }); });
        }

        // Tiny complete existing payload; this tests framing, not native generation.
        const auto body = encodeSessionOutput({}, {});
        auto sessionIdentity = identity;
        sessionIdentity.direction = PreviewChunkDirection::session;
        PreviewChunkSequence session(sessionIdentity, policy);
        session.consume({ PreviewChunkKind::begin, sessionIdentity, 0, body.size(), 0 }, 0);
        for (size_t offset = 0; offset < body.size(); offset += 4)
            session.consume({ PreviewChunkKind::data, sessionIdentity, offset, body.size(), 4 }, 4);
        session.consume({ PreviewChunkKind::end, sessionIdentity, body.size(), body.size(), 0 }, 0);
        session.finishAtEOF();
        require(session.isFramed() && std::holds_alternative<std::monostate>(decodeSessionOutput(body.data(), body.size(), {}).outcome), "Framed tiny session still requires independent semantic decoding");
        reject(PreviewChunkErrorCode::invalidSequence, [&] { session.finishAtEOF(); });
        require(session.isFailed(), "Repeated EOF seals the original state");
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
    return 0;
}
