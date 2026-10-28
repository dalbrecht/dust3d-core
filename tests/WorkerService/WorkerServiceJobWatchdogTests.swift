// SPDX-License-Identifier: MIT
import Foundation
import XCTest

@testable import Dust3DWorkerService

private struct JobTimerCancellation: WorkerServiceJobTimerCancellation {
    let perform: @Sendable () -> Void
    func cancel() { perform() }
}
private final class JobWatchdogFacts: WorkerServiceExecutor, @unchecked Sendable {
    private let lock = NSLock()
    private var instant: UInt64 = 100
    private var operations: [@Sendable () -> Void] = []
    private var callbacks: [WorkerServiceJobTimeout] = []
    private var intervals: [UInt64] = []
    private var failures: [WorkerServiceJobFailure] = []
    private var clockReads = 0
    private var work = 0
    private var cancellations = 0
    func enqueue(_ operation: @escaping @Sendable () -> Void) { lock.withLock { operations.append(operation) } }
    func drain() {
        while let operation = lock.withLock({ operations.isEmpty ? nil : operations.removeFirst() }) { operation() }
    }
    func set(_ value: UInt64) { lock.withLock { instant = value } }
    func fire(_ index: Int) {
        let callback = lock.withLock { callbacks[index] }
        callback.fire()
    }
    func worked() { lock.withLock { work += 1 } }
    func failed(_ failure: WorkerServiceJobFailure) { lock.withLock { failures.append(failure) } }
    var observed: [WorkerServiceJobFailure] { lock.withLock { failures } }
    var requested: [UInt64] { lock.withLock { intervals } }
    var reads: Int { lock.withLock { clockReads } }
    var workedCount: Int { lock.withLock { work } }
    var cancelledCount: Int { lock.withLock { cancellations } }
    var timing: WorkerServiceJobTiming {
        WorkerServiceJobTiming(
            clock: {
                self.lock.withLock {
                    self.clockReads += 1
                    return self.instant
                }
            },
            timer: { interval, callback in
                self.lock.withLock {
                    self.intervals.append(interval)
                    self.callbacks.append(callback)
                }
                return JobTimerCancellation(perform: { self.lock.withLock { self.cancellations += 1 } })
            })
    }
}

/// Recording callbacks only: no native work, real timer expiry or actual process-exit boundary.
final class WorkerServiceJobWatchdogTests: XCTestCase {
    private func scheduler(_ facts: JobWatchdogFacts) throws -> WorkerServiceScheduler {
        WorkerServiceScheduler(
            executor: facts, jobBudget: try WorkerServiceJobBudget(milliseconds: 2), timing: facts.timing,
            onDeadline: { facts.failed($0) }, onRetired: { _ in facts.worked() })
    }
    func testShouldSealExpiredQueuedJobBeforeDispatchAndRetainOriginalAttemptOnce() throws {
        let facts = JobWatchdogFacts()
        let scheduler = try scheduler(facts)
        let owner = try XCTUnwrap(scheduler.makeConnectionCandidate())
        let result = scheduler.submit(
            owner: owner, requestIdentity: 7, onCancelled: { facts.worked() },
            { _ in
                facts.worked()
            })
        guard case .accepted(let lease) = result else { return XCTFail("Ordinary fake admission failed") }
        XCTAssertEqual(facts.requested, [2])
        facts.set(2_000_100)
        facts.fire(0)
        facts.fire(0)
        let failure = try XCTUnwrap(facts.observed.first)
        XCTAssertEqual(facts.observed.count, 1)
        XCTAssertEqual(failure.ownerGeneration, owner.generation)
        XCTAssertEqual(failure.leaseGeneration, lease.generation)
        XCTAssertEqual(failure.requestIdentity, 7)
        XCTAssertEqual(failure.budgetMilliseconds, 2)
        XCTAssertEqual(failure.cause, .expired(start: 100, deadline: 2_000_100, observed: 2_000_100))
        XCTAssertTrue(scheduler.snapshot().isRetired)
        XCTAssertEqual(scheduler.claimConnection(owner), .closed)
        XCTAssertFalse(scheduler.complete(owner: owner, lease))
        XCTAssertFalse(scheduler.retire(owner: owner))
        XCTAssertEqual(scheduler.submit(owner: owner, requestIdentity: 8, onCancelled: {}, { _ in }), .closed)
        facts.drain()
        XCTAssertEqual(facts.workedCount, 0)
        XCTAssertGreaterThan(facts.cancelledCount, 0)
    }
    func testShouldIgnoreOldTimerBeforeClockReadAndPreserveNewerLease() throws {
        let facts = JobWatchdogFacts()
        let scheduler = try scheduler(facts)
        let owner = try XCTUnwrap(scheduler.makeConnectionCandidate())
        guard case .accepted(let first) = scheduler.submit(owner: owner, requestIdentity: 1, onCancelled: {}, { _ in })
        else { return XCTFail("First fake job rejected") }
        XCTAssertTrue(scheduler.complete(owner: owner, first))
        guard case .accepted(let second) = scheduler.submit(owner: owner, requestIdentity: 2, onCancelled: {}, { _ in })
        else { return XCTFail("Second fake job rejected") }
        let reads = facts.reads
        facts.set(UInt64.max)
        facts.fire(0)
        XCTAssertEqual(facts.reads, reads)
        XCTAssertTrue(facts.observed.isEmpty)
        XCTAssertEqual(scheduler.snapshot().activeLease, second)
        facts.set(101)
        XCTAssertTrue(scheduler.complete(owner: owner, second))
    }
    func testShouldRearmOnlyOriginalRemainingTimeAndFailLateCompletionBeforeTimerDelivery() throws {
        let facts = JobWatchdogFacts()
        let scheduler = try scheduler(facts)
        let owner = try XCTUnwrap(scheduler.makeConnectionCandidate())
        let result = scheduler.submit(owner: owner, requestIdentity: 9, onCancelled: {}, { _ in facts.worked() })
        guard case .accepted(let lease) = result else { return XCTFail("Fake job rejected") }
        facts.drain()
        XCTAssertEqual(facts.workedCount, 1)
        facts.set(1_000_100)
        facts.fire(0)
        XCTAssertEqual(facts.requested, [2, 1])
        facts.set(2_000_100)
        XCTAssertFalse(scheduler.complete(owner: owner, lease))
        facts.fire(1)
        XCTAssertEqual(facts.observed.count, 1)
        XCTAssertEqual(facts.observed.first?.cause, .expired(start: 100, deadline: 2_000_100, observed: 2_000_100))
    }
    func testShouldSealLateDispatchAndSuppressPreviouslyQueuedRetirementCleanup() throws {
        let facts = JobWatchdogFacts()
        let scheduler = try scheduler(facts)
        let owner = try XCTUnwrap(scheduler.makeConnectionCandidate())
        guard
            case .accepted = scheduler.submit(
                owner: owner, requestIdentity: 5, onCancelled: {}, { _ in facts.worked() })
        else { return XCTFail("Fake admission failed") }
        XCTAssertTrue(scheduler.retire(owner: owner, beforeRetired: { facts.worked() }))
        facts.set(2_000_100)
        facts.drain()
        XCTAssertEqual(facts.workedCount, 0)
        XCTAssertEqual(facts.observed.count, 1)
        facts.fire(0)
        XCTAssertEqual(facts.observed.count, 1)
    }
    func testShouldRejectBusyBeforeClockReadAndExpireRunningFakeOnIndependentTimer() throws {
        let facts = JobWatchdogFacts()
        let scheduler = try scheduler(facts)
        let owner = try XCTUnwrap(scheduler.makeConnectionCandidate())
        guard
            case .accepted(let lease) = scheduler.submit(
                owner: owner, requestIdentity: 1, onCancelled: {}, { _ in facts.worked() })
        else { return XCTFail("Fake admission failed") }
        facts.drain()
        let reads = facts.reads
        facts.set(UInt64.max)
        XCTAssertEqual(scheduler.submit(owner: owner, requestIdentity: 2, onCancelled: {}, { _ in }), .busy)
        XCTAssertEqual(facts.reads, reads)
        facts.set(2_000_100)
        facts.fire(0)
        XCTAssertEqual(facts.observed.count, 1)
        XCTAssertFalse(scheduler.complete(owner: owner, lease))
        XCTAssertEqual(facts.workedCount, 1)
    }
    func testShouldDetectReversedClockRelativeToLatestEarlyWake() throws {
        let facts = JobWatchdogFacts()
        let scheduler = try scheduler(facts)
        let owner = try XCTUnwrap(scheduler.makeConnectionCandidate())
        _ = scheduler.submit(owner: owner, requestIdentity: 1, onCancelled: {}, { _ in })
        facts.set(1_000_100)
        facts.fire(0)
        facts.set(1_000_099)
        facts.fire(1)
        XCTAssertEqual(facts.observed.first?.cause, .clockReversed(previous: 1_000_100, observed: 1_000_099))
    }
    func testShouldPreserveClockFailureWithoutQueueingOrRunningNativeWork() throws {
        for isOverflow in [true, false] {
            let facts = JobWatchdogFacts()
            let scheduler = try scheduler(facts)
            let owner = try XCTUnwrap(scheduler.makeConnectionCandidate())
            if isOverflow { facts.set(UInt64.max - 1) }
            let result = scheduler.submit(owner: owner, requestIdentity: 3, onCancelled: {}, { _ in facts.worked() })
            if isOverflow {
                XCTAssertEqual(result, .closed)
                XCTAssertEqual(facts.observed.first?.cause, .clockOverflow(start: UInt64.max - 1, milliseconds: 2))
            } else {
                facts.set(99)
                facts.fire(0)
                XCTAssertEqual(facts.observed.first?.cause, .clockReversed(previous: 100, observed: 99))
            }
            facts.drain()
            XCTAssertEqual(facts.workedCount, 0)
            XCTAssertEqual(facts.observed.count, 1)
        }
    }
}
