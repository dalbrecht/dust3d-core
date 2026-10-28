// SPDX-License-Identifier: MIT
import Foundation
import XCTest

@testable import Dust3DWorkerXPC

private final class HandshakeScalarClock: @unchecked Sendable {
    private let lock = NSLock()
    private var instant: UInt64 = 100
    func read() -> UInt64 { lock.withLock { instant } }
    func set(_ value: UInt64) { lock.withLock { instant = value } }
}

/// Synchronous fake observations only. No real timers, IPC or native operations.
final class WorkerXPCHandshakeCaptureTests: XCTestCase {
    private func capture(_ clock: HandshakeScalarClock) throws -> WorkerXPCHandshakeCapture {
        let budget = try WorkerXPCHandshakeBudget(milliseconds: 2)
        return WorkerXPCHandshakeCapture(
            token: 7, generation: 9,
            deadline: try WorkerXPCHandshakeDeadline(budget: budget, start: 100), clock: { clock.read() })
    }
    private var fields: WorkerXPCHandshakeReplyFields {
        WorkerXPCHandshakeReplyFields(process: 42, sandbox: true, admission: 0, instance: .init(high: 1, low: 2))
    }
    func testShouldKeepTimelyReplyWhenActorDeliveryOccursAfterTheDeadline() throws {
        let clock = HandshakeScalarClock()
        let pending = try capture(clock)
        clock.set(2_000_099)
        XCTAssertEqual(pending.reply(fields), .wake)
        XCTAssertEqual(pending.reply(fields), .coalesced)
        clock.set(3_000_000)
        XCTAssertEqual(pending.timer(), .coalesced)
        guard case .completed(.success(let value), let connectionFailure) = try XCTUnwrap(pending.take()) else {
            return XCTFail("Timely captured reply was lost to actor delay")
        }
        XCTAssertEqual(value.process, 42)
        XCTAssertNil(connectionFailure)
        XCTAssertEqual(pending.reply(fields), .closed)
        XCTAssertNil(pending.take())
    }
    func testShouldRejectExactEndReplyAndRetainFirstAttemptMetadataAcrossDuplicates() throws {
        let clock = HandshakeScalarClock()
        let pending = try capture(clock)
        clock.set(2_000_100)
        XCTAssertEqual(pending.reply(fields), .wake)
        clock.set(100)
        XCTAssertEqual(pending.reply(fields), .coalesced)
        guard case .completed(.failure(let original), _) = try XCTUnwrap(pending.take()),
            let failure = original as? WorkerXPCHandshakeFailure
        else { return XCTFail("Exact-end reply must fail") }
        XCTAssertEqual(failure.token, 7)
        XCTAssertEqual(failure.generation, 9)
        XCTAssertEqual(failure.budgetMilliseconds, 2)
        XCTAssertEqual(failure.cause, .expired(start: 100, deadline: 2_000_100, observed: 2_000_100))
    }
    func testShouldRetainConnectionLossAfterTimelyReplyWithoutReplacingTheReply() throws {
        let clock = HandshakeScalarClock()
        let pending = try capture(clock)
        XCTAssertEqual(pending.reply(fields), .wake)
        let original = NSError(domain: "FakeHandshake", code: 31)
        XCTAssertEqual(pending.failure(original, connection: true), .coalesced)
        guard case .completed(.success, let failure) = try XCTUnwrap(pending.take()) else {
            return XCTFail("Capture order was replaced")
        }
        XCTAssertEqual((failure as NSError?)?.domain, original.domain)
        XCTAssertEqual((failure as NSError?)?.code, original.code)
    }
    func testShouldRescheduleOnlyRemainingTimeAndDetectClockReversalAfterAnEarlyWake() throws {
        let clock = HandshakeScalarClock()
        let pending = try capture(clock)
        clock.set(1_000_100)
        XCTAssertEqual(pending.timer(), .wake)
        guard case .waiting(let remaining) = try XCTUnwrap(pending.take()) else { return XCTFail("Early wake expired") }
        XCTAssertEqual(remaining, 1)
        clock.set(1_000_099)
        XCTAssertEqual(pending.timer(), .wake)
        guard case .completed(.failure(let original), _) = try XCTUnwrap(pending.take()),
            let failure = original as? WorkerXPCHandshakeFailure
        else { return XCTFail("Reversed clock was accepted") }
        XCTAssertEqual(failure.cause, .clockReversed(previous: 1_000_100, observed: 1_000_099))
    }
    func testShouldKeepTimelyOriginalRemoteFailureDespiteLaterActorDelay() throws {
        let clock = HandshakeScalarClock()
        let pending = try capture(clock)
        let original = NSError(domain: "OriginalHandshake", code: 17)
        XCTAssertEqual(pending.failure(original, connection: false), .wake)
        XCTAssertEqual(pending.reply(fields), .coalesced)
        clock.set(3_000_000)
        guard case .completed(.failure(let retained), let lostConnection) = try XCTUnwrap(pending.take()) else {
            return XCTFail("Timely original failure was replaced")
        }
        XCTAssertEqual((retained as NSError).domain, original.domain)
        XCTAssertEqual((retained as NSError).code, original.code)
        XCTAssertNil(lostConnection)
    }
    func testShouldExpireAnEarlyWakeDelayedPastTheOriginalEndWithoutExtendingIt() throws {
        let clock = HandshakeScalarClock()
        let pending = try capture(clock)
        XCTAssertEqual(pending.timer(), .wake)
        XCTAssertEqual(pending.timer(), .coalesced)
        clock.set(2_000_200)
        guard case .completed(.failure(let original), _) = try XCTUnwrap(pending.take()),
            let failure = original as? WorkerXPCHandshakeFailure
        else { return XCTFail("Delayed early wake moved the deadline") }
        XCTAssertEqual(failure.cause, .expired(start: 100, deadline: 2_000_100, observed: 2_000_200))
    }
    func testShouldPreserveOriginalRemoteCauseWhenClockExpiryIsAlsoObserved() throws {
        let clock = HandshakeScalarClock()
        let pending = try capture(clock)
        clock.set(2_000_101)
        let original = NSError(domain: "FakeHandshake", code: 19)
        XCTAssertEqual(pending.failure(original, connection: false), .wake)
        guard case .completed(.failure(let cause), _) = try XCTUnwrap(pending.take()),
            let failure = cause as? WorkerXPCHandshakeFailure
        else { return XCTFail("Expiry metadata missing") }
        XCTAssertEqual((failure.originalTransportCause as NSError?)?.domain, original.domain)
        XCTAssertEqual((failure.originalTransportCause as NSError?)?.code, original.code)
        pending.close()
        XCTAssertEqual(pending.timer(), .closed)
    }
}
