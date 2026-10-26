// SPDX-License-Identifier: MIT
#include "WorkerQueue.h"
#include <utility>

namespace dust3d {
namespace worker {
    QueueError::QueueError(QueueErrorCode code, const char* message)
        : std::runtime_error(message)
        , m_code(code)
    {
    }
    RequestQueue::RequestQueue(Limits limits)
        : m_limits(limits)
    {
        validateWireLimits(limits);
    }
    std::optional<RequestIdentity> RequestQueue::submit(Frame request)
    {
        if (m_isClosed)
            throw QueueError(QueueErrorCode::closed, "Worker queue is closed");
        encodeHeader(request.header, m_limits);
        if (request.header.kind != FrameKind::request || request.header.payloadBytes != request.payload.size())
            throw QueueError(QueueErrorCode::invalidRequest, "Worker queue requires an exact request payload");
        if (request.header.identity.request <= m_lastRequest)
            throw QueueError(QueueErrorCode::reusedRequest, "Worker request IDs must increase across queue lifetime");
        std::optional<RequestIdentity> replaced;
        if (m_waiting)
            replaced = m_waiting->header.identity;
        m_lastRequest = request.header.identity.request;
        m_current = request.header.identity;
        m_waiting = std::move(request);
        return replaced;
    }
    std::optional<Frame> RequestQueue::begin()
    {
        return beginWork(0);
    }
    std::optional<Frame> RequestQueue::beginService(uint64_t connectionGeneration)
    {
        if (!connectionGeneration)
            throw QueueError(QueueErrorCode::invalidServiceSession, "Service queue requires a nonzero connection generation");
        return beginWork(connectionGeneration);
    }
    std::optional<Frame> RequestQueue::beginWork(uint64_t connectionGeneration)
    {
        if (m_active || !m_waiting)
            return std::nullopt;
        auto request = std::move(m_waiting);
        m_waiting.reset();
        m_active = request->header;
        m_activeServiceGeneration = connectionGeneration;
        return request;
    }
    void RequestQueue::cancel(const RequestIdentity& identity) noexcept
    {
        if (m_current && *m_current == identity)
            m_current.reset();
        if (m_waiting && m_waiting->header.identity == identity)
            m_waiting.reset();
    }
    void RequestQueue::close() noexcept
    {
        m_isClosed = true;
        m_current.reset();
        m_waiting.reset();
    }
    void RequestQueue::abandon(const RequestIdentity& collected)
    {
        if (!m_active || !(m_active->identity == collected))
            throw QueueError(QueueErrorCode::unexpectedCompletion, "Worker failure has no matching active request");
        m_active.reset();
        m_activeServiceGeneration = 0;
        if (m_current && *m_current == collected)
            m_current.reset();
    }
    AdmittedReply RequestQueue::complete(const RequestIdentity& collected, ProcessOutcome outcome,
        const uint8_t* bytes, size_t size, const MeshPayloadLimits& meshLimits,
        const FailurePayloadLimits& failureLimits, const UvMeshPayloadLimits& uvLimits, const SourcedMeshPayloadLimits& sourceLimits)
    {
        if (!m_active || !(m_active->identity == collected))
            throw QueueError(QueueErrorCode::unexpectedCompletion, "Worker completion has no matching active request");
        if (m_activeServiceGeneration)
            throw QueueError(QueueErrorCode::wrongCompletionMode, "Service work requires service completion admission");
        const auto request = *m_active;
        m_active.reset();
        PendingReply pending(request, m_limits);
        if (!m_current)
            pending.retire();
        return pending.complete(m_current.value_or(request.identity), outcome, bytes, size,
            m_limits, meshLimits, failureLimits, uvLimits, sourceLimits);
    }
    AdmittedReply RequestQueue::completeService(const RequestIdentity& collected, const ServiceObservation& observed,
        const ServiceReceipt& receipt, const uint8_t* bytes, size_t size,
        const MeshPayloadLimits& meshLimits, const FailurePayloadLimits& failureLimits, const UvMeshPayloadLimits& uvLimits, const SourcedMeshPayloadLimits& sourceLimits)
    {
        if (!m_active || !(m_active->identity == collected))
            throw QueueError(QueueErrorCode::unexpectedCompletion, "Service completion has no matching active request");
        if (!m_activeServiceGeneration)
            throw QueueError(QueueErrorCode::wrongCompletionMode, "Child work requires observed process-exit completion admission");
        const auto request = *m_active;
        const auto generation = m_activeServiceGeneration;
        m_active.reset();
        m_activeServiceGeneration = 0;
        PendingServiceReply pending(request, generation, m_limits);
        if (!m_current)
            pending.retire();
        return pending.complete(m_current.value_or(request.identity), observed, receipt, bytes, size,
            m_limits, meshLimits, failureLimits, uvLimits, sourceLimits);
    }

}
}
