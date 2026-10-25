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

private func pureOwnership() throws {
    let initial = WorkerServiceOwnershipState()
    let first = initial.issuingCandidate()
    guard let owner = first.candidate else { throw AdmissionCheckError.invalid("Initial candidate missing") }
    try require(initial.owner == nil && first.state.owner == nil, "Candidate issuance never claims ownership")
    try require(!initial.claiming(owner).didClaim, "Unissued candidate cannot claim")
    try require(!first.state.retiring(owner).didRetire, "Unclaimed disconnect cannot retire service")
    let second = first.state.issuingCandidate()
    guard let other = second.candidate else { throw AdmissionCheckError.invalid("Second candidate missing") }
    let claimed = second.state.claiming(owner)
    try require(
        claimed.didClaim && claimed.state.owner == owner && second.state.owner == nil,
        "First invocation claims one owner immutably")
    try require(claimed.state.claiming(owner).state == claimed.state, "Owner may reuse connection")
    try require(
        !claimed.state.claiming(other).didClaim && !claimed.state.retiring(other).didRetire,
        "Other candidate cannot steal or retire owner")
    let retired = claimed.state.retiring(owner)
    try require(
        retired.didRetire && retired.state.isRetired && retired.state.owns(owner),
        "Owner retirement retains routing for in-flight completion")
    try require(
        !retired.state.claiming(owner).didClaim && retired.state.issuingCandidate().candidate == nil,
        "Retired service cannot reopen")
    try require(!retired.state.retiring(owner).didRetire, "Duplicate retirement has no new effect")
    let last = WorkerServiceOwnershipState(lastIssuedGeneration: UInt64.max - 1).issuingCandidate()
    guard let finalCandidate = last.candidate else { throw AdmissionCheckError.invalid("Final candidate missing") }
    let finalOwner = last.state.claiming(finalCandidate)
    try require(
        finalOwner.didClaim && finalOwner.state.issuingCandidate().candidate == nil && !finalOwner.state.isRetired,
        "Candidate numeric exhaustion cannot wrap or retire an existing owner")
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
    guard let candidate = scheduler.makeConnectionCandidate() else {
        throw AdmissionCheckError.invalid("Missing candidate")
    }
    let first = scheduler.submit(owner: candidate) { lease in
        events.append("job")
        events.append(scheduler.complete(owner: candidate, lease) ? "released" : "invalid completion")
    }
    guard case .accepted = first else { throw AdmissionCheckError.invalid("Initial recorded job not admitted") }
    try require(first.status == .accepted && executor.count == 1, "Admission enqueues exactly one action")
    let busy = scheduler.submit(owner: candidate) { _ in events.append("unexpected busy action") }
    try require(busy == .busy && busy.status == .busy && executor.count == 1, "Busy action is never queued")
    scheduler.retire(owner: candidate)
    scheduler.retire(owner: candidate)
    let closed = scheduler.submit(owner: candidate) { _ in events.append("unexpected retired action") }
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
    guard let reusableCandidate = reusable.makeConnectionCandidate() else {
        throw AdmissionCheckError.invalid("Missing reusable candidate")
    }
    for _ in 0..<2 {
        guard
            case .accepted = reusable.submit(
                owner: reusableCandidate, { lease in _ = reusable.complete(owner: reusableCandidate, lease) })
        else {
            throw AdmissionCheckError.invalid("Completed live service did not release capacity")
        }
        try reusableExecutor.drainOne()
        try require(reusable.snapshot().activeLease == nil, "Normal completion releases capacity")
    }
}

private func recordedOwnership() throws {
    let executor = RecordingExecutor()
    let events = AdmissionEvents()
    let scheduler = WorkerServiceScheduler(executor: executor, onRetired: { events.append("stop") })
    guard let owner = scheduler.makeConnectionCandidate(), let other = scheduler.makeConnectionCandidate() else {
        throw AdmissionCheckError.invalid("Missing recorded ownership candidates")
    }
    try require(!scheduler.retire(owner: other) && executor.count == 0, "Unclaimed disconnect cannot queue stop")
    let accepted = scheduler.submit(owner: owner) { lease in
        events.append(scheduler.complete(owner: owner, lease) ? "owner completed" : "invalid completion")
    }
    guard case .accepted(let lease) = accepted else { throw AdmissionCheckError.invalid("Owner claim missing") }
    try require(scheduler.ownershipSnapshot().owner == owner, "Invocation claims owner before native enqueue")
    let foreign = scheduler.submit(owner: other) { _ in events.append("unexpected foreign job") }
    try require(
        foreign == .notOwner && foreign.status == .notOwner && executor.count == 1,
        "Other candidate cannot enqueue work")
    try require(
        !scheduler.complete(owner: other, lease) && scheduler.snapshot().activeLease == lease,
        "Foreign completion cannot release owner job")
    try require(!scheduler.retire(owner: other) && executor.count == 1, "Other disconnect cannot stop active owner")
    try executor.drainOne()
    try require(
        events.snapshot == ["owner completed"] && scheduler.snapshot().activeLease == nil,
        "Owner completion releases capacity")
    try require(
        scheduler.submit(owner: other, { _ in events.append("unexpected takeover") }) == .notOwner,
        "Job completion never transfers connection ownership")
    try require(
        scheduler.retire(owner: owner) && !scheduler.retire(owner: owner) && executor.count == 1,
        "Only owner queues one stop")
    try require(scheduler.makeConnectionCandidate() == nil, "Retirement blocks future candidates")
    try executor.drainOne()
    try require(events.snapshot == ["owner completed", "stop"], "Owner stop follows work exactly once")
}

@main
struct WorkerAdmissionSmoke {
    static func main() {
        do {
            try pureTransitions()
            try pureOwnership()
            try recordedScheduling()
            try recordedOwnership()
            print("Pure one-slot admission and recorded scheduling passed; no native work")
        } catch {
            FileHandle.standardError.write(Data("Admission fixture failed: \(error)\n".utf8))
            exit(EXIT_FAILURE)
        }
    }
}
