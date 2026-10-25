// SPDX-License-Identifier: MIT
#include "WorkerAttempt.h"
#include "WorkerTransport.h"
#include <exception>
#include <utility>

namespace dust3d {
namespace worker {
    WorkerAttemptCapture::WorkerAttemptCapture(CollectedJob& result, bool& observedTerminalOutcome, WorkerAttemptReport* report) noexcept
        : m_result(result)
        , m_observedTerminalOutcome(observedTerminalOutcome)
        , m_report(report)
        , m_exceptionCount(std::uncaught_exceptions())
    {
        if (m_report)
            *m_report = {};
    }
    WorkerAttemptCapture::~WorkerAttemptCapture() noexcept
    {
        if (!m_isEnabled || !m_report || std::uncaught_exceptions() <= m_exceptionCount)
            return;
        m_report->captured = true;
        if (m_observedTerminalOutcome)
            m_report->outcome = m_result.outcome;
        m_report->reply = std::move(m_result.reply);
        m_report->diagnostic = std::move(m_result.diagnostic);
        m_report->diagnosticTruncated = m_result.diagnosticTruncated;
    }
}
}
