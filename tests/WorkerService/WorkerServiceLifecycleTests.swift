// SPDX-License-Identifier: MIT
import Foundation
import XCTest

@testable import Dust3DWorkerService

private enum CleanupCause: Error { case original }
private final class RecordedExecutor: WorkerServiceExecutor, @unchecked Sendable {
    private let lock = NSLock()
    private var operations: [@Sendable () -> Void] = []
    func enqueue(_ operation: @escaping @Sendable () -> Void) {
        lock.withLock { operations.append(operation) }
    }
    func drain() {
        while let operation = lock.withLock({ operations.isEmpty ? nil : operations.removeFirst() }) {
            operation()
        }
    }
}
private final class RetirementFacts: @unchecked Sendable {
    private let lock = NSLock()
    private var recorded: [String] = []
    var events: [String] { lock.withLock { recorded } }
    func record(_ value: String) { lock.withLock { recorded.append(value) } }
}
final class WorkerServiceLifecycleTests: XCTestCase {
    func testShouldDrainAcceptedWorkBeforeReturningOriginalCleanupCause() throws {
        let executor = RecordedExecutor()
        let facts = RetirementFacts()
        let scheduler = WorkerServiceScheduler(executor: executor) { result in
            switch result {
            case .success: facts.record("unexpected success")
            case .failure(CleanupCause.original): facts.record("original cause")
            case .failure: facts.record("replaced cause")
            }
        }
        let candidate = try XCTUnwrap(scheduler.makeConnectionCandidate())
        let decision = scheduler.submit(
            owner: candidate, requestIdentity: 1, onCancelled: {},
            { lease in
                facts.record("work")
                XCTAssertTrue(scheduler.complete(owner: candidate, lease))
            })
        guard case .accepted = decision else { return XCTFail("Admission rejected") }
        XCTAssertTrue(
            scheduler.retire(owner: candidate) {
                facts.record("cleanup")
                throw CleanupCause.original
            })
        XCTAssertEqual(facts.events, [])
        XCTAssertEqual(
            scheduler.submit(owner: candidate, requestIdentity: 2, onCancelled: {}) { _ in
                facts.record("unexpected work")
            }, .closed)
        executor.drain()
        XCTAssertEqual(facts.events, ["work", "cleanup", "original cause"])
        XCTAssertFalse(scheduler.retire(owner: candidate))
    }
    func testShouldLeaveUnclaimedCandidateWithoutRetirementCallback() throws {
        let executor = RecordedExecutor()
        let facts = RetirementFacts()
        let scheduler = WorkerServiceScheduler(executor: executor) { _ in facts.record("retired") }
        let candidate = try XCTUnwrap(scheduler.makeConnectionCandidate())
        XCTAssertFalse(scheduler.retire(owner: candidate))
        executor.drain()
        XCTAssertEqual(facts.events, [])
        XCTAssertFalse(scheduler.snapshot().isRetired)
    }

    func testShouldClaimHandshakeWithoutJobLeaseOrRequestConsumption() throws {
        let executor = RecordedExecutor()
        let facts = RetirementFacts()
        let scheduler = WorkerServiceScheduler(executor: executor) { _ in facts.record("retired") }
        let owner = try XCTUnwrap(scheduler.makeConnectionCandidate())
        let other = try XCTUnwrap(scheduler.makeConnectionCandidate())
        XCTAssertEqual(scheduler.claimConnection(owner), .accepted)
        XCTAssertEqual(scheduler.claimConnection(owner), .accepted)
        XCTAssertEqual(scheduler.claimConnection(other), .notOwner)
        XCTAssertNil(scheduler.snapshot().activeLease)
        XCTAssertEqual(scheduler.snapshot().lastIssuedGeneration, 0)
        let decision = scheduler.submit(
            owner: owner, requestIdentity: 1, onCancelled: {},
            { lease in
                facts.record("job")
                XCTAssertTrue(scheduler.complete(owner: owner, lease))
            })
        guard case .accepted = decision else { return XCTFail("Handshake consumed first job") }
        XCTAssertTrue(scheduler.retire(owner: owner))
        XCTAssertEqual(scheduler.claimConnection(owner), .closed)
        XCTAssertEqual(facts.events, [])
        executor.drain()
        XCTAssertEqual(facts.events, ["job", "retired"])
    }
}
