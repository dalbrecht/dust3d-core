// SPDX-License-Identifier: MIT
import Darwin
import Foundation

private enum AdmissionCheckError: Error { case invalid(String) }
private func require(_ value: Bool, _ message: String) throws {
    if !value { throw AdmissionCheckError.invalid(message) }
}
private func pureTransitions() throws {
    let idle = WorkerServiceAdmissionState()
    let first = idle.acquiring()
    guard case .accepted(let lease) = first.decision else { throw AdmissionCheckError.invalid("Initial slot rejected") }
    try require(
        idle.activeLease == nil && first.state.activeLease == lease, "Pure transition must leave old state unchanged")
    try require(first.state.acquiring().decision == .busy, "Only one native invocation is retained")
    let completed = first.state.completing(lease)
    try require(completed.didComplete && completed.state.activeLease == nil, "Matching completion releases slot")
    let second = completed.state.acquiring()
    guard case .accepted(let next) = second.decision else {
        throw AdmissionCheckError.invalid("Released slot unavailable")
    }
    try require(next.generation > lease.generation, "Leases cannot repeat")
    let stale = second.state.completing(lease)
    try require(!stale.didComplete && stale.state == second.state, "Old completion cannot release current work")
    try require(!idle.completing(lease).didComplete, "Idle completion cannot retire new work")
    let retiring = second.state.retired()
    try require(
        retiring.acquiring().decision == .closed && retiring.activeLease == next,
        "Retirement seals admission without interrupting active work")
    try require(retiring.retired() == retiring, "Retirement is idempotent")
    let drained = retiring.completing(next)
    try require(
        drained.didComplete && drained.state.activeLease == nil && drained.state.acquiring().decision == .closed,
        "Drained retired service cannot restart")
    let limit = WorkerServiceAdmissionState(lastIssuedGeneration: UInt64.max - 1).acquiring()
    guard case .accepted(let last) = limit.decision else {
        throw AdmissionCheckError.invalid("Final nonwrapping lease lost")
    }
    try require(
        last.generation == UInt64.max && limit.state.acquiring().decision == .busy,
        "Last active lease still owns one slot")
    let exhausted = limit.state.completing(last).state.acquiring()
    try require(
        exhausted.decision == .closed && exhausted.state.isRetired, "Sequence exhaustion closes instead of wrapping")
    try require(
        WorkerServiceAdmissionState(lastIssuedGeneration: UInt64.max).acquiring().decision == .closed,
        "Initially exhausted value closes without allocation")
}

private final class RecordingExecutor: WorkerServiceExecutor, @unchecked Sendable {
    private let lock = NSLock()
    private var pending: [@Sendable () -> Void] = []
    var count: Int { lock.withLock { pending.count } }
    func enqueue(_ operation: @escaping @Sendable () -> Void) { lock.withLock { pending.append(operation) } }
    func drainOne() throws {
        let next: (@Sendable () -> Void)? = lock.withLock {
            guard !pending.isEmpty else { return nil }
            return pending.removeFirst()
        }
        guard let next else { throw AdmissionCheckError.invalid("Missing recorded action") }
        next()
    }
}
private final class AdmissionEvents: @unchecked Sendable {
    private let lock = NSLock()
    private var values: [String] = []
    func append(_ value: String) { lock.withLock { values.append(value) } }
    var snapshot: [String] { lock.withLock { values } }
}
private func recordedScheduling() throws {
    let executor = RecordingExecutor()
    let events = AdmissionEvents()
    let scheduler = WorkerServiceScheduler(executor: executor, onRetired: { events.append("stop") })
    let first = scheduler.submit { lease in
        events.append("job")
        events.append(scheduler.complete(lease) ? "released" : "invalid completion")
    }
    guard case .accepted = first else { throw AdmissionCheckError.invalid("Initial recorded job not admitted") }
    try require(first.status == .accepted && executor.count == 1, "Admission enqueues exactly one action")
    let busy = scheduler.submit { _ in events.append("unexpected busy action") }
    try require(busy == .busy && busy.status == .busy && executor.count == 1, "Busy action is never queued")
    scheduler.retire()
    scheduler.retire()
    let closed = scheduler.submit { _ in events.append("unexpected retired action") }
    try require(
        closed == .closed && closed.status == .closed && executor.count == 2,
        "Stop is ordered once after accepted work; closed action never queued")
    try executor.drainOne()
    try require(
        events.snapshot == ["job", "released"] && scheduler.snapshot().isRetired
            && scheduler.snapshot().activeLease == nil, "Retired work drains without reopening")
    try executor.drainOne()
    try require(
        events.snapshot == ["job", "released", "stop"] && executor.count == 0, "Ordered stop follows native action")

    let reusableExecutor = RecordingExecutor()
    let reusable = WorkerServiceScheduler(executor: reusableExecutor, onRetired: {})
    for _ in 0..<2 {
        guard case .accepted = reusable.submit({ lease in _ = reusable.complete(lease) }) else {
            throw AdmissionCheckError.invalid("Completed live service did not release capacity")
        }
        try reusableExecutor.drainOne()
        try require(reusable.snapshot().activeLease == nil, "Normal completion releases capacity")
    }
}

@main
struct WorkerAdmissionSmoke {
    static func main() {
        do {
            try pureTransitions()
            try recordedScheduling()
            print("Pure one-slot admission and recorded scheduling passed; no native work")
        } catch {
            FileHandle.standardError.write(Data("Admission fixture failed: \(error)\n".utf8))
            exit(EXIT_FAILURE)
        }
    }
}
