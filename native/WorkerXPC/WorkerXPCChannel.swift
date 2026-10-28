// SPDX-License-Identifier: MIT
import Foundation

/// Sole actor-owned boundary. Fakes supply callbacks/PIDs without opening a connection.
protocol WorkerXPCChannel: AnyObject {
    var processIdentifier: Int32 { get }
    func installHandlers(invalidated: @escaping @Sendable () -> Void, interrupted: @escaping @Sendable () -> Void)
    func resume()
    func invalidate()
    func proxy(errorHandler: @escaping @Sendable (any Error) -> Void) -> (any Dust3DWorkerXPCProtocol)?
}
final class FoundationWorkerXPCChannel: WorkerXPCChannel {
    private let connection: NSXPCConnection
    init(_ configuration: WorkerXPCConfiguration) {
        connection = NSXPCConnection(serviceName: configuration.serviceIdentifier)
        connection.setCodeSigningRequirement(configuration.signingRequirement)
        connection.remoteObjectInterface = WorkerXPCContract.makeInterface()
    }
    var processIdentifier: Int32 { connection.processIdentifier }
    func installHandlers(invalidated: @escaping @Sendable () -> Void, interrupted: @escaping @Sendable () -> Void) {
        connection.invalidationHandler = invalidated
        connection.interruptionHandler = interrupted
    }
    func resume() { connection.resume() }
    func invalidate() { connection.invalidate() }
    func proxy(errorHandler: @escaping @Sendable (any Error) -> Void) -> (any Dust3DWorkerXPCProtocol)? {
        connection.remoteObjectProxyWithErrorHandler(errorHandler) as? Dust3DWorkerXPCProtocol
    }
}
