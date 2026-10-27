// SPDX-License-Identifier: MIT
#include "WorkerPreviewChunk.h"
#include <algorithm>
#include <limits>
namespace dust3d {
namespace worker {
    namespace {
        [[noreturn]] void reject(PreviewChunkErrorCode code, const char* cause) { throw PreviewChunkError(code, cause); }
        uint64_t add(uint64_t left, uint64_t right)
        {
            if (right > std::numeric_limits<uint64_t>::max() - left)
                reject(PreviewChunkErrorCode::arithmeticOverflow, "Preview framing byte count overflows UInt64.");
            return left + right;
        }
        uint64_t multiply(uint64_t count, uint64_t unit)
        {
            if (count > std::numeric_limits<uint64_t>::max() / unit)
                reject(PreviewChunkErrorCode::arithmeticOverflow, "Preview framing header charge overflows UInt64.");
            return count * unit;
        }
        void validatePolicy(const PreviewChunkPolicy& policy)
        {
            if (!policy.maximumTransferBytes || policy.maximumTransferBytes > std::numeric_limits<size_t>::max() || !policy.chunkPayloadBytes || policy.chunkPayloadBytes > maximumPreviewChunkPayloadBytes)
                reject(PreviewChunkErrorCode::invalidLimits, "Preview chunk policy is invalid or unaddressable.");
        }
        void validateIdentity(const PreviewChunkIdentity& identity)
        {
            if (identity.direction != PreviewChunkDirection::archive && identity.direction != PreviewChunkDirection::session)
                reject(PreviewChunkErrorCode::unsupportedDirection, "Preview chunk direction is unsupported.");
            if (!identity.request.request || !identity.connectionGeneration || !std::any_of(identity.request.document.begin(), identity.request.document.end(), [](uint8_t byte) { return byte != 0; }))
                reject(PreviewChunkErrorCode::invalidIdentity, "Preview chunk identity must be independently nonzero.");
        }
        void validateHeader(const PreviewChunkHeader& header, const PreviewChunkPolicy& policy)
        {
            (void)measurePreviewChunking(header.totalBytes, policy);
            validateIdentity(header.identity);
            switch (header.kind) {
            case PreviewChunkKind::begin:
                if (header.offset || header.payloadBytes)
                    reject(PreviewChunkErrorCode::invalidChunk, "Preview begin marker has no offset/body.");
                break;
            case PreviewChunkKind::data:
                if (header.offset >= header.totalBytes || header.offset % policy.chunkPayloadBytes || header.payloadBytes != std::min<uint64_t>(policy.chunkPayloadBytes, header.totalBytes - header.offset))
                    reject(PreviewChunkErrorCode::invalidChunk, "Preview data chunk is not a canonical bounded range.");
                break;
            case PreviewChunkKind::end:
                if (header.offset != header.totalBytes || header.payloadBytes)
                    reject(PreviewChunkErrorCode::invalidChunk, "Preview end marker must follow the exact body.");
                break;
            default:
                reject(PreviewChunkErrorCode::unsupportedKind, "Preview chunk kind is unsupported.");
            }
        }
        void write(std::array<uint8_t, previewChunkHeaderBytes>& bytes, size_t offset, size_t width, uint64_t value)
        {
            for (size_t index = 0; index < width; ++index) {
                bytes[offset + width - index - 1] = static_cast<uint8_t>(value & 255);
                value >>= 8;
            }
        }
        uint64_t read(const uint8_t* bytes, size_t offset, size_t width)
        {
            uint64_t value = 0;
            for (size_t index = 0; index < width; ++index)
                value = (value << 8) | bytes[offset + index];
            return value;
        }
    }
    PreviewChunkError::PreviewChunkError(PreviewChunkErrorCode code, const char* cause)
        : std::runtime_error(cause)
        , m_code(code)
    {
    }
    bool PreviewChunkIdentity::operator==(const PreviewChunkIdentity& other) const noexcept
    {
        return request == other.request && connectionGeneration == other.connectionGeneration && direction == other.direction;
    }
    PreviewChunkingPlan measurePreviewChunking(uint64_t totalBytes, const PreviewChunkPolicy& policy)
    {
        validatePolicy(policy);
        if (!totalBytes)
            reject(PreviewChunkErrorCode::invalidChunk, "Preview transfer requires a nonempty encoded payload.");
        if (totalBytes > policy.maximumTransferBytes)
            reject(PreviewChunkErrorCode::limitExceeded, "Preview transfer exceeds its frozen logical bound.");
        const auto chunks = add(totalBytes / policy.chunkPayloadBytes, totalBytes % policy.chunkPayloadBytes ? 1 : 0);
        const auto frames = add(chunks, 2);
        return { chunks, frames, add(totalBytes, multiply(frames, previewChunkHeaderBytes)) };
    }
    std::array<uint8_t, previewChunkHeaderBytes> encodePreviewChunkHeader(const PreviewChunkHeader& header, const PreviewChunkPolicy& policy)
    {
        validateHeader(header, policy);
        std::array<uint8_t, previewChunkHeaderBytes> bytes {};
        bytes[0] = 'D';
        bytes[1] = '3';
        bytes[2] = 'P';
        bytes[3] = 'C';
        write(bytes, 4, 2, 1);
        write(bytes, 6, 2, static_cast<uint16_t>(header.kind));
        write(bytes, 8, 2, static_cast<uint16_t>(header.identity.direction));
        std::copy(header.identity.request.document.begin(), header.identity.request.document.end(), bytes.begin() + 16);
        write(bytes, 32, 8, header.identity.request.request);
        write(bytes, 40, 8, header.identity.request.revision);
        write(bytes, 48, 8, header.identity.connectionGeneration);
        write(bytes, 56, 8, header.offset);
        write(bytes, 64, 8, header.totalBytes);
        write(bytes, 72, 4, header.payloadBytes);
        return bytes;
    }
    PreviewChunkHeader decodePreviewChunkHeader(const uint8_t* bytes, size_t size, const PreviewChunkPolicy& policy)
    {
        if (!bytes || size != previewChunkHeaderBytes || bytes[0] != 'D' || bytes[1] != '3' || bytes[2] != 'P' || bytes[3] != 'C')
            reject(PreviewChunkErrorCode::invalidHeader, "Preview chunk requires one exact readable80-byte header.");
        if (read(bytes, 4, 2) != 1)
            reject(PreviewChunkErrorCode::unsupportedVersion, "Preview chunk version is unsupported.");
        for (size_t offset : { 10, 11, 12, 13, 14, 15, 76, 77, 78, 79 })
            if (bytes[offset])
                reject(PreviewChunkErrorCode::invalidHeader, "Preview chunk reserved bytes must be zero.");
        PreviewChunkHeader header {};
        header.kind = static_cast<PreviewChunkKind>(read(bytes, 6, 2));
        header.identity.direction = static_cast<PreviewChunkDirection>(read(bytes, 8, 2));
        std::copy_n(bytes + 16, 16, header.identity.request.document.begin());
        header.identity.request.request = read(bytes, 32, 8);
        header.identity.request.revision = read(bytes, 40, 8);
        header.identity.connectionGeneration = read(bytes, 48, 8);
        header.offset = read(bytes, 56, 8);
        header.totalBytes = read(bytes, 64, 8);
        header.payloadBytes = static_cast<uint32_t>(read(bytes, 72, 4));
        validateHeader(header, policy);
        return header;
    }
    PreviewChunkSequence::PreviewChunkSequence(PreviewChunkIdentity expected, PreviewChunkPolicy policy)
        : m_expected(expected)
        , m_policy(policy)
    {
        validatePolicy(policy);
        validateIdentity(expected);
    }
    void PreviewChunkSequence::consume(const PreviewChunkHeader& header, size_t observedPayloadBytes)
    {
        try {
            if (m_state == State::failed || m_state == State::finished || m_state == State::awaitingEOF)
                reject(PreviewChunkErrorCode::invalidSequence, "Preview framing no longer accepts chunks.");
            validateHeader(header, m_policy);
            if (!(header.identity == m_expected))
                reject(PreviewChunkErrorCode::identityMismatch, "Preview chunk differs from independently captured identity.");
            if (observedPayloadBytes != header.payloadBytes)
                reject(PreviewChunkErrorCode::payloadMismatch, "Preview chunk body is incomplete or has trailing bytes.");
            if (m_state == State::awaitingBegin) {
                if (header.kind != PreviewChunkKind::begin)
                    reject(PreviewChunkErrorCode::invalidSequence, "Preview framing requires its begin marker first.");
                m_totalBytes = header.totalBytes;
                m_state = State::receiving;
                return;
            }
            if (header.totalBytes != m_totalBytes)
                reject(PreviewChunkErrorCode::lengthMismatch, "Preview transfer length changed after begin.");
            if (header.kind == PreviewChunkKind::data && header.offset == m_receivedBytes) {
                m_receivedBytes = add(m_receivedBytes, header.payloadBytes);
                return;
            }
            if (header.kind == PreviewChunkKind::end && m_receivedBytes == m_totalBytes) {
                m_state = State::awaitingEOF;
                return;
            }
            reject(PreviewChunkErrorCode::invalidSequence, "Preview chunks are missing, repeated or out of order.");
        } catch (...) {
            m_state = State::failed;
            throw;
        }
    }
    void PreviewChunkSequence::finishAtEOF()
    {
        if (m_state != State::awaitingEOF) {
            m_state = State::failed;
            reject(PreviewChunkErrorCode::invalidSequence, "Actual EOF must follow one complete preview end marker.");
        }
        m_state = State::finished;
    }
}
}
