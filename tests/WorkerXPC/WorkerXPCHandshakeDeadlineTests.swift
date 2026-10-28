// SPDX-License-Identifier: MIT
import XCTest

@testable import Dust3DWorkerXPC

/// Pure scalar policy only: no clock reads, timers, IPC, signals or native work.
final class WorkerXPCHandshakeDeadlineTests: XCTestCase {
    func testShouldRequireAnExplicitPositiveBoundedBudgetAndPreserveInvalidValue() throws {
        for milliseconds: UInt64 in [1, 10000] {
            XCTAssertEqual(try WorkerXPCHandshakeBudget(milliseconds: milliseconds).milliseconds, milliseconds)
        }
        for milliseconds: UInt64 in [0, 10001, UInt64.max] {
            XCTAssertThrowsError(try WorkerXPCHandshakeBudget(milliseconds: milliseconds)) { cause in
                XCTAssertEqual(cause as? WorkerXPCHandshakeDeadlineError, .invalidBudget(milliseconds: milliseconds))
            }
        }
    }
    func testShouldRoundOnlyRemainingTimeWithoutMovingTheOriginalDeadline() throws {
        let start: UInt64 = 1_000_000_000
        let deadline = try WorkerXPCHandshakeDeadline(
            budget: WorkerXPCHandshakeBudget(milliseconds: 5), start: start)
        XCTAssertEqual(deadline.start, start)
        XCTAssertEqual(deadline.end, start + 5_000_000)
        XCTAssertEqual(try deadline.remainingMilliseconds(at: start), 5)
        XCTAssertEqual(try deadline.remainingMilliseconds(at: start + 1), 5)
        XCTAssertEqual(try deadline.remainingMilliseconds(at: start + 2_000_000), 3)
        XCTAssertEqual(try deadline.remainingMilliseconds(at: start + 4_000_001), 1)
        XCTAssertEqual(deadline.end, start + 5_000_000)
        for observed in [deadline.end, deadline.end + 1] {
            XCTAssertThrowsError(try deadline.remainingMilliseconds(at: observed)) { cause in
                XCTAssertEqual(
                    cause as? WorkerXPCHandshakeDeadlineError,
                    .expired(start: start, deadline: deadline.end, observed: observed))
            }
        }
    }
    func testShouldRejectReversedAndOverflowingClockValuesWithOriginalMetadata() throws {
        let deadline = try WorkerXPCHandshakeDeadline(
            budget: WorkerXPCHandshakeBudget(milliseconds: 1), start: 100)
        XCTAssertThrowsError(try deadline.remainingMilliseconds(at: 99)) { cause in
            XCTAssertEqual(cause as? WorkerXPCHandshakeDeadlineError, .clockReversed(previous: 100, observed: 99))
        }
        let overflowingStart = UInt64.max - 1000
        XCTAssertThrowsError(
            try WorkerXPCHandshakeDeadline(
                budget: WorkerXPCHandshakeBudget(milliseconds: 1), start: overflowingStart)
        ) { cause in
            XCTAssertEqual(
                cause as? WorkerXPCHandshakeDeadlineError,
                .clockOverflow(start: overflowingStart, milliseconds: 1))
        }
    }
}
