// SPDX-License-Identifier: MIT
import Darwin
import Foundation

/// Dispatch sources are thread-safe. This wrapper has no mutable Swift state; every source is resumed
/// before its owner escapes, so cancelling/releasing cannot leave a suspended source behind.
private final class WorkerXPCDispatchObservation: WorkerXPCObservationCancellation, @unchecked Sendable {
    private let source: any DispatchSourceProtocol
    init(_ source: any DispatchSourceProtocol) { self.source = source }
    func cancel() { source.cancel() }
    deinit { source.cancel() }
}
extension WorkerXPCExitObserver {
    /// Create an unregistered public macOS process-exit observer. No PID is signalled or reaped.
    /// - Parameter peer: Captured authenticated handshake; registration must precede a fresh matching handshake.
    /// - Throws: invalidPeer for inconsistent scalar identity before creating any OS source.
    public init(peer: WorkerXPCHandshakeReceipt) throws {
        let queue = DispatchQueue(label: "org.dust3d.worker.exit-observation", qos: .utility)
        try self.init(
            peer: peer,
            events: { process, callbacks in
                let source = DispatchSource.makeProcessSource(identifier: process, eventMask: .exit, queue: queue)
                source.setRegistrationHandler(handler: callbacks.registered)
                source.setEventHandler { [weak source] in
                    guard let source else { return }
                    let flags = source.data
                    if flags.contains(.exit) { callbacks.exited() } else { callbacks.failed(flags.rawValue) }
                }
                source.resume()
                return WorkerXPCDispatchObservation(source)
            },
            timer: { milliseconds, timeout in
                let source = DispatchSource.makeTimerSource(queue: queue)
                source.schedule(deadline: .now() + .milliseconds(Int(milliseconds)))
                source.setEventHandler(handler: timeout.fire)
                source.resume()
                return WorkerXPCDispatchObservation(source)
            }, clock: { DispatchTime.now().uptimeNanoseconds }, delivery: { wakeup in Task { await wakeup.perform() } })
    }
}
