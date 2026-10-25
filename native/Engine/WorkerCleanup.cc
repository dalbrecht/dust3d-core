// SPDX-License-Identifier: MIT
#include "WorkerCleanup.h"

namespace dust3d {
namespace worker {
    WorkerCleanupReport cleanupWorkerChild(bool hasOwnedChild, bool shouldStop, WorkerCleanupSystem& system) noexcept
    {
        WorkerCleanupReport report;
        if (!hasOwnedChild)
            return report;
        if (shouldStop) {
            report.attemptedStop = true;
            const int cause = system.requestForceStop();
            if (cause != 0)
                report.stopError = cause;
        }
        report.attemptedReap = true;
        for (;;) {
            const auto observed = system.reap();
            switch (observed.state) {
            case CleanupWaitState::interrupted:
                continue;
            case CleanupWaitState::collected:
                report.outcome = observed.outcome;
                return report;
            case CleanupWaitState::failed:
                report.waitFailure = CleanupWaitFailure { CleanupFailureCode::operatingSystem, observed.operatingSystemCode };
                return report;
            default:
                report.waitFailure = CleanupWaitFailure { CleanupFailureCode::invalidWaitResult, observed.operatingSystemCode };
                return report;
            }
        }
    }
}
}
