// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_PREVIEW_CHUNK_H_
#define DUST3D_NATIVE_WORKER_PREVIEW_CHUNK_H_
#include "WorkerProtocol.h"
namespace dust3d {
namespace worker {
    constexpr size_t previewChunkHeaderBytes = 80;
    constexpr uint32_t maximumPreviewChunkPayloadBytes = 65536;
    enum class PreviewChunkDirection : uint16_t { archive = 1,
        session = 2 };
    enum class PreviewChunkKind : uint16_t { begin = 1,
        data = 2,
        end = 3 };
    /// Parent-captured identity. Correlation is not peer authentication.
    struct PreviewChunkIdentity {
        RequestIdentity request;
        uint64_t connectionGeneration = 0;
        PreviewChunkDirection direction = PreviewChunkDirection::archive;
        bool operator==(const PreviewChunkIdentity& other) const noexcept;
    };
    /// Explicit frozen transfer bound and canonical chunk size. No legacy cap is
    /// raised and no total-size allocation follows from selecting this policy.
    struct PreviewChunkPolicy {
        uint64_t maximumTransferBytes;
        uint32_t chunkPayloadBytes = maximumPreviewChunkPayloadBytes;
    };
    struct PreviewChunkHeader {
        PreviewChunkKind kind;
        PreviewChunkIdentity identity;
        uint64_t offset, totalBytes;
        uint32_t payloadBytes;
    };
    struct PreviewChunkingPlan {
        uint64_t dataChunks, frameCount, wireBytes;
    };
    enum class PreviewChunkErrorCode { invalidLimits,
        invalidIdentity,
        invalidHeader,
        unsupportedVersion,
        unsupportedKind,
        unsupportedDirection,
        limitExceeded,
        invalidChunk,
        arithmeticOverflow,
        invalidSequence,
        identityMismatch,
        lengthMismatch,
        payloadMismatch,
        invalidInput };
    class PreviewChunkError : public std::runtime_error {
    public:
        PreviewChunkError(PreviewChunkErrorCode code, const char* cause);
        PreviewChunkErrorCode code() const noexcept { return m_code; }

    private:
        PreviewChunkErrorCode m_code;
    };
    /// Checked physical byte/frame counts including begin/end headers; no buffers
    /// or native work. Throws PreviewChunkError for policy, zero size or overflow.
    /// Example: auto size = measurePreviewChunking(payloadBytes, frozenPolicy);
    PreviewChunkingPlan measurePreviewChunking(uint64_t totalBytes, const PreviewChunkPolicy& policy);
    /// Encode a validated version1 explicit-endian80-byte header into a fixed array.
    /// Begin/end have no body; data chunks are full-sized except the final remainder.
    /// Throws PreviewChunkError before returning bytes. No semantic admission occurs.
    std::array<uint8_t, previewChunkHeaderBytes> encodePreviewChunkHeader(const PreviewChunkHeader& header, const PreviewChunkPolicy& policy);
    /// Read exactly80 stable readable bytes, validate magic/version/reserved fields,
    /// identity, counts and policy before returning copied scalar values. Never
    /// retain input or allocate a transfer-sized buffer. Throws PreviewChunkError.
    PreviewChunkHeader decodePreviewChunkHeader(const uint8_t* bytes, size_t size, const PreviewChunkPolicy& policy);
    /// Collecting-thread framing state only. The caller must supply actual observed
    /// body sizes and real EOF. This validates no body bytes, peer or application
    /// state; complete semantic/host admission remains mandatory before publication.
    class PreviewChunkSequence {
    public:
        PreviewChunkSequence(PreviewChunkIdentity expected, PreviewChunkPolicy policy);
        /// Consume one validated header and actual complete body byte count.
        /// All errors seal the sequence; no newer transfer can reuse this object.
        void consume(const PreviewChunkHeader& header, size_t observedPayloadBytes);
        /// Call only after reading actual EOF; an end marker is insufficient.
        /// Early/repeated EOF throws and seals this framing state.
        void finishAtEOF();
        bool isFramed() const noexcept { return m_state == State::finished; }
        bool isFailed() const noexcept { return m_state == State::failed; }
        uint64_t receivedBytes() const noexcept { return m_receivedBytes; }

    private:
        enum class State { awaitingBegin,
            receiving,
            awaitingEOF,
            finished,
            failed };
        PreviewChunkIdentity m_expected;
        PreviewChunkPolicy m_policy;
        State m_state = State::awaitingBegin;
        uint64_t m_totalBytes = 0, m_receivedBytes = 0;
    };
}
}
#endif
