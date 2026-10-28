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
    private let onRetired: @Sendable (Result<Void, any Error>) -> Void
    init(
        executor: any WorkerServiceExecutor = DispatchWorkerServiceExecutor(),
        onRetired: @escaping @Sendable (Result<Void, any Error>) -> Void
    ) {
        self.executor = executor
        self.onRetired = onRetired
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
        lock.withLock {
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
                // Retain the admitted operation through completion, never queue a rejected one.
                executor.enqueue {
                    let cancelled: Bool? = self.lock.withLock {
                        guard self.state.activeLease == lease else { return nil }
                        if self.cancelledLease == lease { return true }
                        self.startedLease = lease
                        return false
                    }
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
    }
    func cancelQueued(owner: WorkerServiceConnection, requestIdentity: UInt64) -> WorkerServiceCancellationDecision {
        lock.withLock {
            guard ownership.owns(owner) else { return .notOwner }
            guard activeRequestIdentity == requestIdentity, let lease = state.activeLease else { return .stale }
            return cancelQueuedLocked(lease)
        }
    }
    func cancelQueued(owner: WorkerServiceConnection, _ lease: WorkerServiceLease) -> WorkerServiceCancellationDecision
    {
        lock.withLock {
            guard ownership.owns(owner) else { return .notOwner }
            guard state.activeLease == lease else { return .stale }
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
        lock.withLock {
            guard ownership.owns(owner) else { return false }
            let transition = state.completing(lease)
            state = transition.state
            if transition.didComplete {
                activeRequestIdentity = nil
                startedLease = nil
                cancelledLease = nil
            }
            return transition.didComplete
        }
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
                let result = Result { try beforeRetired() }
                self.onRetired(result)
            }
            return true
        }
    }
    func ownershipSnapshot() -> WorkerServiceOwnershipState { lock.withLock { ownership } }
    func snapshot() -> WorkerServiceAdmissionState { lock.withLock { state } }
}
