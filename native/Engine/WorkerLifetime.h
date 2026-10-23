// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_LIFETIME_H_
#define DUST3D_NATIVE_WORKER_LIFETIME_H_
#include <cstdint>
#include <optional>
#include <stdexcept>

namespace dust3d {
namespace worker {
    struct LifetimePolicy {
        uint64_t workMilliseconds;
        uint64_t stopGraceMilliseconds;
        uint64_t drainMilliseconds;
    };
    struct LifetimeObservation {
        uint64_t elapsedMilliseconds;
        bool cancellationRequested;
        bool hasExited;
        bool channelsClosed;
    };
    enum class LifetimeAction { none,
        requestStop,
        requestForceStop,
        closeChannels };
    enum class LifetimeReason { none,
        cancelled,
        deadline,
        drainTimeout };
    struct LifetimeDecision {
        LifetimeAction action = LifetimeAction::none;
        LifetimeReason reason = LifetimeReason::none;
        bool mayAdmit = false;
        bool finished = false;
    };
    enum class LifetimeErrorCode { invalidPolicy,
        timeReversed,
        stateReversed };
    class LifetimeError : public std::runtime_error {
    public:
        LifetimeError(LifetimeErrorCode code, const char* message);
        LifetimeErrorCode code() const noexcept { return m_code; }

    private:
        LifetimeErrorCode m_code;
    };
    /// Pure single-owner policy, externally serialized. No clock, process, signal or channel operations.
    /// Budgets are explicit: work 1..600000ms, stop grace/drain each 1..10000ms.
    /// OS termination/reaping and semantic PendingReply admission remain the caller's responsibility.
    class WorkerLifetime {
    public:
        explicit WorkerLifetime(LifetimePolicy policy);
        WorkerLifetime(const WorkerLifetime&) = delete;
        WorkerLifetime& operator=(const WorkerLifetime&) = delete;
        /// Observe elapsed monotonic time and sticky exit/channel state. Invalid observations are atomic.
        /// Stop/escalation/close actions and successful admission permission are each emitted once.
        /// Cancellation wins a simultaneous deadline; the first retirement reason is retained.
        /// mayAdmit permits only subsequent exit-code/frame/revision validation, never publication alone.
        /// Throws LifetimeError for invalid policy, reversed elapsed time or reversed terminal state.
        LifetimeDecision observe(LifetimeObservation observation);

    private:
        LifetimePolicy m_policy;
        uint64_t m_elapsed = 0;
        std::optional<uint64_t> m_exitAt;
        std::optional<uint64_t> m_stopAt;
        LifetimeReason m_reason = LifetimeReason::none;
        bool m_channelsClosed = false;
        bool m_forceRequested = false;
        bool m_finished = false;
    };
}
}
#endif
