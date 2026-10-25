// SPDX-License-Identifier: MIT
import Darwin
import Dust3DWorkerRuntime
import Foundation

final class StaticWorkerObject: NSObject, StaticWorkerJob, Sendable {
    private let queue = DispatchQueue(label: "org.dust3d.core.static-worker.serial")
    func serve(
        _ input: FileHandle, output: FileHandle,
        withReply reply:
            @escaping @Sendable (UInt32, UInt32, UInt64, Int32, UInt32, UInt32, Int32, UInt8, Int32) -> Void
    ) {
        // All native entry points in this service share this single queue.
        queue.async {
            let result = dust3d_worker_serve(input.fileDescriptor, output.fileDescriptor)
            // NSXPC owns the duplicated handles through this invocation; ARC releases
            // this service's copies after the serial block. The fixture uses regular files,
            // not pipe-close completion, and never infers process exit from this receipt.
            reply(
                result.status.rawValue, result.operation.rawValue, result.request_identity,
                result.system_error, result.io_stage.rawValue, result.detail_source.rawValue,
                result.detail_code, result.message_truncated, getpid())
        }
    }
    func finishWhenIdle() {
        // Normal fixture shutdown only, ordered after any in-flight ordinary job.
        queue.async { exit(EXIT_SUCCESS) }
    }
}

final class StaticListenerDelegate: NSObject, NSXPCListenerDelegate {
    private let requirement: String
    private let worker = StaticWorkerObject()
    init(requirement: String) { self.requirement = requirement }
    func listener(_ listener: NSXPCListener, shouldAcceptNewConnection connection: NSXPCConnection) -> Bool {
        guard connection.effectiveUserIdentifier == getuid() else { return false }
        connection.setCodeSigningRequirement(requirement)
        connection.exportedInterface = NSXPCInterface(with: StaticWorkerJob.self)
        connection.exportedObject = worker
        let worker = worker
        connection.invalidationHandler = { worker.finishWhenIdle() }
        connection.resume()
        return true
    }
}

@main
struct WorkerXPCService {
    static func main() {
        do {
            let pins = try readDevelopmentPins()
            let delegate = StaticListenerDelegate(requirement: try developmentRequirement(pins.host))
            let listener = NSXPCListener.service()
            listener.delegate = delegate
            withExtendedLifetime(delegate) { listener.resume() }
        } catch {
            FileHandle.standardError.write(Data("Static XPC service preflight failed: \(error)\n".utf8))
            exit(EXIT_FAILURE)
        }
    }
}
