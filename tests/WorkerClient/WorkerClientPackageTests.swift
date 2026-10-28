// SPDX-License-Identifier: MIT
import Dust3DWorkerClient
import Foundation
import XCTest

private struct Snapshot: Sendable {
    let archive: Data
    let indexedBudget: UInt64
}
final class WorkerClientPackageTests: XCTestCase {
    func testOwnedSnapshotSurvivesSuccessfulCompletion() throws {
        let identity = try WorkerPreviewIdentity(document: UUID(), request: 1, revision: 0, generation: 9)
        let owner = WorkerPreviewSourceCoordinator<Snapshot>()
        var archive = Data([1, 2, 3])
        try owner.submit(identity, source: Snapshot(archive: archive, indexedBudget: UInt64.max))
        archive[0] = 9
        XCTAssertEqual(owner.begin(), identity)
        let snapshot = try owner.source(for: identity)
        XCTAssertEqual(snapshot.archive, Data([1, 2, 3]))
        XCTAssertEqual(snapshot.indexedBudget, UInt64.max)
        let receipt = WorkerPreviewControlReceipt(
            status: 0, request: 1, systemError: 0, stage: 0,
            source: 0, code: 0, truncated: 0, process: 42, sandboxEntitlementVerified: true,
            finalizationError: 0, admissionStatus: 0, boundaryStatus: 0, connectionGeneration: 9,
            observedProcess: 42, cause: Data())
        try owner.complete(identity, receipt: receipt, reachedEOF: true)
        XCTAssertNil(owner.activeIdentity)
        XCTAssertEqual(snapshot.archive, Data([1, 2, 3]))
        owner.close()
        XCTAssertTrue(owner.isClosed)
    }
    func testForeignScopeCannotReplaceWaitingSnapshot() throws {
        let identity = try WorkerPreviewIdentity(document: UUID(), request: 1, revision: 0, generation: 9)
        let foreign = try WorkerPreviewIdentity(document: UUID(), request: 2, revision: 1, generation: 9)
        let owner = WorkerPreviewSourceCoordinator<String>()
        try owner.submit(identity, source: "original")
        XCTAssertThrowsError(try owner.submit(foreign, source: "foreign")) { error in
            guard case WorkerPreviewQueueError.foreignScope = error else {
                return XCTFail("Unexpected original error: \(error)")
            }
        }
        XCTAssertEqual(owner.waitingIdentity, identity)
        XCTAssertEqual(owner.begin(), identity)
        XCTAssertEqual(try owner.source(for: identity), "original")
        try owner.abandon(identity)
    }
}
