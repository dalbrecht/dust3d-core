// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_PREVIEW_CHUNK_STREAM_H_
#define DUST3D_NATIVE_WORKER_PREVIEW_CHUNK_STREAM_H_
#include "WorkerPreviewChunk.h"
#include <exception>
#include <functional>
#include <optional>
namespace dust3d {
namespace worker {
    namespace detail {
        /// Internal first-cause retention and nonreentrant callback guard.
        class PreviewStreamStatus {
        public:
            void check() const;
            void capture() noexcept;
            bool isFailed() const noexcept { return bool(m_failure); }
            template <class Callback>
            void invoke(Callback&& callback)
            {
                m_insideSink = true;
                try {
                    callback();
                } catch (...) {
                    m_insideSink = false;
                    throw;
                }
                m_insideSink = false;
                // A callback may have caught a rejected recursive endpoint call.
                // Retain that first cause and prevent the outer operation succeeding.
                if (m_failure)
                    std::rethrow_exception(m_failure);
            }

        private:
            std::exception_ptr m_failure;
            bool m_insideSink = false;
        };
    }
    /// Consume/copy borrowed bytes completely before returning, or throw the
    /// original error. No pointer retention or reentrant endpoint calls.
    using PreviewChunkWriteSink = std::function<void(const uint8_t*, size_t)>;
    /// Header/body callbacks are provisional until real EOF and semantic admission.
    /// Copy/consume a borrowed body before return; controls have null/zero body.
    /// No pointer retention, publication, native dispatch or reentrant calls.
    using PreviewChunkReadSink = std::function<void(const PreviewChunkHeader&, const uint8_t*, size_t)>;

    /// Collecting-thread writer retaining only one frozen-size payload workspace.
    /// Does no I/O itself. Caller retains readable append spans for each call and
    /// finalizes the actual channel after finish; an end marker is not physical EOF.
    class PreviewChunkWriter {
    public:
        PreviewChunkWriter(PreviewChunkIdentity identity, uint64_t totalBytes, PreviewChunkPolicy policy, PreviewChunkWriteSink sink);
        PreviewChunkWriter(const PreviewChunkWriter&) = delete;
        PreviewChunkWriter& operator=(const PreviewChunkWriter&) = delete;
        /// Emit exactly one begin marker. All mutation errors seal the endpoint.
        void start();
        /// Accept arbitrary small readable spans, buffering canonical chunks.
        /// Empty/null is a no-op; null/nonempty or total overrun rejects before copy.
        void append(const uint8_t* bytes, size_t size);
        /// Require the exact promised body; emit final remainder and end marker.
        /// Never closes a descriptor or claims real EOF. Throws original sink errors.
        void finish();
        bool isFinished() const noexcept { return m_state == State::finished && !m_status.isFailed(); }
        bool isFailed() const noexcept { return m_status.isFailed(); }

    private:
        void emitData();
        enum class State { ready,
            writing,
            finished };
        PreviewChunkIdentity m_identity;
        uint64_t m_totalBytes;
        PreviewChunkPolicy m_policy;
        std::array<uint8_t, previewChunkHeaderBytes> m_begin;
        PreviewChunkWriteSink m_sink;
        std::vector<uint8_t> m_workspace;
        detail::PreviewStreamStatus m_status;
        State m_state = State::ready;
        uint64_t m_receivedBytes = 0, m_emittedBytes = 0;
        size_t m_used = 0;
    };
    /// Incremental byte reader with80-byte header plus one policy-sized workspace.
    /// Decode errors seal this owner too, before a caller can accidentally resume.
    /// It retains no whole transfer and performs no payload semantic/peer admission.
    class PreviewChunkReader {
    public:
        PreviewChunkReader(PreviewChunkIdentity expected, PreviewChunkPolicy policy, PreviewChunkReadSink sink);
        PreviewChunkReader(const PreviewChunkReader&) = delete;
        PreviewChunkReader& operator=(const PreviewChunkReader&) = delete;
        /// Ingest actual readable bytes, including arbitrarily split headers/bodies.
        /// Callback bytes expire on return. All errors retain their first cause.
        void append(const uint8_t* bytes, size_t size);
        /// Call only on actual channel EOF. Partial/trailing input cannot publish.
        /// Framing still does not establish semantic/current-source admission.
        void finishAtEOF();
        bool isFramed() const noexcept { return !m_status.isFailed() && m_sequence.isFramed(); }
        bool isFailed() const noexcept { return m_status.isFailed(); }

    private:
        void emitPending();
        PreviewChunkIdentity m_expected;
        PreviewChunkPolicy m_policy;
        PreviewChunkSequence m_sequence;
        PreviewChunkReadSink m_sink;
        std::vector<uint8_t> m_workspace;
        detail::PreviewStreamStatus m_status;
        std::array<uint8_t, previewChunkHeaderBytes> m_header {};
        std::optional<PreviewChunkHeader> m_pending;
        size_t m_headerUsed = 0, m_bodyUsed = 0;
        bool m_ended = false;
    };
}
}
#endif
