// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_CLEANUP_H_
#define DUST3D_NATIVE_WORKER_CLEANUP_H_
#include "WorkerAdmission.h"
#include <optional>

namespace dust3d {
namespace worker {
    enum class CleanupWaitState { collected,
        interrupted,
        failed };
    struct CleanupWaitResult {
        CleanupWaitState state = CleanupWaitState::failed;
        ProcessOutcome outcome;
        int operatingSystemCode = 0;
    };
    enum class CleanupFailureCode { operatingSystem,
        invalidWaitResult };
    struct CleanupWaitFailure {
        CleanupFailureCode code;
        int operatingSystemCode;
    };
    /// Allocation-free cleanup observations; absent outcome never implies a collected child.
    struct WorkerCleanupReport {
        bool attemptedStop = false;
        bool attemptedReap = false;
        std::optional<int> stopError;
        std::optional<CleanupWaitFailure> waitFailure;
        std::optional<ProcessOutcome> outcome;
    };
    /// Private owned-child boundary. Recording implementations perform no process operations.
    class WorkerCleanupSystem {
    public:
        virtual ~WorkerCleanupSystem() = default;
        /// Return zero or the original OS error; implementations normalize an already-absent stop.
        virtual int requestForceStop() noexcept = 0;
        /// Observe one blocking reap attempt; interrupted observations are retried by the policy.
        virtual CleanupWaitResult reap() noexcept = 0;
    };
    /// Best-effort cleanup without replacing an active primary exception.
    /// @param hasOwnedChild False prevents all process operations.
    /// @param shouldStop True requests force-stop once before reaping, even if stop fails.
    /// @param system Exclusively owned process adapter; must not throw or allocate on failure.
    /// @return Independent stop/wait causes and actual outcome, if observed. No allocation/logging.
    /// Reap retries interrupted observations only; OS blocking time is not bounded by this function.
    WorkerCleanupReport cleanupWorkerChild(bool hasOwnedChild, bool shouldStop, WorkerCleanupSystem& system) noexcept;
}
}
#endif
