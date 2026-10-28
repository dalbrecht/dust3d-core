// SPDX-License-Identifier: MIT
import Foundation

struct WorkerXPCHandshakeWakeup: Sendable { let perform: @Sendable () async -> Void }
struct WorkerXPCHandshakeTimeout: Sendable { let fire: @Sendable () -> Void }
struct WorkerXPCHandshakeTiming: Sendable {
    let clock: @Sendable () -> UInt64
    let timer: @Sendable (UInt64, WorkerXPCHandshakeTimeout) -> any WorkerXPCObservationCancellation
    let delivery: @Sendable (WorkerXPCHandshakeWakeup) -> Void
    static func system() -> Self {
        let queue = DispatchQueue(label: "org.dust3d.worker.handshake-deadline", qos: .utility)
        return Self(
            clock: { DispatchTime.now().uptimeNanoseconds },
            timer: { milliseconds, timeout in
                let source = DispatchSource.makeTimerSource(queue: queue)
                // The validated handshake budget bounds conversion to Int and every remaining interval.
                source.schedule(deadline: .now() + .milliseconds(Int(milliseconds)))
                source.setEventHandler(handler: timeout.fire)
                source.resume()
                return WorkerXPCDispatchObservation(source)
            }, delivery: { wakeup in Task { await wakeup.perform() } })
    }
}

struct WorkerXPCHandshakeConnectionLoss: Sendable {
    let capture: WorkerXPCHandshakeCapture?
    let cause: any Error
}
/// This lock publishes the current capture to Foundation callbacks and coalesces lifetime loss once.
/// A global loss must still reach the actor when the captured handshake has already completed.
final class WorkerXPCHandshakeSlot: @unchecked Sendable {
    private let lock = NSLock()
    private var current: WorkerXPCHandshakeCapture?
    private var firstLoss: (any Error)?
    func install(_ capture: WorkerXPCHandshakeCapture) -> (any Error)? {
        lock.withLock {
            if let firstLoss { return firstLoss }
            current = capture
            return nil
        }
    }
    func clear(_ capture: WorkerXPCHandshakeCapture) {
        lock.withLock { if current === capture { current = nil } }
    }
    func connectionLost(_ cause: any Error) -> WorkerXPCHandshakeConnectionLoss? {
        lock.withLock {
            guard firstLoss == nil else { return nil }
            firstLoss = cause
            _ = current?.failure(cause, connection: true)
            return WorkerXPCHandshakeConnectionLoss(capture: current, cause: cause)
        }
    }
}
