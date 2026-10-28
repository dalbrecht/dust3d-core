// SPDX-License-Identifier: MIT

/// Explicit pre-job handshake wait, independent of active native work or process termination.
public struct WorkerXPCHandshakeBudget: Equatable, Sendable {
    public let milliseconds: UInt64
    /// Validate the caller's explicit bounded wait before dispatch or RPC token consumption.
    /// - Parameter milliseconds: Integer duration in 1...10000 milliseconds; no unbounded fallback.
    /// - Throws: `WorkerXPCHandshakeDeadlineError.invalidBudget` retaining the requested value.
    /// - Example: `try WorkerXPCHandshakeBudget(milliseconds: 5000)`.
    public init(milliseconds: UInt64) throws(WorkerXPCHandshakeDeadlineError) {
        guard milliseconds > 0, milliseconds <= 10000 else {
            throw WorkerXPCHandshakeDeadlineError.invalidBudget(milliseconds: milliseconds)
        }
        self.milliseconds = milliseconds
    }
}

/// Original scalar clock/policy cause. Transport correlation is retained separately with the RPC attempt.
public enum WorkerXPCHandshakeDeadlineError: Error, Equatable, Sendable {
    case invalidBudget(milliseconds: UInt64)
    case clockOverflow(start: UInt64, milliseconds: UInt64)
    case clockReversed(previous: UInt64, observed: UInt64)
    case expired(start: UInt64, deadline: UInt64, observed: UInt64)
}

/// Immutable monotonic nanosecond bounds; no clock reads, timers or IPC side effects.
struct WorkerXPCHandshakeDeadline: Sendable {
    let budget: WorkerXPCHandshakeBudget
    let start: UInt64
    let end: UInt64
    init(budget: WorkerXPCHandshakeBudget, start: UInt64) throws(WorkerXPCHandshakeDeadlineError) {
        // The validated maximum bounds multiplication before the checked absolute-time addition.
        let end = start.addingReportingOverflow(budget.milliseconds * 1_000_000)
        guard !end.overflow else {
            throw WorkerXPCHandshakeDeadlineError.clockOverflow(start: start, milliseconds: budget.milliseconds)
        }
        self.budget = budget
        self.start = start
        self.end = end.partialValue
    }
    func remainingMilliseconds(at observed: UInt64) throws(WorkerXPCHandshakeDeadlineError) -> UInt64 {
        guard observed >= start else {
            throw WorkerXPCHandshakeDeadlineError.clockReversed(previous: start, observed: observed)
        }
        guard observed < end else {
            throw WorkerXPCHandshakeDeadlineError.expired(start: start, deadline: end, observed: observed)
        }
        // Round only the bounded remainder upward; an early wakeup must never extend the original end.
        return (end - observed + 999_999) / 1_000_000
    }
}
