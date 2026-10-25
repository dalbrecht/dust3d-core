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
    private let executor: any WorkerServiceExecutor
    private let onRetired: @Sendable () -> Void
    init(
        executor: any WorkerServiceExecutor = DispatchWorkerServiceExecutor(), onRetired: @escaping @Sendable () -> Void
    ) {
        self.executor = executor
        self.onRetired = onRetired
    }
    func submit(_ operation: @escaping @Sendable (WorkerServiceLease) -> Void) -> WorkerServiceAdmissionDecision {
        lock.withLock {
            let transition = state.acquiring()
            state = transition.state
            if case .accepted(let lease) = transition.decision {
                // Retain the admitted operation through completion, never queue a rejected one.
                executor.enqueue { operation(lease) }
            }
            return transition.decision
        }
    }
    func complete(_ lease: WorkerServiceLease) -> Bool {
        lock.withLock {
            let transition = state.completing(lease)
            state = transition.state
            return transition.didComplete
        }
    }
    func retire() {
        lock.withLock {
            guard !state.isRetired else { return }
            state = state.retired()
            // Enqueue under the same lock as admission, after all accepted submissions.
            executor.enqueue(onRetired)
        }
    }
    func snapshot() -> WorkerServiceAdmissionState { lock.withLock { state } }
}
