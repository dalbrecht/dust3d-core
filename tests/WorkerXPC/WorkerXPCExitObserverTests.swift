// SPDX-License-Identifier: MIT
import Foundation
import XCTest

@testable import Dust3DWorkerXPC

private final class ObservationToken: WorkerXPCObservationCancellation, @unchecked Sendable {
    private let lock = NSLock()
    private var count = 0
    func cancel() { lock.withLock { count += 1 } }
    var cancellations: Int { lock.withLock { count } }
}
/// Synthetic callbacks only: no process registration, signals, clock sleeps or native work.
private final class ObservationDriver: @unchecked Sendable {
    private let lock = NSLock()
    private var callbacks: WorkerXPCExitCallbacks?
    private var timers: [WorkerXPCObservationTimeout] = []
    private var deadlines: [UInt64] = []
    private var instant: UInt64 = 0
    var now: UInt64 { lock.withLock { instant } }
    func setTime(_ nanoseconds: UInt64) { lock.withLock { instant = nanoseconds } }
    private var waiter: (Int, CheckedContinuation<Void, Never>)?
    let source = ObservationToken()
    func start(_ process: Int32, _ callbacks: WorkerXPCExitCallbacks) -> any WorkerXPCObservationCancellation {
        lock.withLock { self.callbacks = callbacks }
        return source
    }
    func timer(_ milliseconds: UInt64, _ timeout: WorkerXPCObservationTimeout) -> any WorkerXPCObservationCancellation {
        let pending = lock.withLock { () -> CheckedContinuation<Void, Never>? in
            timers.append(timeout)
            let end = instant.addingReportingOverflow(milliseconds * 1_000_000)
            deadlines.append(end.overflow ? UInt64.max : end.partialValue)
            guard let waiting = waiter, timers.count >= waiting.0 else { return nil }
            waiter = nil
            return waiting.1
        }
        pending?.resume()
        return ObservationToken()
    }
    func waitForTimers(_ count: Int) async {
        await withCheckedContinuation { continuation in
            let ready = lock.withLock {
                if timers.count >= count { return true }
                waiter = (count, continuation)
                return false
            }
            if ready { continuation.resume() }
        }
    }
    func registered() { lock.withLock { callbacks }?.registered() }
    func exited() { lock.withLock { callbacks }?.exited() }
    func fail(_ flags: UInt) { lock.withLock { callbacks }?.failed(flags) }
    func expire(_ index: Int) {
        let timeout = lock.withLock {
            instant = max(instant, deadlines[index])
            return timers[index]
        }
        timeout.fire()
    }
    func fireEarly(_ index: Int) { lock.withLock { timers[index] }.fire() }
    var timerCount: Int { lock.withLock { timers.count } }
    func deadline(_ index: Int) -> UInt64 { lock.withLock { deadlines[index] } }
}
@MainActor
final class WorkerXPCExitObserverTests: XCTestCase {
    private func observer(_ driver: ObservationDriver) throws -> WorkerXPCExitObserver {
        try WorkerXPCExitObserver(
            peer: WorkerXPCHandshakeReceipt(
                instance: WorkerXPCServiceInstance(high: 7, low: 11), process: 42,
                sandboxEntitlementVerified: true, admissionStatus: 0, connectionGeneration: 9, observedProcess: 42),
            events: { driver.start($0, $1) }, timer: { driver.timer($0, $1) }, clock: { driver.now })
    }
    func testShouldSeparateRegistrationFromExitAndIgnoreEarlierTimer() async throws {
        let driver = ObservationDriver()
        let observer = try observer(driver)
        let registration = Task { try await observer.register(timeoutMilliseconds: 50) }
        await driver.waitForTimers(1)
        driver.registered()
        try await registration.value
        let waiting = Task { try await observer.waitForExit(timeoutMilliseconds: 75) }
        await driver.waitForTimers(2)
        driver.expire(0)
        driver.exited()
        let observation = try await waiting.value
        XCTAssertEqual(observation.peer.observedProcess, 42)
        XCTAssertEqual(observation.peer.instance.low, 11)
        let repeated = try await observer.waitForExit(timeoutMilliseconds: 75)
        XCTAssertEqual(repeated.peer.instance, observation.peer.instance)
        XCTAssertEqual(driver.timerCount, 2)
        XCTAssertEqual(driver.source.cancellations, 1)
    }
    func testShouldReportExitTimeoutWithoutClaimingProcessDisappearance() async throws {
        let driver = ObservationDriver()
        let observer = try observer(driver)
        let registration = Task { try await observer.register(timeoutMilliseconds: 50) }
        await driver.waitForTimers(1)
        driver.registered()
        try await registration.value
        let waiting = Task { try await observer.waitForExit(timeoutMilliseconds: 75) }
        await driver.waitForTimers(2)
        driver.expire(1)
        do {
            _ = try await waiting.value
            XCTFail("Expiry must not produce exit evidence")
        } catch WorkerXPCExitObservationError.timeout(let phase, let peer) {
            XCTAssertEqual(phase, .exit)
            XCTAssertEqual(peer.observedProcess, 42)
        }
        do {
            _ = try await observer.waitForExit(timeoutMilliseconds: 75)
            XCTFail("Expired observation must not retry")
        } catch WorkerXPCExitObservationError.timeout(let phase, _) {
            XCTAssertEqual(phase, .exit)
        }
        XCTAssertEqual(driver.timerCount, 2)
        XCTAssertEqual(driver.source.cancellations, 1)
    }
    func testShouldRejectInvalidBudgetBeforeRegisteringAnyProcess() async throws {
        let driver = ObservationDriver()
        let observer = try observer(driver)
        for budget: UInt64 in [0, 10001, UInt64.max] {
            do {
                try await observer.register(timeoutMilliseconds: budget)
                XCTFail("Invalid budget must not register")
            } catch WorkerXPCExitObservationError.invalidBudget {
            }
        }
        XCTAssertEqual(driver.timerCount, 0)
        XCTAssertEqual(driver.source.cancellations, 0)
    }
    func testShouldSealRegistrationExpiryAndIgnoreLateCallbacks() async throws {
        let driver = ObservationDriver()
        let observer = try observer(driver)
        let registration = Task { try await observer.register(timeoutMilliseconds: 50) }
        await driver.waitForTimers(1)
        driver.expire(0)
        do {
            try await registration.value
            XCTFail("Registration expiry cannot authenticate a process source")
        } catch WorkerXPCExitObservationError.timeout(let phase, let peer) {
            XCTAssertEqual(phase, .registration)
            XCTAssertEqual(peer.instance.high, 7)
        }
        driver.registered()
        driver.exited()
        do {
            _ = try await observer.waitForExit(timeoutMilliseconds: 50)
            XCTFail("Late callbacks cannot reopen a retired observer")
        } catch WorkerXPCExitObservationError.timeout(let phase, _) {
            XCTAssertEqual(phase, .registration)
        }
        XCTAssertEqual(driver.source.cancellations, 1)
        XCTAssertEqual(driver.timerCount, 1)
    }
    func testShouldRetainUnexpectedEventMetadataWithoutFabricatingExit() async throws {
        let driver = ObservationDriver()
        let observer = try observer(driver)
        let registration = Task { try await observer.register(timeoutMilliseconds: 50) }
        await driver.waitForTimers(1)
        driver.registered()
        try await registration.value
        let waiting = Task { try await observer.waitForExit(timeoutMilliseconds: 75) }
        await driver.waitForTimers(2)
        driver.fail(UInt.max)
        do {
            _ = try await waiting.value
            XCTFail("An unexpected event is not exit evidence")
        } catch WorkerXPCExitObservationError.unexpectedEvent(let flags, let peer) {
            XCTAssertEqual(flags, UInt.max)
            XCTAssertEqual(peer.observedProcess, 42)
        }
        do {
            _ = try await observer.waitForExit(timeoutMilliseconds: 75)
            XCTFail("The original observation failure must remain available")
        } catch WorkerXPCExitObservationError.unexpectedEvent(let flags, _) {
            XCTAssertEqual(flags, UInt.max)
        }
        XCTAssertEqual(driver.source.cancellations, 1)
    }

    func testShouldBoundConcurrentWaitersAndAcceptBudgetEndpoints() async throws {
        let driver = ObservationDriver()
        let observer = try observer(driver)
        do {
            _ = try await observer.waitForExit(timeoutMilliseconds: 1)
            XCTFail("Unregistered sources cannot emit exit observations")
        } catch WorkerXPCExitObservationError.notRegistered {
        }
        let registration = Task { try await observer.register(timeoutMilliseconds: 1) }
        await driver.waitForTimers(1)
        do {
            try await observer.register(timeoutMilliseconds: 1)
            XCTFail("Only one registration waiter may be retained")
        } catch WorkerXPCExitObservationError.busy {
        }
        do {
            _ = try await observer.waitForExit(timeoutMilliseconds: 10000)
            XCTFail("Exit cannot wait on unfinished registration")
        } catch WorkerXPCExitObservationError.busy {
        }
        driver.registered()
        try await registration.value
        let waiting = Task { try await observer.waitForExit(timeoutMilliseconds: 10000) }
        await driver.waitForTimers(2)
        try await observer.register(timeoutMilliseconds: 1)
        do {
            _ = try await observer.waitForExit(timeoutMilliseconds: 10000)
            XCTFail("Only one exit waiter may be retained")
        } catch WorkerXPCExitObservationError.busy {
        }
        driver.exited()
        _ = try await waiting.value
        XCTAssertEqual(driver.timerCount, 2)
        XCTAssertEqual(driver.source.cancellations, 1)
    }
    func testShouldRejectExitDuringRegistrationAndRetainOriginalCause() async throws {
        let driver = ObservationDriver()
        let observer = try observer(driver)
        let registration = Task { try await observer.register(timeoutMilliseconds: 50) }
        await driver.waitForTimers(1)
        driver.exited()
        do {
            try await registration.value
            XCTFail("A pre-barrier event cannot establish process identity")
        } catch WorkerXPCExitObservationError.exitedDuringRegistration(let peer) {
            XCTAssertEqual(peer.instance.high, 7)
        }
        driver.registered()
        do {
            _ = try await observer.waitForExit(timeoutMilliseconds: 50)
            XCTFail("Late registration must not erase the original cause")
        } catch WorkerXPCExitObservationError.exitedDuringRegistration(let peer) {
            XCTAssertEqual(peer.observedProcess, 42)
        }
        XCTAssertEqual(driver.source.cancellations, 1)
    }
    func testShouldRejectUntrustedScalarPeerBeforeSourceFactory() throws {
        let driver = ObservationDriver()
        func peer(
            instance: WorkerXPCServiceInstance = WorkerXPCServiceInstance(high: 1, low: 1),
            process: Int32 = 42, observed: Int32 = 42, generation: UInt64 = 9,
            sandbox: Bool = true, admission: UInt32 = 0
        ) -> WorkerXPCHandshakeReceipt {
            WorkerXPCHandshakeReceipt(
                instance: instance, process: process, sandboxEntitlementVerified: sandbox,
                admissionStatus: admission, connectionGeneration: generation, observedProcess: observed)
        }
        for invalid in [
            peer(instance: WorkerXPCServiceInstance(high: 0, low: 0)), peer(observed: 0),
            peer(process: 43), peer(generation: 0), peer(sandbox: false), peer(admission: 4),
        ] {
            do {
                _ = try WorkerXPCExitObserver(
                    peer: invalid, events: { driver.start($0, $1) }, timer: { driver.timer($0, $1) },
                    clock: { driver.now })
                XCTFail("Untrusted scalar identity must not construct an observer")
            } catch WorkerXPCExitObservationError.invalidPeer(let original) {
                XCTAssertEqual(original.instance, invalid.instance)
                XCTAssertEqual(original.observedProcess, invalid.observedProcess)
                XCTAssertEqual(original.admissionStatus, invalid.admissionStatus)
            }
        }
        XCTAssertEqual(driver.timerCount, 0)
        XCTAssertEqual(driver.source.cancellations, 0)
    }

    func testShouldRejectRegistrationAtDeadlineWithoutWaitingForTimerDelivery() async throws {
        let driver = ObservationDriver()
        let observer = try observer(driver)
        let registration = Task { try await observer.register(timeoutMilliseconds: 50) }
        await driver.waitForTimers(1)
        driver.setTime(50_000_000)
        driver.registered()
        do {
            try await registration.value
            XCTFail("A late timer cannot make a late registration timely")
        } catch WorkerXPCExitObservationError.timeout(let phase, _) {
            XCTAssertEqual(phase, .registration)
        }
    }
    func testShouldRejectExitAtDeadlineWithoutWaitingForTimerDelivery() async throws {
        let driver = ObservationDriver()
        let observer = try observer(driver)
        let registration = Task { try await observer.register(timeoutMilliseconds: 50) }
        await driver.waitForTimers(1)
        driver.registered()
        try await registration.value
        let waiting = Task { try await observer.waitForExit(timeoutMilliseconds: 75) }
        await driver.waitForTimers(2)
        driver.setTime(75_000_000)
        driver.exited()
        do {
            _ = try await waiting.value
            XCTFail("A late exit must not win against delayed timer delivery")
        } catch WorkerXPCExitObservationError.timeout(let phase, _) {
            XCTAssertEqual(phase, .exit)
        }
    }
    func testShouldAcceptEventsJustBeforeTheirOwnMonotonicDeadlines() async throws {
        let driver = ObservationDriver()
        let observer = try observer(driver)
        let registration = Task { try await observer.register(timeoutMilliseconds: 50) }
        await driver.waitForTimers(1)
        driver.setTime(49_999_999)
        driver.registered()
        try await registration.value
        let waiting = Task { try await observer.waitForExit(timeoutMilliseconds: 75) }
        await driver.waitForTimers(2)
        driver.setTime(124_999_998)
        driver.exited()
        let observed = try await waiting.value
        XCTAssertEqual(observed.peer.observedProcess, 42)
    }
    func testShouldPreserveBackwardClockAndOverflowCausesWithoutNativeWork() async throws {
        let driver = ObservationDriver()
        driver.setTime(10_000_000)
        let observer = try observer(driver)
        let registration = Task { try await observer.register(timeoutMilliseconds: 50) }
        await driver.waitForTimers(1)
        driver.setTime(9_000_000)
        driver.registered()
        do {
            try await registration.value
            XCTFail("A reversed clock cannot authenticate registration")
        } catch WorkerXPCExitObservationError.clockReversed(let start, let observed, _) {
            XCTAssertEqual(start, 10_000_000)
            XCTAssertEqual(observed, 9_000_000)
        }
        let overflow = ObservationDriver()
        overflow.setTime(UInt64.max)
        let invalid = try self.observer(overflow)
        do {
            try await invalid.register(timeoutMilliseconds: 1)
            XCTFail("Checked deadline overflow must precede source creation")
        } catch WorkerXPCExitObservationError.clockOverflow(let start, let milliseconds, _) {
            XCTAssertEqual(start, UInt64.max)
            XCTAssertEqual(milliseconds, 1)
        }
        XCTAssertEqual(overflow.timerCount, 0)
    }

    func testShouldRescheduleEarlyTimerWithoutExtendingOriginalDeadline() async throws {
        let driver = ObservationDriver()
        let observer = try observer(driver)
        let registration = Task { try await observer.register(timeoutMilliseconds: 50) }
        await driver.waitForTimers(1)
        driver.setTime(25_000_000)
        driver.fireEarly(0)
        await driver.waitForTimers(2)
        XCTAssertEqual(driver.deadline(1), 50_000_000)
        driver.setTime(50_000_000)
        driver.registered()
        do {
            try await registration.value
            XCTFail("Early timer rescheduling must not extend registration acceptance")
        } catch WorkerXPCExitObservationError.timeout(let phase, _) {
            XCTAssertEqual(phase, .registration)
        }
    }
    func testShouldAcceptAnExitCapturedBeforeTheWaitStarts() async throws {
        let driver = ObservationDriver()
        let observer = try observer(driver)
        let registration = Task { try await observer.register(timeoutMilliseconds: 50) }
        await driver.waitForTimers(1)
        driver.setTime(10_000_000)
        driver.registered()
        try await registration.value
        driver.setTime(11_000_000)
        driver.exited()
        driver.setTime(20_000_000)
        // The queued event can reach the actor before or after this new wait begins.
        let observed = try await observer.waitForExit(timeoutMilliseconds: 50)
        XCTAssertEqual(observed.peer.observedProcess, 42)
    }

}
