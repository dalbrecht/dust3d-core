// SPDX-License-Identifier: MIT
import Foundation

protocol WorkerServiceExecutor: Sendable {
    // Enqueue in FIFO order without waiting for execution or calling inline.
    func enqueue(_ operation: @escaping @Sendable () -> Void)
}
final class DispatchWorkerServiceExecutor: WorkerServiceExecutor {
    private let queue = DispatchQueue(label: "org.dust3d.core.static-worker.serial")
    func enqueue(_ operation: @escaping @Sendable () -> Void) { queue.async(execute: operation) }
}
final class WorkerServiceScheduler: @unchecked Sendable {
    // All state transitions and enqueue ordering share this lock. Execution never waits inside it.
    private let lock = NSLock()
    private var state = WorkerServiceAdmissionState()
    private var lastAcceptedRequestIdentity: UInt64 = 0
    private var activeRequestIdentity: UInt64?
    private var startedLease: WorkerServiceLease?
    private var cancelledLease: WorkerServiceLease?
    private var ownership = WorkerServiceOwnershipState()
    private let executor: any WorkerServiceExecutor
    private struct TimedJob {
        let owner: WorkerServiceConnection
        let request: UInt64
        let lease: WorkerServiceLease
        let deadline: WorkerServiceJobDeadline
        var previous: UInt64
    }
    private let jobBudget: WorkerServiceJobBudget
    private let timing: WorkerServiceJobTiming
    private let onDeadline: @Sendable (WorkerServiceJobFailure) -> Void
    private var timedJob: TimedJob?
    private var alarm: (any WorkerServiceJobTimerCancellation)?
    private var terminalFailure: WorkerServiceJobFailure?
    private let onRetired: @Sendable (Result<Void, any Error>) -> Void
    init(
        executor: any WorkerServiceExecutor = DispatchWorkerServiceExecutor(),
        jobBudget: WorkerServiceJobBudget, timing: WorkerServiceJobTiming = .system(),
        onDeadline: @escaping @Sendable (WorkerServiceJobFailure) -> Void,
        onRetired: @escaping @Sendable (Result<Void, any Error>) -> Void
    ) {
        self.jobBudget = jobBudget
        self.timing = timing
        self.onDeadline = onDeadline
        self.executor = executor
        self.onRetired = onRetired
    }
    deinit { alarm?.cancel() }
    // Caller holds the scheduler lock; terminal callbacks run only after it is released.
    private func sealLocked(
        owner: WorkerServiceConnection, request: UInt64, lease: WorkerServiceLease,
        cause: WorkerServiceJobDeadlineError
    ) -> WorkerServiceJobFailure {
        if let terminalFailure { return terminalFailure }
        let failure = WorkerServiceJobFailure(
            ownerGeneration: owner.generation, requestIdentity: request, leaseGeneration: lease.generation,
            budgetMilliseconds: jobBudget.milliseconds, cause: cause)
        terminalFailure = failure
        ownership = ownership.retiring(owner).state
        state = state.retired()
        alarm?.cancel()
        alarm = nil
        timedJob = nil
        return failure
    }
    private func checkLocked(owner: WorkerServiceConnection, lease: WorkerServiceLease)
        -> WorkerServiceJobFailure?
    {
        guard var job = timedJob, job.owner == owner, job.lease == lease, terminalFailure == nil else { return nil }
        let observed = timing.clock()
        guard observed >= job.previous else {
            return sealLocked(
                owner: owner, request: job.request, lease: lease,
                cause: .clockReversed(previous: job.previous, observed: observed))
        }
        do {
            _ = try job.deadline.remainingMilliseconds(at: observed)
            job.previous = observed
            timedJob = job
            return nil
        } catch {
            return sealLocked(owner: owner, request: job.request, lease: lease, cause: error)
        }
    }
    private func armLocked(_ job: TimedJob, milliseconds: UInt64) {
        alarm?.cancel()
        alarm = timing.timer(
            milliseconds,
            WorkerServiceJobTimeout(fire: { [weak self] in
                self?.deadlineWake(owner: job.owner, lease: job.lease)
            }))
    }
    private func deadlineWake(owner: WorkerServiceConnection, lease: WorkerServiceLease) {
        let failure: WorkerServiceJobFailure? = lock.withLock {
            // Old callbacks never read the clock or touch a newer job's timer.
            guard terminalFailure == nil, let job = timedJob, job.owner == owner, job.lease == lease else { return nil }
            if let failure = checkLocked(owner: owner, lease: lease) { return failure }
            guard let current = timedJob else { return nil }
            // checkLocked has just validated this exact observation against the original end.
            let remaining = (current.deadline.end - current.previous + 999_999) / 1_000_000
            armLocked(current, milliseconds: remaining)
            return nil
        }
        if let failure { onDeadline(failure) }
    }
    func makeConnectionCandidate() -> WorkerServiceConnection? {
        lock.withLock {
            let transition = ownership.issuingCandidate()
            ownership = transition.state
            return transition.candidate
        }
    }
    /// Called only from a requirement-checked invocation; claims routing ownership without a native lease.
    /// Acceptance is not native-idle/admission or completion evidence.
    func claimConnection(_ owner: WorkerServiceConnection) -> WorkerServiceAdmissionStatus {
        lock.withLock {
            guard !ownership.isRetired else { return .closed }
            let transition = ownership.claiming(owner)
            ownership = transition.state
            return transition.didClaim ? .accepted : .notOwner
        }
    }
    func submit(
        owner: WorkerServiceConnection, requestIdentity: UInt64, onCancelled: @escaping @Sendable () -> Void,
        _ operation: @escaping @Sendable (WorkerServiceLease) -> Void
    )
        -> WorkerServiceAdmissionDecision
    {
        var failure: WorkerServiceJobFailure?
        let decision: WorkerServiceAdmissionDecision = lock.withLock {
            guard !ownership.isRetired else { return .closed }
            guard requestIdentity != 0 else { return .invalidRequestIdentity }
            let claimed = ownership.claiming(owner)
            ownership = claimed.state
            guard claimed.didClaim else { return .notOwner }
            let transition = state.acquiring()
            if case .accepted = transition.decision {
                guard requestIdentity > lastAcceptedRequestIdentity else { return .invalidRequestIdentity }
                lastAcceptedRequestIdentity = requestIdentity
                activeRequestIdentity = requestIdentity
            }
            state = transition.state
            if case .accepted(let lease) = transition.decision {
                let start = timing.clock()
                do throws(WorkerServiceJobDeadlineError) {
                    let deadline = try WorkerServiceJobDeadline(budget: jobBudget, start: start)
                    let job = TimedJob(
                        owner: owner, request: requestIdentity, lease: lease,
                        deadline: deadline, previous: start)
                    timedJob = job
                    armLocked(job, milliseconds: jobBudget.milliseconds)
                } catch {
                    failure = sealLocked(owner: owner, request: requestIdentity, lease: lease, cause: error)
                    return .closed
                }
                // Retain the admitted operation through completion, never queue a rejected one.
                executor.enqueue {
                    var deadlineFailure: WorkerServiceJobFailure?
                    let cancelled: Bool? = self.lock.withLock {
                        guard self.terminalFailure == nil, self.state.activeLease == lease else { return nil }
                        deadlineFailure = self.checkLocked(owner: owner, lease: lease)
                        guard deadlineFailure == nil else { return nil }
                        if self.cancelledLease == lease { return true }
                        self.startedLease = lease
                        return false
                    }
                    if let deadlineFailure { self.onDeadline(deadlineFailure) }
                    guard let cancelled else { return }
                    if cancelled {
                        onCancelled()
                        _ = self.complete(owner: owner, lease)
                    } else {
                        operation(lease)
                    }
                }
            }
            return transition.decision
        }
        if let failure { onDeadline(failure) }
        return decision
    }
    func cancelQueued(owner: WorkerServiceConnection, requestIdentity: UInt64) -> WorkerServiceCancellationDecision {
        lock.withLock {
            guard ownership.owns(owner) else { return .notOwner }
            guard terminalFailure == nil, activeRequestIdentity == requestIdentity, let lease = state.activeLease else {
                return .stale
            }
            return cancelQueuedLocked(lease)
        }
    }
    func cancelQueued(owner: WorkerServiceConnection, _ lease: WorkerServiceLease) -> WorkerServiceCancellationDecision
    {
        lock.withLock {
            guard ownership.owns(owner) else { return .notOwner }
            guard terminalFailure == nil, state.activeLease == lease else { return .stale }
            return cancelQueuedLocked(lease)
        }
    }
    private func cancelQueuedLocked(_ lease: WorkerServiceLease) -> WorkerServiceCancellationDecision {
        if startedLease == lease { return .alreadyStarted }
        if cancelledLease == lease { return .alreadyCancelled }
        // Retain admission until the queued cancellation callback completes.
        cancelledLease = lease
        return .cancelled
    }
    func complete(owner: WorkerServiceConnection, _ lease: WorkerServiceLease) -> Bool {
        var failure: WorkerServiceJobFailure?
        let completed = lock.withLock {
            guard terminalFailure == nil, ownership.owns(owner), state.activeLease == lease else { return false }
            failure = checkLocked(owner: owner, lease: lease)
            guard failure == nil else { return false }
            let transition = state.completing(lease)
            state = transition.state
            if transition.didComplete {
                alarm?.cancel()
                alarm = nil
                timedJob = nil
                activeRequestIdentity = nil
                startedLease = nil
                cancelledLease = nil
            }
            return transition.didComplete
        }
        if let failure { onDeadline(failure) }
        return completed
    }
    @discardableResult
    func retire(owner: WorkerServiceConnection, beforeRetired: @escaping @Sendable () throws -> Void = {}) -> Bool {
        lock.withLock {
            let transition = ownership.retiring(owner)
            ownership = transition.state
            guard transition.didRetire else { return false }
            state = state.retired()
            // Enqueue under the same lock as admission, after all accepted submissions.
            executor.enqueue {
                guard self.lock.withLock({ self.terminalFailure == nil }) else { return }
                let result = Result { try beforeRetired() }
                self.onRetired(result)
            }
            return true
        }
    }
    func ownershipSnapshot() -> WorkerServiceOwnershipState { lock.withLock { ownership } }
    func snapshot() -> WorkerServiceAdmissionState { lock.withLock { state } }
}
