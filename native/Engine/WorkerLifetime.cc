// SPDX-License-Identifier: MIT
#include "WorkerLifetime.h"

namespace dust3d {
namespace worker {
    LifetimeError::LifetimeError(LifetimeErrorCode code, const char* message)
        : std::runtime_error(message)
        , m_code(code)
    {
    }
    WorkerLifetime::WorkerLifetime(LifetimePolicy policy)
        : m_policy(policy)
    {
        if (!policy.workMilliseconds || policy.workMilliseconds > 600000
            || !policy.stopGraceMilliseconds || policy.stopGraceMilliseconds > 10000
            || !policy.drainMilliseconds || policy.drainMilliseconds > 10000)
            throw LifetimeError(LifetimeErrorCode::invalidPolicy, "Worker lifetime budgets are outside supported bounds");
    }
    LifetimeDecision WorkerLifetime::observe(LifetimeObservation observation)
    {
        if (observation.elapsedMilliseconds < m_elapsed)
            throw LifetimeError(LifetimeErrorCode::timeReversed, "Worker lifetime elapsed time moved backwards");
        if ((m_exitAt && !observation.hasExited) || (m_channelsClosed && !observation.channelsClosed))
            throw LifetimeError(LifetimeErrorCode::stateReversed, "Worker lifetime terminal observation moved backwards");
        m_elapsed = observation.elapsedMilliseconds;
        if (m_finished)
            return { LifetimeAction::none, m_reason, false, true };
        if (m_reason == LifetimeReason::none) {
            if (observation.cancellationRequested)
                m_reason = LifetimeReason::cancelled;
            else if (!m_exitAt && m_elapsed >= m_policy.workMilliseconds)
                m_reason = LifetimeReason::deadline;
        }
        if (!m_exitAt && observation.hasExited)
            m_exitAt = m_elapsed;
        m_channelsClosed = observation.channelsClosed;
        if (m_exitAt) {
            if (m_channelsClosed) {
                m_finished = true;
                return { LifetimeAction::none, m_reason, m_reason == LifetimeReason::none, true };
            }
            // Subtract monotonic observations rather than adding deadlines that could overflow.
            if (m_elapsed - *m_exitAt >= m_policy.drainMilliseconds) {
                if (m_reason == LifetimeReason::none)
                    m_reason = LifetimeReason::drainTimeout;
                m_finished = true;
                return { LifetimeAction::closeChannels, m_reason, false, true };
            }
        } else if (m_reason != LifetimeReason::none) {
            if (!m_stopAt) {
                m_stopAt = m_elapsed;
                return { LifetimeAction::requestStop, m_reason, false, false };
            }
            if (!m_forceRequested && m_elapsed - *m_stopAt >= m_policy.stopGraceMilliseconds) {
                m_forceRequested = true;
                return { LifetimeAction::requestForceStop, m_reason, false, false };
            }
        }
        return { LifetimeAction::none, m_reason, false, false };
    }
}
}
