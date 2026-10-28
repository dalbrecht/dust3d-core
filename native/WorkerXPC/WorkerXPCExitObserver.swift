// SPDX-License-Identifier: MIT
import Foundation

protocol WorkerXPCObservationCancellation: Sendable { func cancel() }
struct WorkerXPCExitCallbacks: Sendable {
    let registered: @Sendable () -> Void
    let exited: @Sendable () -> Void
    let failed: @Sendable (UInt) -> Void
}
struct WorkerXPCObservationTimeout: Sendable { let fire: @Sendable () -> Void }

/// Explicit bounded observation policy, independent of native work and process termination.
public struct WorkerXPCExitObservationBudget: Sendable {
    public let milliseconds: UInt64
    /// - Throws: invalidBudget unless milliseconds is in 1...10000.
    public init(milliseconds: UInt64) throws {
        guard milliseconds > 0, milliseconds <= 10000 else { throw WorkerXPCExitObservationError.invalidBudget }
        self.milliseconds = milliseconds
    }
}

public enum WorkerXPCExitObservationPhase: Sendable { case registration, exit }
public enum WorkerXPCExitObservationError: Error {
    case invalidBudget, busy, notRegistered
    case invalidPeer(WorkerXPCHandshakeReceipt)
    case clockReversed(start: UInt64, observed: UInt64, peer: WorkerXPCHandshakeReceipt)
    case clockOverflow(start: UInt64, milliseconds: UInt64, peer: WorkerXPCHandshakeReceipt)
    case exitedDuringRegistration(WorkerXPCHandshakeReceipt)
    case unexpectedEvent(flags: UInt, peer: WorkerXPCHandshakeReceipt)
    case timeout(phase: WorkerXPCExitObservationPhase, peer: WorkerXPCHandshakeReceipt)
}
/// An event from the registered process source, not waitpid status, native drain or publication permission.
/// Caller must bracket source registration with fresh matching authenticated incarnation handshakes.
public struct WorkerXPCExitObservation: Sendable {
    public let peer: WorkerXPCHandshakeReceipt
}

/// One bounded registration/exit waiter. Cancelling an observer never signals or stops a process.
/// The post-registration authenticated handshake barrier remains the executor's responsibility.
public actor WorkerXPCExitObserver {
    typealias Clock = @Sendable () -> UInt64
    typealias Events = @Sendable (Int32, WorkerXPCExitCallbacks) -> any WorkerXPCObservationCancellation
    typealias Timer = @Sendable (UInt64, WorkerXPCObservationTimeout) -> any WorkerXPCObservationCancellation
    private enum State {
        case idle, registering, registered, waiting, exited
        case retired(any Error)
    }
    private struct Deadline {
        let start: UInt64
        let end: UInt64
        let phase: WorkerXPCExitObservationPhase
    }
    private var deadline: Deadline?
    private var registrationStart: UInt64 = 0
    private let peer: WorkerXPCHandshakeReceipt
    private let events: Events
    private let timer: Timer
    private let clock: Clock
    private var state: State = .idle
    private var source: (any WorkerXPCObservationCancellation)?
    private var alarm: (any WorkerXPCObservationCancellation)?
    private var registration: CheckedContinuation<Void, any Error>?
    private var exit: CheckedContinuation<WorkerXPCExitObservation, any Error>?

    init(peer: WorkerXPCHandshakeReceipt, events: @escaping Events, timer: @escaping Timer, clock: @escaping Clock)
        throws
    {
        guard peer.observedProcess > 0, peer.process == peer.observedProcess,
            peer.connectionGeneration > 0, peer.instance.isValid,
            peer.sandboxEntitlementVerified, peer.admissionStatus == 0
        else { throw WorkerXPCExitObservationError.invalidPeer(peer) }
        self.peer = peer
        self.events = events
        self.timer = timer
        self.clock = clock
    }
    deinit {
        source?.cancel()
        alarm?.cancel()
    }
    private func validate(_ milliseconds: UInt64) throws {
        _ = try WorkerXPCExitObservationBudget(milliseconds: milliseconds)
    }
    private func makeDeadline(_ milliseconds: UInt64, phase: WorkerXPCExitObservationPhase) throws -> Deadline {
        let budget = try WorkerXPCExitObservationBudget(milliseconds: milliseconds)
        let start = clock()
        let end = start.addingReportingOverflow(budget.milliseconds * 1_000_000)
        guard !end.overflow else {
            throw WorkerXPCExitObservationError.clockOverflow(start: start, milliseconds: milliseconds, peer: peer)
        }
        return Deadline(start: start, end: end.partialValue, phase: phase)
    }
    private func accepts(_ observed: UInt64, since start: UInt64) -> Bool {
        guard observed >= start else {
            fail(WorkerXPCExitObservationError.clockReversed(start: start, observed: observed, peer: peer))
            return false
        }
        if let deadline, observed >= deadline.end {
            fail(WorkerXPCExitObservationError.timeout(phase: deadline.phase, peer: peer))
            return false
        }
        return true
    }
    private func scheduleTimer(for deadline: Deadline, at observed: UInt64) {
        guard accepts(observed, since: deadline.start) else { return }
        // The validated budget bounds this difference and rounding addition; never extend the original end.
        let milliseconds = (deadline.end - observed + 999999) / 1_000_000
        let readClock = clock
        let phase = deadline.phase
        cancelAlarm()
        alarm = timer(
            milliseconds,
            WorkerXPCObservationTimeout(fire: { [weak self] in
                let instant = readClock()
                Task { await self?.expire(phase, at: instant) }
            }))
    }
    /// Arm the event source while the authenticated peer is alive. No job channels may exist yet.
    /// - Parameter timeoutMilliseconds: Explicit bounded registration budget, 1...10000ms.
    /// - Throws: Original typed phase/peer failure. Expiry never proves process disappearance.
    /// Captured monotonic event times must precede the checked original deadline, including delayed timer delivery.
    /// Repeat after registration is idempotent; overlapping waiters are rejected.
    public func register(timeoutMilliseconds: UInt64) async throws {
        try validate(timeoutMilliseconds)
        switch state {
        case .registered, .waiting, .exited: return
        case .registering: throw WorkerXPCExitObservationError.busy
        case .retired(let cause): throw cause
        case .idle: break
        }
        let captured = try makeDeadline(timeoutMilliseconds, phase: .registration)
        deadline = captured
        registrationStart = captured.start
        let readClock = clock
        state = .registering
        try await withCheckedThrowingContinuation { continuation in
            registration = continuation
            source = events(
                peer.observedProcess,
                WorkerXPCExitCallbacks(
                    registered: { [weak self] in
                        let instant = readClock()
                        Task { await self?.didRegister(at: instant) }
                    },
                    exited: { [weak self] in
                        let instant = readClock()
                        Task { await self?.didExit(at: instant) }
                    },
                    failed: { [weak self] flags in Task { await self?.unexpected(flags) } }))
            scheduleTimer(for: captured, at: clock())
        }
    }
    /// Await the registered process's exit event after normal owner invalidation and owned phase joins.
    /// - Parameter timeoutMilliseconds: Explicit bounded observation budget, 1...10000ms.
    /// - Returns: Original peer plus registered-source event; caller supplies handshake-barrier correlation.
    /// - Throws: Phase-specific expiry or invalid lifecycle. Expired observers never retry automatically.
    /// Queued exit events captured before this wait starts remain eligible; late events cannot win against late timers.
    /// Scheduling is not real-time; this never promises bounded native work or OS termination.
    public func waitForExit(timeoutMilliseconds: UInt64) async throws -> WorkerXPCExitObservation {
        try validate(timeoutMilliseconds)
        switch state {
        case .exited: return WorkerXPCExitObservation(peer: peer)
        case .retired(let cause): throw cause
        case .idle: throw WorkerXPCExitObservationError.notRegistered
        case .registering, .waiting: throw WorkerXPCExitObservationError.busy
        case .registered: break
        }
        let captured = try makeDeadline(timeoutMilliseconds, phase: .exit)
        deadline = captured
        state = .waiting
        return try await withCheckedThrowingContinuation { continuation in
            exit = continuation
            scheduleTimer(for: captured, at: clock())
        }
    }
    private func cancelAlarm() {
        alarm?.cancel()
        alarm = nil
    }
    private func cancelSource() {
        source?.cancel()
        source = nil
    }
    private func didRegister(at observed: UInt64) {
        guard case .registering = state else { return }
        guard accepts(observed, since: registrationStart) else { return }
        state = .registered
        deadline = nil
        cancelAlarm()
        let pending = registration
        registration = nil
        pending?.resume()
    }
    private func didExit(at observed: UInt64) {
        switch state {
        case .registering:
            guard accepts(observed, since: registrationStart) else { return }
            fail(WorkerXPCExitObservationError.exitedDuringRegistration(peer))
        case .registered, .waiting:
            // An event captured before the exit wait began can already be pending on this actor.
            guard accepts(observed, since: registrationStart) else { return }
            state = .exited
            deadline = nil
            cancelAlarm()
            cancelSource()
            let pending = exit
            exit = nil
            pending?.resume(returning: WorkerXPCExitObservation(peer: peer))
        case .idle, .exited, .retired: break
        }
    }
    private func unexpected(_ flags: UInt) {
        switch state {
        case .registering, .registered, .waiting:
            fail(WorkerXPCExitObservationError.unexpectedEvent(flags: flags, peer: peer))
        case .idle, .exited, .retired: break
        }
    }
    private func expire(_ phase: WorkerXPCExitObservationPhase, at observed: UInt64) {
        switch (phase, state) {
        case (.registration, .registering), (.exit, .waiting):
            guard let deadline else { return }
            scheduleTimer(for: deadline, at: observed)
        default: break
        }
    }
    private func fail(_ failure: any Error) {
        state = .retired(failure)
        deadline = nil
        cancelAlarm()
        cancelSource()
        let pendingRegistration = registration
        let pendingExit = exit
        registration = nil
        exit = nil
        pendingRegistration?.resume(throwing: failure)
        pendingExit?.resume(throwing: failure)
    }
}
