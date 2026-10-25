// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_REPLY_COLLECTOR_H_
#define DUST3D_NATIVE_WORKER_REPLY_COLLECTOR_H_
#include "WorkerProtocol.h"
namespace dust3d {
namespace worker {
    /// Incremental framing only, not process/transaction/semantic/publication admission.
    /// Caller serializes access and retains the output vector object without modifying it
    /// while this collector is in use. Partial bytes remain diagnostic-only on rejection.
    class WorkerReplyCollector {
    public:
        /// Validate expected request and wire policy before reserving the fixed header.
        /// @param request Stable expected document/request/revision/operation, copied for this stream.
        /// @param limits Supported wire budgets; logical bytes exclude capacity/allocator overhead/RSS.
        /// @param output Empty caller-owned vector, borrowed until collector is no longer used.
        /// @throws ProtocolError or allocation errors. No body allocation occurs here.
        WorkerReplyCollector(const Header& request, const Limits& limits, std::vector<uint8_t>& output);
        WorkerReplyCollector(const WorkerReplyCollector&) = delete;
        WorkerReplyCollector& operator=(const WorkerReplyCollector&) = delete;
        /// Maximum next read (1..65536), stopping at the header before body collection.
        /// After a validated header, one extra byte detects overlong/trailing data.
        /// @param maximumChunk Positive supported caller I/O chunk cap.
        /// @return Bounded read size, never an instruction to allocate the advertised whole body.
        /// @throws ProtocolError for unsupported cap, rejected state or observed EOF.
        size_t nextReadBytes(size_t maximumChunk) const;
        /// Copy a stable independent borrowed span; validate exact header identity/quota first.
        /// @param bytes Complete readable span for this call; NULL permitted only for zero size.
        /// @param size Exact count. Overlong body spans reject before body insertion.
        /// @throws Original ProtocolError/allocation errors; rejection becomes sticky.
        void append(const uint8_t* bytes, size_t size);
        /// Observe actual EOF. Requires one complete frame; repeated successful EOF is idempotent.
        /// @throws ProtocolError for truncation/rejected state. This never authorizes publication.
        /// Example: collector.finish(); // only after channel read returns EOF
        void finish();

    private:
        Header m_request;
        Limits m_limits;
        std::vector<uint8_t>& m_output;
        size_t m_received = 0;
        size_t m_expectedBytes = headerBytes;
        bool m_hasHeader = false;
        bool m_failed = false;
        bool m_finished = false;
        void checkState() const;
    };
}
}
#endif
