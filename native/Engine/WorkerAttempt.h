// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_ATTEMPT_H_
#define DUST3D_NATIVE_WORKER_ATTEMPT_H_
#include "WorkerAdmission.h"
#include <optional>
#include <vector>

namespace dust3d {
namespace worker {
    struct CollectedJob;
    /// Diagnostic-only partial collector state; never an admissible worker completion.
    struct WorkerAttemptReport {
        bool captured = false;
        /// Collector-observed terminal outcome only; exceptional cleanup has its separate report.
        std::optional<ProcessOutcome> outcome;
        std::vector<uint8_t> reply;
        std::vector<uint8_t> diagnostic;
        bool diagnosticTruncated = false;
    };
    /// Private RAII boundary retaining bounded collector buffers during exception unwinding.
    /// It moves bytes without allocation, wrapping/replacing the active exception, parsing or logging.
    class WorkerAttemptCapture {
    public:
        /// Reset caller-owned output; borrowed inputs/output must outlive this single-thread scope.
        WorkerAttemptCapture(CollectedJob& result, bool& observedTerminalOutcome, WorkerAttemptReport* report) noexcept;
        ~WorkerAttemptCapture() noexcept;
        WorkerAttemptCapture(const WorkerAttemptCapture&) = delete;
        WorkerAttemptCapture& operator=(const WorkerAttemptCapture&) = delete;
        /// Already-owned retirement exceptions retain their own context and need no partial capture.
        void dismiss() noexcept { m_isEnabled = false; }

    private:
        CollectedJob& m_result;
        const bool& m_observedTerminalOutcome;
        WorkerAttemptReport* m_report;
        int m_exceptionCount;
        bool m_isEnabled = true;
    };
}
}
#endif
