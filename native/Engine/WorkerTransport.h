// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_TRANSPORT_H_
#define DUST3D_NATIVE_WORKER_TRANSPORT_H_
#include "WorkerAdmission.h"
#include "WorkerAttempt.h"
#include "WorkerCleanup.h"
#include "WorkerLifetime.h"
#include "WorkerResourceObservation.h"
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
        /// Optional caller-owned output, reset before preflight. Retain until return/throw;
        /// read afterward on the calling thread only. Never share it between active operations.
        /// Reports exceptional destructor cleanup without replacing the original exception.
        WorkerCleanupReport* cleanupReport = nullptr;
        /// Optional caller-owned partial collector diagnostics, reset before preflight.
        /// Keep alive until return/throw and read afterward only; never publish its partial bytes.
        /// Does not replace the primary exception or the separate exceptional cleanup report.
        WorkerAttemptReport* attemptReport = nullptr;
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
    struct ResourceCollectedJob {
        CollectedJob collected;
        ResourceObservation observation;
    };
    enum class ResourceJobErrorCode { unsuccessfulExit,
        invalidObservation };
    /// Independently owns original outcome/reply/opaque diagnostics and the typed
    /// metadata cause. Consumers must surface original diagnostic bytes safely;
    /// they are never discarded, decoded as text or automatically logged here.
    class ResourceJobError : public std::runtime_error {
    public:
        ResourceJobError(CollectedJob collected, std::optional<ResourceObservationError> observationFailure = std::nullopt);
        ResourceJobErrorCode code() const noexcept { return m_observationFailure ? ResourceJobErrorCode::invalidObservation : ResourceJobErrorCode::unsuccessfulExit; }
        const CollectedJob& collected() const noexcept { return m_collected; }
        const std::optional<ResourceObservationError>& observationFailure() const noexcept { return m_observationFailure; }

    private:
        CollectedJob m_collected;
        std::optional<ResourceObservationError> m_observationFailure;
    };
    /// Pure resource admission of an actually collected completion against its
    /// prevalidated launch. Require normal exit zero before inspecting metadata.
    /// Preserves full original diagnostics, including opaque tail/truncation.
    /// Does not validate/publish the frame or revision; PendingReply remains required.
    /// Throws ResourceJobError with owned context; original invalid requested-profile
    /// errors propagate. Synthetic unit outcomes prove decisions, not OS events.
    ResourceCollectedJob admitResourceCompletion(CollectedJob collected, const ResourceLaunch& requested);
    /// Opt-in supervised resource launch. Profile arguments are encoded before
    /// spawning; unsupported virtual caps and capture below 1024 bytes reject first.
    /// Retired work still throws WorkerStopped. Require resource metadata plus
    /// separate PendingReply/queue admission before any publication. Never calls
    /// setrlimit in the parent and never retries/falls back to an unrestricted launch.
    /// Caller runs off the UI thread, keeps configuration immutable during the call
    /// and exclusively owns child reaping. Trusted binary identity, production budgets,
    /// RSS/filesystem/descendant/parent-death containment and Apple adoption are absent.
    ResourceCollectedJob runResourceLimitedWorkerJob(const std::string& executable, const Frame& request,
        const ResourceLaunch& launch, const TransportLimits& limits = {}, const WorkerSupervision& supervision = {});
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
