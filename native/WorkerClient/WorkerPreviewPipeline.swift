// SPDX-License-Identifier: MIT
import Foundation

/// Trusted backend boundary. Return/throw only after owned execution/channel cleanup drains.
/// Implementations must independently authenticate receipts and observe actual EOF/write completion.
/// Never return borrowed native buffers, treat control errors as drain, or interrupt native work on task cancellation.
public protocol WorkerPreviewExecuting: Sendable {
    associatedtype Source: Sendable
    associatedtype Value: Sendable
    /// Execute one immutable captured snapshot off the UI actor, preserving original typed errors.
    /// - Parameters:
    ///   - identity: Host-captured scope and monotonic request, never reply-derived.
    ///   - source: Independently owned immutable archive/control snapshot.
    /// - Returns: Owned value with independently gathered transport observations.
    /// - Throws: Original backend error after its owned cleanup; this is not process-disappearance proof.
    func execute(identity: WorkerPreviewIdentity, source: Source) async throws -> WorkerPreviewExecutionCompletion<
        Value
    >
}

/// Completed owned execution; supplying these fields does not itself establish transport trust.
public struct WorkerPreviewExecutionCompletion<Value: Sendable>: Sendable {
    public let value: Value
    public let receipt: WorkerPreviewControlReceipt
    public let requestWritten: Bool
    public let reachedEOF: Bool
    /// Preserve the original control receipt and independent write/semantic-EOF observations.
    /// - Parameters:
    ///   - value: Fully copied immutable output, no borrowed native views.
    ///   - receipt: Authenticated original fields and independently observed peer/lifetime.
    ///   - requestWritten: Entire captured request finished writing successfully.
    ///   - reachedEOF: Decoder completed the correlated semantic reply through actual descriptor EOF.
    public init(value: Value, receipt: WorkerPreviewControlReceipt, requestWritten: Bool, reachedEOF: Bool) {
        self.value = value
        self.receipt = receipt
        self.requestWritten = requestWritten
        self.reachedEOF = reachedEOF
    }
}

public enum WorkerPreviewExecutionError: Error {
    case incompleteRequest(WorkerPreviewControlReceipt)
    case retirementPending
    case cleanup(operation: any Error, cleanup: any Error)
}

/// Async one-active/newest-waiting coordinator for a single document/connection lifetime.
/// Cancellation retires publication; the unstructured execution is never cancelled and retains
/// its slot/snapshot until backend cleanup returns. Backend deadlines/process supervision remain separate.
public actor WorkerPreviewPipeline<Executor: WorkerPreviewExecuting> {
    private struct Work: Sendable {
        let identity: WorkerPreviewIdentity
        let source: Executor.Source
        let continuation: CheckedContinuation<Executor.Value, any Error>
    }
    private let executor: Executor
    private let queue = WorkerPreviewPublicationQueue()
    // Queue admission precedes storage without suspension; globally monotonic IDs key at most two owners.
    private var work: [UInt64: Work] = [:]
    private var cancelledActive = false
    private var retirementWaiter: CheckedContinuation<Void, any Error>?

    /// Create an empty coordinator; no backend work starts until generate is accepted.
    public init(executor: Executor) { self.executor = executor }
    public var activeIdentity: WorkerPreviewIdentity? { queue.activeIdentity }
    public var waitingIdentity: WorkerPreviewIdentity? { queue.waitingIdentity }
    public var isRetired: Bool { queue.isClosed }

    /// Retain the newest accepted immutable snapshot and await its admitted owned value.
    /// - Parameters:
    ///   - identity: Valid host-selected scope and increasing request identity.
    ///   - source: Immutable owned archive and frozen policies; no shared mutable or borrowed memory.
    /// - Returns: Value admitted against the current source when backend cleanup finishes.
    ///   The UI must recheck its captured identity on the main actor before applying the result.
    /// - Throws: Original queue/backend/admission error, or CancellationError after active cleanup.
    ///   Waiting displacement/cancellation finishes promptly; active execution is never interrupted.
    public func generate(identity: WorkerPreviewIdentity, source: Executor.Source) async throws -> Executor.Value {
        try Task.checkCancellation()
        let value: Executor.Value = try await withTaskCancellationHandler {
            try await withCheckedThrowingContinuation {
                (continuation: CheckedContinuation<Executor.Value, any Error>) in
                guard !Task.isCancelled else {
                    continuation.resume(throwing: CancellationError())
                    return
                }
                do {
                    let replaced = try queue.submit(identity)
                    if let replaced {
                        work.removeValue(forKey: replaced.request)?.continuation.resume(throwing: CancellationError())
                    }
                    work[identity.request] = Work(identity: identity, source: source, continuation: continuation)
                    startNext()
                } catch { continuation.resume(throwing: error) }
            }
        } onCancel: {
            Task { await self.cancelPublication(for: identity) }
        }
        try Task.checkCancellation()
        return value
    }

    /// Seal publication/submission and wait for the active backend to return from owned cleanup.
    /// - Throws: retirementPending for a second concurrent waiter; there is at most one retained waiter.
    /// This does not invalidate a service connection, prove process exit, or impose a native deadline.
    /// Active callers retain their original backend error; successful drained work is rejected as retired.
    public func retire() async throws {
        guard retirementWaiter == nil else { throw WorkerPreviewExecutionError.retirementPending }
        let waiting = queue.waitingIdentity
        queue.close()
        if let waiting {
            work.removeValue(forKey: waiting.request)?.continuation.resume(throwing: WorkerPreviewQueueError.closed)
        }
        guard queue.activeIdentity != nil else { return }
        try await withCheckedThrowingContinuation { retirementWaiter = $0 }
    }

    /// Retire a matching request's publication, without interrupting active execution.
    /// - Parameter identity: Exact host identity; foreign/stale identities have no effect.
    /// Waiting callers finish promptly; active callers retain capacity until backend cleanup returns.
    public func cancelPublication(for identity: WorkerPreviewIdentity) {
        guard work[identity.request]?.identity == identity else { return }
        let isActive = queue.activeIdentity == identity
        queue.cancel(identity)
        if isActive {
            cancelledActive = true
        } else {
            work.removeValue(forKey: identity.request)?.continuation.resume(throwing: CancellationError())
        }
    }
    private func startNext() {
        guard queue.activeIdentity == nil, let identity = queue.waitingIdentity, let pending = work[identity.request]
        else { return }
        _ = queue.begin()
        let executor = executor
        // A caller's cancellation must not propagate into a native backend or free its active slot.
        Task.detached {
            let result: Result<WorkerPreviewExecutionCompletion<Executor.Value>, any Error>
            do { result = .success(try await executor.execute(identity: identity, source: pending.source)) } catch {
                result = .failure(error)
            }
            await self.finish(pending, result: result)
        }
    }
    private func finish(
        _ pending: Work, result: Result<WorkerPreviewExecutionCompletion<Executor.Value>, any Error>
    ) {
        let identity = pending.identity
        work.removeValue(forKey: identity.request)
        let completion: Result<Executor.Value, any Error>
        switch result {
        case .failure(let cause):
            do {
                try queue.abandon(identity)
                completion = .failure(cause)
            } catch { completion = .failure(WorkerPreviewExecutionError.cleanup(operation: cause, cleanup: error)) }
        case .success(let output):
            do {
                guard output.requestWritten else {
                    try queue.abandon(identity)
                    throw WorkerPreviewExecutionError.incompleteRequest(output.receipt)
                }
                try queue.complete(identity, receipt: output.receipt, reachedEOF: output.reachedEOF)
                completion = .success(output.value)
            } catch WorkerPreviewAdmissionError.retired where cancelledActive {
                completion = .failure(CancellationError())
            } catch { completion = .failure(error) }
        }
        cancelledActive = false
        pending.continuation.resume(with: completion)
        retirementWaiter?.resume()
        retirementWaiter = nil
        startNext()
    }
}
