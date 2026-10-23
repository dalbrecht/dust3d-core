// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_QUEUE_H_
#define DUST3D_NATIVE_WORKER_QUEUE_H_
#include "WorkerAdmission.h"
#include <optional>

namespace dust3d {
namespace worker {
    enum class QueueErrorCode { closed,
        invalidRequest,
        reusedRequest,
        unexpectedCompletion };
    class QueueError : public std::runtime_error {
    public:
        QueueError(QueueErrorCode code, const char* message);
        QueueErrorCode code() const noexcept { return m_code; }

    private:
        QueueErrorCode m_code;
    };
    /// Single-owner, externally serialized scheduling state; never starts or stops a process.
    /// Retains one waiting archive and active header only. Caller owns/drains/reaps active transport.
    /// Submit, cancel, close and complete must run on the same serialized owner, away from blocking I/O.
    class RequestQueue {
    public:
        explicit RequestQueue(Limits limits = {});
        RequestQueue(const RequestQueue&) = delete;
        RequestQueue& operator=(const RequestQueue&) = delete;
        /// Validate before replacing waiting work; globally increasing nonzero request IDs cannot replay.
        /// Returns the replaced waiting identity. Throws QueueError or original ProtocolError.
        std::optional<RequestIdentity> submit(Frame request);
        /// Transfer the waiting archive only when no transport is active; otherwise returns nullopt.
        std::optional<Frame> begin();
        /// Retire matching publication and discard matching waiting data; active transport still drains.
        void cancel(const RequestIdentity& identity) noexcept;
        /// Permanently reject new requests and retire publication; caller must still collect active I/O.
        void close() noexcept;
        /// Release matching work after transport has cleaned up and reaped on error; publishes no data.
        /// The caller must retain/report the original transport cause. Throws QueueError on mismatch.
        void abandon(const RequestIdentity& collected);
        /// Release the matching active slot before admission; reject stale/cancelled/closed/replayed data.
        /// Foreign completions do not release active work. Typed admission/codec causes propagate.
        AdmittedReply complete(const RequestIdentity& collected, ProcessOutcome outcome,
            const uint8_t* bytes, size_t size, const MeshPayloadLimits& meshLimits = {},
            const FailurePayloadLimits& failureLimits = {});

    private:
        Limits m_limits;
        uint64_t m_lastRequest = 0;
        bool m_isClosed = false;
        std::optional<RequestIdentity> m_current;
        std::optional<Header> m_active;
        std::optional<Frame> m_waiting;
    };
}
}
#endif
