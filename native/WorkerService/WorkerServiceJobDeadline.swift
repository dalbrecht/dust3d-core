// SPDX-License-Identifier: MIT

/// Trusted per-admitted-job wall duration, independent of cumulative CPU or native payload quotas.
public struct WorkerServiceJobBudget: Equatable, Sendable {
    public let milliseconds: UInt64
    /// Validate an explicit finite service policy before listener/job ownership.
    /// - Parameter milliseconds: Integer duration in 1...3600000ms. There is no unbounded/default policy.
    /// - Throws: Original invalid-budget value; validation never starts timers or changes process state.
    /// - Example: `try WorkerServiceJobBudget(milliseconds: 300000)`.
    public init(milliseconds: UInt64) throws(WorkerServiceJobDeadlineError) {
        guard milliseconds > 0, milliseconds <= 3_600_000 else {
            throw WorkerServiceJobDeadlineError.invalidBudget(milliseconds: milliseconds)
        }
        self.milliseconds = milliseconds
    }
}

/// Original scalar policy/clock cause; correlation with the service job is retained separately.
public enum WorkerServiceJobDeadlineError: Error, Equatable, Sendable {
    case invalidBudget(milliseconds: UInt64)
    case clockOverflow(start: UInt64, milliseconds: UInt64)
    case clockReversed(previous: UInt64, observed: UInt64)
    case expired(start: UInt64, deadline: UInt64, observed: UInt64)
}

/// Checked monotonic bounds. This value performs no clock reads, native work or process termination.
struct WorkerServiceJobDeadline: Sendable {
    let budget: WorkerServiceJobBudget
    let start: UInt64
    let end: UInt64
    init(budget: WorkerServiceJobBudget, start: UInt64) throws(WorkerServiceJobDeadlineError) {
        // The validated hour ceiling bounds multiplication; absolute uptime still needs checked addition.
        let end = start.addingReportingOverflow(budget.milliseconds * 1_000_000)
        guard !end.overflow else {
            throw WorkerServiceJobDeadlineError.clockOverflow(start: start, milliseconds: budget.milliseconds)
        }
        self.budget = budget
        self.start = start
        self.end = end.partialValue
    }
    func remainingMilliseconds(at observed: UInt64) throws(WorkerServiceJobDeadlineError) -> UInt64 {
        guard observed >= start else {
            throw WorkerServiceJobDeadlineError.clockReversed(previous: start, observed: observed)
        }
        guard observed < end else {
            throw WorkerServiceJobDeadlineError.expired(start: start, deadline: end, observed: observed)
        }
        return (end - observed + 999_999) / 1_000_000
    }
}
