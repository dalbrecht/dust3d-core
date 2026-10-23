// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_TRANSPORT_H_
#define DUST3D_NATIVE_WORKER_TRANSPORT_H_
#include "WorkerAdmission.h"
#include "WorkerLifetime.h"
#include <atomic>

namespace dust3d {
namespace worker {
    struct TransportLimits {
        Limits wire;
        size_t maximumIoChunkBytes = 65536;
        size_t maximumCapturedDiagnosticBytes = 4096;
    };
    struct CollectedJob {
        ProcessOutcome outcome;
        std::vector<uint8_t> reply;
        std::vector<uint8_t> diagnostic;
        bool diagnosticTruncated = false;
    };
    enum class TransportErrorCode { invalidConfiguration,
        setup,
        spawn,
        input,
        output,
        diagnostic,
        wait,
        stop,
        clock };
    /// OS errors retain the original errno/posix_spawn code; no document/child diagnostic content is logged.
    class TransportError : public std::runtime_error {
    public:
        TransportError(TransportErrorCode code, int operatingSystemCode, const char* operation);
        TransportErrorCode code() const noexcept { return m_code; }
        int operatingSystemCode() const noexcept { return m_operatingSystemCode; }

    private:
        TransportErrorCode m_code;
        int m_operatingSystemCode;
    };
    /// Cross-thread cancellation flag; caller retains it until supervised execution returns.
    class WorkerCancellation {
    public:
        void requestCancellation() noexcept { m_requested.store(true, std::memory_order_release); }
        bool isCancellationRequested() const noexcept { return m_requested.load(std::memory_order_acquire); }

    private:
        std::atomic<bool> m_requested { false };
    };
    struct WorkerSupervision {
        LifetimePolicy lifetime { 600000, 1000, 1000 };
        const WorkerCancellation* cancellation = nullptr;
    };
    /// A retired job cannot be admitted. Collected outcome/opaque diagnostics are retained after spawn.
    /// An empty collected value means cancellation was observed before any child was started.
    class WorkerStopped : public std::runtime_error {
    public:
        WorkerStopped(LifetimeReason reason, std::optional<CollectedJob> collected);
        LifetimeReason reason() const noexcept { return m_reason; }
        const std::optional<CollectedJob>& collected() const noexcept { return m_collected; }

    private:
        LifetimeReason m_reason;
        std::optional<CollectedJob> m_collected;
    };
    /// Opt-in supervised Unix operation. Uses monotonic lifetime decisions and the actual child outcome.
    /// On retirement, stop input, request termination/escalation and collect exit before throwing WorkerStopped.
    /// Post-exit channel drain is bounded. Exception cleanup closes channels and requests force-stop before reap.
    /// Caller exclusively owns child reaping: no external waitpid/SIGCHLD reaper may collect this helper.
    /// Must run off the UI thread. OS spawn/reap in uninterruptible states is not promised bounded.
    /// Does not impose CPU/RSS/filesystem limits, sandbox or parent-death containment; no production adoption.
    /// Throws original transport/codec/lifetime errors or WorkerStopped; never automatically retries.
    CollectedJob runSupervisedWorkerJob(const std::string& executable, const Frame& request,
        const TransportLimits& limits = {}, const WorkerSupervision& supervision = {});
    /// Development-only synchronous Unix transport. Pass a trusted absolute helper executable path.
    /// Validate/encode request, spawn directly without shell, concurrently send/drain three channels,
    /// collect bounded reply/diagnostic bytes and observe termination. No data is admitted/published here.
    /// Feed the result to PendingReply with authoritative current identity after this returns.
    /// Caller must run off the UI thread and serialize application state/admission separately.
    /// No deadline, hard cancellation, RSS/filesystem sandbox or parent-death policy is provided.
    /// Cleanup closes channels and reaps the child, and can block on uncooperative generation.
    /// Throws typed transport/codec errors or allocation errors; never automatically retries.
    CollectedJob runWorkerJob(const std::string& executable, const Frame& request, const TransportLimits& limits = {});
}
}
#endif
