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
        if (m_active || !m_waiting)
            return std::nullopt;
        auto request = std::move(m_waiting);
        m_waiting.reset();
        m_active = request->header;
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
        if (m_current && *m_current == collected)
            m_current.reset();
    }
    AdmittedReply RequestQueue::complete(const RequestIdentity& collected, ProcessOutcome outcome,
        const uint8_t* bytes, size_t size, const MeshPayloadLimits& meshLimits,
        const FailurePayloadLimits& failureLimits)
    {
        if (!m_active || !(m_active->identity == collected))
            throw QueueError(QueueErrorCode::unexpectedCompletion, "Worker completion has no matching active request");
        const auto request = *m_active;
        m_active.reset();
        PendingReply pending(request);
        if (!m_current)
            pending.retire();
        return pending.complete(m_current.value_or(request.identity), outcome, bytes, size,
            m_limits, meshLimits, failureLimits);
    }
}
}
