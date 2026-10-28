// SPDX-License-Identifier: MIT
import Foundation

/// Original deadline cause and attempt identity. A nil token means ownership was not consumed.
/// This is failure metadata, never authentication, native drain or process-exit evidence.
public struct WorkerXPCHandshakeFailure: Error, Sendable {
    public let token: UInt64?
    public let generation: UInt64
    public let budgetMilliseconds: UInt64
    public let cause: WorkerXPCHandshakeDeadlineError
    /// Preserve a transport failure observed alongside expiry instead of hiding its original cause.
    public let originalTransportCause: (any Error)?
}

struct WorkerXPCHandshakeReplyFields: Sendable {
    let process: Int32
    let sandbox: Bool
    let admission: UInt32
    let instance: WorkerXPCServiceInstance
}
enum WorkerXPCHandshakeCaptureDelivery: Equatable, Sendable { case wake, coalesced, closed }
enum WorkerXPCHandshakeCaptureUpdate: Sendable {
    case waiting(milliseconds: UInt64)
    case completed(Result<WorkerXPCHandshakeReplyFields, any Error>, connectionFailure: (any Error)?)
}

/// The lock orders scalar capture and clock reads before actor scheduling. Storage is one terminal
/// result, one connection failure and one pending wake, independent of duplicate callback count.
/// The caller owns timers/continuations; this collector invokes neither and performs no IPC.
final class WorkerXPCHandshakeCapture: @unchecked Sendable {
    private let lock = NSLock()
    private let token: UInt64
    private let generation: UInt64
    private let deadline: WorkerXPCHandshakeDeadline
    private let clock: @Sendable () -> UInt64
    private var previous: UInt64
    private var result: Result<WorkerXPCHandshakeReplyFields, any Error>?
    private var connectionFailure: (any Error)?
    private var isScheduled = false
    private var isClosed = false
    init(
        token: UInt64, generation: UInt64,
        deadline: WorkerXPCHandshakeDeadline, clock: @escaping @Sendable () -> UInt64
    ) {
        self.token = token
        self.generation = generation
        self.deadline = deadline
        self.clock = clock
        previous = deadline.start
    }
    private func deadlineFailure(_ cause: WorkerXPCHandshakeDeadlineError, original: (any Error)?)
        -> WorkerXPCHandshakeFailure
    {
        WorkerXPCHandshakeFailure(
            token: token, generation: generation, budgetMilliseconds: deadline.budget.milliseconds,
            cause: cause, originalTransportCause: original)
    }
    // Called only while locked. Production clock readers must be nonblocking scalar reads.
    private func remaining(original: (any Error)? = nil) -> Result<UInt64, WorkerXPCHandshakeFailure> {
        let observed = clock()
        guard observed >= previous else {
            return .failure(deadlineFailure(.clockReversed(previous: previous, observed: observed), original: original))
        }
        previous = observed
        do { return .success(try deadline.remainingMilliseconds(at: observed)) } catch {
            return .failure(deadlineFailure(error, original: original))
        }
    }
    private func delivery() -> WorkerXPCHandshakeCaptureDelivery {
        guard !isScheduled else { return .coalesced }
        isScheduled = true
        return .wake
    }
    func reply(_ fields: WorkerXPCHandshakeReplyFields) -> WorkerXPCHandshakeCaptureDelivery {
        lock.withLock {
            guard !isClosed else { return .closed }
            guard result == nil else { return .coalesced }
            switch remaining() {
            case .success: result = .success(fields)
            case .failure(let cause): result = .failure(cause)
            }
            return delivery()
        }
    }
    func failure(_ cause: any Error, connection: Bool) -> WorkerXPCHandshakeCaptureDelivery {
        lock.withLock {
            guard !isClosed else { return .closed }
            // Connection loss still retires the transport after a timely selected reply is delivered.
            if connection, connectionFailure == nil { connectionFailure = cause }
            if result == nil {
                switch remaining(original: cause) {
                case .success: result = .failure(cause)
                case .failure(let deadlineCause): result = .failure(deadlineCause)
                }
            }
            return delivery()
        }
    }
    func timer() -> WorkerXPCHandshakeCaptureDelivery {
        lock.withLock {
            guard !isClosed else { return .closed }
            guard result == nil, !isScheduled else { return .coalesced }
            if case .failure(let cause) = remaining() { result = .failure(cause) }
            return delivery()
        }
    }
    func take() -> WorkerXPCHandshakeCaptureUpdate? {
        lock.withLock {
            guard !isClosed else { return nil }
            isScheduled = false
            // A captured terminal reply wins over later actor delay; never read a new clock over it.
            let completed: Result<WorkerXPCHandshakeReplyFields, any Error>
            if let result {
                completed = result
            } else {
                switch remaining() {
                case .success(let milliseconds): return .waiting(milliseconds: milliseconds)
                case .failure(let cause): completed = .failure(cause)
                }
            }
            let lostConnection = connectionFailure
            isClosed = true
            self.result = nil
            connectionFailure = nil
            return .completed(completed, connectionFailure: lostConnection)
        }
    }
    func close() {
        lock.withLock {
            isClosed = true
            isScheduled = false
            result = nil
            connectionFailure = nil
        }
    }
}
