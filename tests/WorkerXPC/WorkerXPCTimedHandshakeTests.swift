// SPDX-License-Identifier: MIT
import Foundation
import XCTest

@testable import Dust3DWorkerXPC

private struct HandshakeTestCancellation: WorkerXPCObservationCancellation {
    let perform: @Sendable () -> Void
    func cancel() { perform() }
}
private final class HandshakeTimerFacts: @unchecked Sendable {
    private let lock = NSLock()
    private var instant: UInt64 = 100
    private var callback: WorkerXPCHandshakeTimeout?
    private var waits: [UInt64] = []
    private var cancelled = 0
    private var reads = 0
    private var holdsDelivery = false
    private var queued: [WorkerXPCHandshakeWakeup] = []
    private var queuedWaiter: CheckedContinuation<Void, Never>?
    private var alarmWaiter: (Int, CheckedContinuation<Void, Never>)?
    var clockReadCount: Int { lock.withLock { reads } }
    func holdDelivery() { lock.withLock { holdsDelivery = true } }
    func waitUntilDelivery() async {
        await withCheckedContinuation { continuation in
            let ready = lock.withLock {
                if !queued.isEmpty { return true }
                queuedWaiter = continuation
                return false
            }
            if ready { continuation.resume() }
        }
    }
    func deliverQueuedInReverse() async {
        let work = lock.withLock {
            let work = Array(queued.reversed())
            queued.removeAll()
            return work
        }
        for item in work { await item.perform() }
    }
    func waitForAlarmCount(_ count: Int) async {
        await withCheckedContinuation { continuation in
            let ready = lock.withLock {
                if waits.count >= count { return true }
                alarmWaiter = (count, continuation)
                return false
            }
            if ready { continuation.resume() }
        }
    }
    var requested: [UInt64] { lock.withLock { waits } }
    var cancellationCount: Int { lock.withLock { cancelled } }
    func set(_ value: UInt64) { lock.withLock { instant = value } }
    func fire() {
        let timeout = lock.withLock { callback }
        timeout?.fire()
    }
    var timing: WorkerXPCHandshakeTiming {
        WorkerXPCHandshakeTiming(
            clock: {
                self.lock.withLock {
                    self.reads += 1
                    return self.instant
                }
            },
            timer: { milliseconds, timeout in
                let waiter = self.lock.withLock { () -> CheckedContinuation<Void, Never>? in
                    self.waits.append(milliseconds)
                    self.callback = timeout
                    if let waiter = self.alarmWaiter, self.waits.count >= waiter.0 {
                        self.alarmWaiter = nil
                        return waiter.1
                    }
                    return nil
                }
                waiter?.resume()
                return HandshakeTestCancellation(perform: { self.lock.withLock { self.cancelled += 1 } })
            },
            delivery: { wakeup in
                let (held, waiter) = self.lock.withLock { () -> (Bool, CheckedContinuation<Void, Never>?) in
                    guard self.holdsDelivery else { return (false, nil) }
                    self.queued.append(wakeup)
                    let waiter = self.queuedWaiter
                    self.queuedWaiter = nil
                    return (true, waiter)
                }
                waiter?.resume()
                if !held { Task { await wakeup.perform() } }
            })
    }
}

@MainActor
final class WorkerXPCTimedHandshakeTests: XCTestCase {
    func testShouldExpireOwnedRPCAndRetainOriginalFailureAcrossLateReplyAndRetry() async throws {
        let channel = FakeXPCFacts()
        channel.mode = .hold
        let timer = HandshakeTimerFacts()
        let connection = WorkerXPCConnection(factory: { FakeXPCChannel(channel) }, handshakeTiming: timer.timing)
        let pending = Task { try await connection.handshake(generation: 9, timeoutMilliseconds: 2) }
        await channel.waitUntilHeld()
        XCTAssertEqual(timer.requested, [2])
        timer.set(2_000_100)
        timer.fire()
        do {
            _ = try await pending.value
            XCTFail("Expired handshake accepted")
        } catch let failure as WorkerXPCHandshakeFailure {
            XCTAssertEqual(failure.token, 1)
            XCTAssertEqual(failure.generation, 9)
            XCTAssertEqual(failure.cause, .expired(start: 100, deadline: 2_000_100, observed: 2_000_100))
        }
        channel.release()
        do {
            _ = try await connection.handshake(generation: 9, timeoutMilliseconds: 2)
            XCTFail("Expired lifetime reused")
        } catch let failure as WorkerXPCHandshakeFailure {
            XCTAssertEqual(failure.token, 1)
            XCTAssertEqual(failure.cause, .expired(start: 100, deadline: 2_000_100, observed: 2_000_100))
        }
        _ = try await connection.invalidate()
        XCTAssertGreaterThan(timer.cancellationCount, 0)
    }
    func testShouldPreserveTimelyReplyAndRetireGlobalLossWhenDeliveryOrderIsReversed() async throws {
        let channel = FakeXPCFacts()
        let timer = HandshakeTimerFacts()
        timer.holdDelivery()
        let connection = WorkerXPCConnection(factory: { FakeXPCChannel(channel) }, handshakeTiming: timer.timing)
        let pending = Task { try await connection.handshake(generation: 9, timeoutMilliseconds: 2) }
        await timer.waitUntilDelivery()
        timer.set(3_000_000)
        channel.interrupt()
        await timer.deliverQueuedInReverse()
        let receipt = try await pending.value
        XCTAssertEqual(receipt.observedProcess, 42)
        do {
            _ = try await connection.handshake(generation: 9, timeoutMilliseconds: 2)
            XCTFail("Global connection loss was discarded with the completed capture")
        } catch WorkerXPCTransportError.retired {}
    }
    func testShouldRejectOverlapBeforeReadingClockOrDisturbingPendingOwnership() async throws {
        let channel = FakeXPCFacts()
        channel.mode = .hold
        let timer = HandshakeTimerFacts()
        let connection = WorkerXPCConnection(factory: { FakeXPCChannel(channel) }, handshakeTiming: timer.timing)
        let pending = Task { try await connection.handshake(generation: 9, timeoutMilliseconds: 2) }
        await channel.waitUntilHeld()
        let reads = timer.clockReadCount
        timer.set(UInt64.max)
        do {
            _ = try await connection.handshake(generation: 9, timeoutMilliseconds: 2)
            XCTFail("Overlapping handshake accepted")
        } catch WorkerXPCTransportError.busy {}
        XCTAssertEqual(timer.clockReadCount, reads)
        XCTAssertEqual(timer.cancellationCount, 0)
        timer.set(100)
        channel.release()
        let receipt = try await pending.value
        XCTAssertEqual(receipt.observedProcess, 42)
        _ = try await connection.invalidate()
    }
    func testShouldRescheduleEarlyWakeWithoutMovingOriginalDeadline() async throws {
        let channel = FakeXPCFacts()
        channel.mode = .hold
        let timer = HandshakeTimerFacts()
        let connection = WorkerXPCConnection(factory: { FakeXPCChannel(channel) }, handshakeTiming: timer.timing)
        let pending = Task { try await connection.handshake(generation: 9, timeoutMilliseconds: 2) }
        await channel.waitUntilHeld()
        timer.set(1_000_100)
        timer.fire()
        await timer.waitForAlarmCount(2)
        XCTAssertEqual(timer.requested, [2, 1])
        timer.set(2_000_100)
        timer.fire()
        do {
            _ = try await pending.value
            XCTFail("Early wake extended the original deadline")
        } catch let failure as WorkerXPCHandshakeFailure {
            XCTAssertEqual(failure.cause, .expired(start: 100, deadline: 2_000_100, observed: 2_000_100))
        }
        channel.release()
    }
    func testShouldRejectInvalidBudgetWithoutConsumingRPCOwnership() async throws {
        let channel = FakeXPCFacts()
        let timer = HandshakeTimerFacts()
        let connection = WorkerXPCConnection(factory: { FakeXPCChannel(channel) }, handshakeTiming: timer.timing)
        do {
            _ = try await connection.handshake(generation: 9, timeoutMilliseconds: 0)
            XCTFail("Missing explicit budget accepted")
        } catch WorkerXPCHandshakeDeadlineError.invalidBudget(let value) { XCTAssertEqual(value, 0) }
        let receipt = try await connection.handshake(generation: 9, timeoutMilliseconds: 2)
        XCTAssertEqual(receipt.observedProcess, 42)
        _ = try await connection.invalidate()
    }
    func testShouldRejectClockOverflowBeforeTokenAndKeepItsOriginalMetadata() async throws {
        let channel = FakeXPCFacts()
        let timer = HandshakeTimerFacts()
        timer.set(UInt64.max - 10)
        let connection = WorkerXPCConnection(factory: { FakeXPCChannel(channel) }, handshakeTiming: timer.timing)
        do {
            _ = try await connection.handshake(generation: 9, timeoutMilliseconds: 2)
            XCTFail("Wrapped absolute deadline accepted")
        } catch let failure as WorkerXPCHandshakeFailure {
            XCTAssertNil(failure.token)
            XCTAssertEqual(failure.cause, .clockOverflow(start: UInt64.max - 10, milliseconds: 2))
        }
        XCTAssertTrue(timer.requested.isEmpty)
    }
}
