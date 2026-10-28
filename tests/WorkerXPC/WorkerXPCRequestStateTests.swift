import XCTest

// SPDX-License-Identifier: MIT
@testable import Dust3DWorkerXPC

final class WorkerXPCRequestStateTests: XCTestCase {
    func testOverlapAndDuplicateCompletionDoNotConsumeNewWork() throws {
        var state = WorkerXPCRequestState()
        let first = try state.begin(generation: 9)
        XCTAssertThrowsError(try state.begin(generation: 9)) { error in
            guard case WorkerXPCTransportError.busy = error else { return XCTFail("Unexpected cause: \(error)") }
        }
        XCTAssertFalse(state.finish(first + 1))
        XCTAssertEqual(state.activeToken, first)
        XCTAssertTrue(state.finish(first))
        XCTAssertFalse(state.finish(first))
        let second = try state.begin(generation: 9)
        XCTAssertGreaterThan(second, first)
        XCTAssertFalse(state.finish(first))
        XCTAssertEqual(state.activeToken, second)
        XCTAssertThrowsError(try state.close())
        XCTAssertTrue(state.finish(second))
        try state.close()
        try state.close()
        XCTAssertTrue(state.isRetired)
        XCTAssertThrowsError(try state.begin(generation: 9))
    }
    func testInvalidGenerationAndOverflowPreserveOriginalState() throws {
        var state = WorkerXPCRequestState()
        XCTAssertThrowsError(try state.begin(generation: 0))
        let first = try state.begin(generation: 9)
        XCTAssertTrue(state.finish(first))
        XCTAssertThrowsError(try state.begin(generation: 10))
        XCTAssertNil(state.activeToken)
        let second = try state.begin(generation: 9)
        XCTAssertGreaterThan(second, first)
        XCTAssertEqual(state.retire(), second)
        XCTAssertNil(state.retire())
        XCTAssertFalse(state.finish(second))
        XCTAssertThrowsError(try state.begin(generation: 9))
        var exhausted = WorkerXPCRequestState(lastToken: UInt64.max)
        XCTAssertThrowsError(try exhausted.begin(generation: 9)) { error in
            guard case WorkerXPCTransportError.sequenceExhausted = error else {
                return XCTFail("Unexpected cause: \(error)")
            }
        }
        XCTAssertNil(exhausted.activeToken)
    }
    func testPeerObservationCannotSilentlyRebindConnection() throws {
        var state = WorkerXPCRequestState()
        let token = try state.begin(generation: 9)
        XCTAssertThrowsError(try state.observePeer(0))
        try state.observePeer(42)
        try state.observePeer(42)
        XCTAssertThrowsError(try state.observePeer(43)) { error in
            guard case WorkerXPCTransportError.peerChanged(let expected, let observed) = error else {
                return XCTFail("Unexpected cause: \(error)")
            }
            XCTAssertEqual(expected, 42)
            XCTAssertEqual(observed, 43)
        }
        try state.observePeer(42)
        XCTAssertEqual(state.activeToken, token)
        XCTAssertTrue(state.finish(token))
    }

}
