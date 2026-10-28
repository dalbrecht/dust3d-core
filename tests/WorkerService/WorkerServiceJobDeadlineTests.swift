// SPDX-License-Identifier: MIT
import XCTest

@testable import Dust3DWorkerService

/// Scalar observations only; no timers, native work or process-exit calls.
final class WorkerServiceJobDeadlineTests: XCTestCase {
    func testShouldRequireExplicitPositiveBoundedMilliseconds() throws {
        for invalid: UInt64 in [0, 3_600_001, UInt64.max] {
            XCTAssertThrowsError(try WorkerServiceJobBudget(milliseconds: invalid)) { cause in
                XCTAssertEqual(cause as? WorkerServiceJobDeadlineError, .invalidBudget(milliseconds: invalid))
            }
        }
        for valid: UInt64 in [1, 300_000, 3_600_000] {
            XCTAssertEqual(try WorkerServiceJobBudget(milliseconds: valid).milliseconds, valid)
        }
    }
    func testShouldKeepOriginalEndAcrossEarlyObservationsAndRejectExactEnd() throws {
        let deadline = try WorkerServiceJobDeadline(budget: WorkerServiceJobBudget(milliseconds: 2), start: 100)
        XCTAssertEqual(try deadline.remainingMilliseconds(at: 100), 2)
        XCTAssertEqual(try deadline.remainingMilliseconds(at: 1_000_101), 1)
        XCTAssertEqual(try deadline.remainingMilliseconds(at: 2_000_099), 1)
        for late: UInt64 in [2_000_100, 2_000_101] {
            XCTAssertThrowsError(try deadline.remainingMilliseconds(at: late)) { cause in
                XCTAssertEqual(
                    cause as? WorkerServiceJobDeadlineError,
                    .expired(start: 100, deadline: 2_000_100, observed: late))
            }
        }
    }
    func testShouldPreserveCheckedClockOverflowAndReversalWithoutSystemCalls() throws {
        let budget = try WorkerServiceJobBudget(milliseconds: 1)
        XCTAssertThrowsError(try WorkerServiceJobDeadline(budget: budget, start: UInt64.max - 2)) { cause in
            XCTAssertEqual(
                cause as? WorkerServiceJobDeadlineError,
                .clockOverflow(start: UInt64.max - 2, milliseconds: 1))
        }
        let deadline = try WorkerServiceJobDeadline(budget: budget, start: 100)
        XCTAssertThrowsError(try deadline.remainingMilliseconds(at: 99)) { cause in
            XCTAssertEqual(cause as? WorkerServiceJobDeadlineError, .clockReversed(previous: 100, observed: 99))
        }
    }
}
