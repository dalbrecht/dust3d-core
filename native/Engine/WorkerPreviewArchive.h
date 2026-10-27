// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_PREVIEW_ARCHIVE_H_
#define DUST3D_NATIVE_WORKER_PREVIEW_ARCHIVE_H_
#include "WorkerPreviewChunkStream.h"
#include "WorkerSourceRequest.h"
#include <memory>
namespace dust3d {
namespace worker {
    /// Independently captured request controls, not fields trusted from the stream.
    /// Archive is1..256MiB. Zero source/indexed budgets retain native defaults;
    /// source is at most256MiB, explicit indexed policies may exceed that default.
    struct PreviewArchiveContext {
        uint64_t archiveBytes, maximumSourceBytes, maximumIndexedBytes;
    };
    namespace detail {
        /// Shared scalar preflight for collector and channel sender. Returns the
        /// frozen value or throws PreviewChunkError, without archive allocation.
        PreviewArchiveContext checkedPreviewArchiveContext(PreviewArchiveContext context);
    }
    /// Archive storage is borrowed until collector destruction. No archive parsing,
    /// peer authentication, current-source admission or native dispatch is implied.
    struct PreviewArchiveView {
        SourceRequestView source;
        uint64_t maximumIndexedBytes;
    };
    /// Collecting-thread owned request body for the D3PC archive direction.
    /// Validates captured controls and fixed prefix before allocating exactly the
    /// captured archive size. Retains that archive,24-byte prefix and one bounded
    /// reader workspace, never a second whole envelope. No OS I/O or native work.
    class PreviewArchiveCollector {
    public:
        /// Freeze independent identity/context/policy; reject before archive retention.
        /// Throws PreviewChunkError for invalid captured limits/direction/identity.
        /// Example: PreviewArchiveCollector owner(identity, captured, frozenPolicy);
        PreviewArchiveCollector(PreviewChunkIdentity identity, PreviewArchiveContext context, PreviewChunkPolicy policy);
        PreviewArchiveCollector(const PreviewArchiveCollector&) = delete;
        PreviewArchiveCollector& operator=(const PreviewArchiveCollector&) = delete;
        /// Ingest stable readable bytes (borrowed for the call), arbitrarily split.
        /// All mutation failures seal this owner and preserve their original cause.
        void append(const uint8_t* bytes, size_t size);
        /// Call only at real EOF; a terminal marker alone never exposes the archive.
        /// Throws original framing/prefix/allocation error; no native execution.
        void finishAtEOF();
        /// Borrow immutable owned bytes only after complete framing plus actual EOF.
        /// An incomplete query throws PreviewChunkError without mutating collection;
        /// a failed owner rethrows its original cause. Pointer expires at destruction.
        /// Example: auto archive = owner.view(); // after finishAtEOF()
        PreviewArchiveView view() const;
        bool isComplete() const noexcept { return !m_status.isFailed() && m_reader.isFramed(); }
        bool isFailed() const noexcept { return m_status.isFailed(); }

    private:
        void consume(const PreviewChunkHeader& header, const uint8_t* bytes, size_t size);
        const PreviewArchiveContext m_context;
        std::array<uint8_t, sourceRequestPrefixBytes> m_prefix {};
        size_t m_prefixUsed = 0, m_archiveUsed = 0;
        std::unique_ptr<uint8_t[]> m_archive;
        detail::PreviewStreamStatus m_status;
        PreviewChunkReader m_reader;
    };
}
}
#endif
