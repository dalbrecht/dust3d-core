// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_QUEUE_H_
#define DUST3D_NATIVE_WORKER_QUEUE_H_
#include "WorkerAdmission.h"
#include <optional>
#include <utility>

namespace dust3d {
namespace worker {
    enum class QueueErrorCode { closed,
        invalidRequest,
        reusedRequest,
        unexpectedCompletion,
        invalidServiceSession,
        wrongCompletionMode };
    class QueueError : public std::runtime_error {
    public:
        QueueError(QueueErrorCode code, const char* message);
        QueueErrorCode code() const noexcept { return m_code; }

    private:
        QueueErrorCode m_code;
    };
    /// Single-owner, externally serialized scheduling state; never starts or stops a process.
    /// Retains one waiting archive and active header only. Caller drains transport and reaps owned children.
    /// Submit, cancel, close and complete must run on the same serialized owner, away from blocking I/O.
    class RequestQueue {
    public:
        /// Validate positive supported wire quotas before accepting work; throws ProtocolError.
        explicit RequestQueue(Limits limits = {});
        RequestQueue(const RequestQueue&) = delete;
        RequestQueue& operator=(const RequestQueue&) = delete;
        /// Validate before replacing waiting work; globally increasing nonzero request IDs cannot replay.
        /// Returns the replaced waiting identity. Throws QueueError or original ProtocolError.
        std::optional<RequestIdentity> submit(Frame request);
        /// Transfer waiting child-process work only when no transport is active; otherwise nullopt.
        std::optional<Frame> begin();
        /// Begin persistent-service work, freezing its connection lifetime before submission.
        /// @param connectionGeneration Nonzero caller-trusted lifetime, not receipt-derived.
        /// @return Owned waiting frame, or nullopt while active/empty; no process or I/O starts.
        /// @throws QueueError(invalidServiceSession) without consuming waiting data for zero generation.
        /// Example: auto next = queue.beginService(connectionGeneration);
        std::optional<Frame> beginService(uint64_t connectionGeneration);
        /// Retire matching publication and discard matching waiting data; active transport still drains.
        void cancel(const RequestIdentity& identity) noexcept;
        /// Permanently reject new requests and retire publication; caller must still collect active I/O.
        void close() noexcept;
        /// Boundary adapters can reject retired submissions before copying input.
        bool isClosed() const noexcept { return m_isClosed; }
        /// Release matching work after transport cleanup (and child reaping where applicable); no publication.
        /// The caller must retain/report the original transport cause. Throws QueueError on mismatch.
        void abandon(const RequestIdentity& collected);
        /// Release the matching active slot before admission; reject stale/cancelled/closed/replayed data.
        /// UV/source/indexed responses use matching independent policies, separately from base mesh limits.
        /// Foreign or wrong-mode completions do not release active work. Typed admission/codec causes propagate.
        AdmittedReply complete(const RequestIdentity& collected, ProcessOutcome outcome,
            const uint8_t* bytes, size_t size, const MeshPayloadLimits& meshLimits = {},
            const FailurePayloadLimits& failureLimits = {}, const UvMeshPayloadLimits& uvLimits = {}, const SourcedMeshPayloadLimits& sourceLimits = {}, const IndexedReplyLimits& indexedLimits = {}, const ImportedRenderLimits& importedLimits = {}, const LegacyBakedLimits& legacyLimits = {}, const SourceBakedLimits& sourceBakeLimits = {});

        /// Complete matching persistent-service work after transport collection/cleanup.
        /// @param collected Captured active identity supplied by the transport owner.
        /// @param observed Real authenticated-connection lifetime/OS-peer/EOF observations.
        /// @param receipt Scalar service callback; not trusted observation or current source.
        /// @param bytes Exact collected frame borrowed during this call.
        /// @param size Actual frame byte count.
        /// @param meshLimits Base semantic/output policy.
        /// @param failureLimits Original failure payload policy.
        /// @param uvLimits Atomic paired geometry/UV policy.
        /// @param sourceLimits Atomic paired geometry/source policy.
        /// @param indexedLimits Ordinary/indexed-source semantic policies.
        /// @return Owned semantics only for authoritative current work, once.
        /// @throws QueueError for foreign/mode mismatch (active retained), or original admission/codec errors
        /// after releasing matching work. Never fabricates child exit or cancels native execution.
        /// Example: queue.completeService(id, observed, receipt, bytes.data(), bytes.size()).
        AdmittedReply completeService(const RequestIdentity& collected, const ServiceObservation& observed,
            const ServiceReceipt& receipt, const uint8_t* bytes, size_t size,
            const MeshPayloadLimits& meshLimits = {}, const FailurePayloadLimits& failureLimits = {},
            const UvMeshPayloadLimits& uvLimits = {}, const SourcedMeshPayloadLimits& sourceLimits = {}, const IndexedReplyLimits& indexedLimits = {}, const ImportedRenderLimits& importedLimits = {}, const LegacyBakedLimits& legacyLimits = {}, const SourceBakedLimits& sourceBakeLimits = {});

        /// Retire matching collected service work, then admit its session envelope.
        /// @param collected Captured active identity from this queue's transport slot.
        /// @param observed Independent authenticated transport/peer/EOF observations.
        /// @param receipt Callback control values, not peer/current identity evidence.
        /// @param bytes Complete stable borrowed framed session output.
        /// @param size Actual readable count.
        /// @param limits Independent session semantic/aggregate policies.
        /// @return Owned outcome/diagnostics/observations for authoritative current work.
        /// @throws QueueError for foreign/mode mismatch without releasing active work;
        /// matching collection releases before original admission/codec errors propagate.
        /// No native work, process control or authentication occurs here.
        /// Example: auto value = queue.completeSessionService(id, observed, receipt, bytes, size);
        SessionOutput completeSessionService(const RequestIdentity& collected, const ServiceObservation& observed,
            const ServiceReceipt& receipt, const uint8_t* bytes, size_t size, const SessionOutputLimits& limits = {});

    private:
        std::optional<Frame> beginWork(uint64_t connectionGeneration);
        std::pair<Header, uint64_t> collectService(const RequestIdentity& collected);
        Limits m_limits;
        // Reserve zero so service work cannot pass the child-exit completion path.
        uint64_t m_activeServiceGeneration = 0;
        uint64_t m_lastRequest = 0;
        bool m_isClosed = false;
        std::optional<RequestIdentity> m_current;
        std::optional<Header> m_active;
        std::optional<Frame> m_waiting;
    };
}
}
#endif
