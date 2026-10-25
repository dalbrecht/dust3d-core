// SPDX-License-Identifier: MIT
import Foundation

struct WorkerControlReceipt: Sendable {
    let status: UInt32
    let operation: UInt32
    let request: UInt64
    let systemError: Int32
    let stage: UInt32
    let source: UInt32
    let code: Int32
    let truncated: UInt8
    let process: Int32
    let sandboxEntitlementVerified: Bool
    let finalizationError: Int32
    let admissionStatus: UInt32
    let connectionGeneration: UInt64
    let observedProcess: Int32
}
/// All mutable Foundation connection access stays on this actor.
actor WorkerXPCPeer {
    private let connection: NSXPCConnection
    init(requirement: String) {
        connection = NSXPCConnection(serviceName: staticServiceIdentifier)
        connection.setCodeSigningRequirement(requirement)
        connection.remoteObjectInterface = NSXPCInterface(with: StaticWorkerJob.self)
    }
    func submit(
        input: FileHandle, output: FileHandle, isStream: Bool, request: UInt64,
        generation: UInt64, outcome: WorkerOutcome<WorkerControlReceipt>
    ) throws {
        connection.invalidationHandler = {
            outcome.finish(.failure(StaticFixtureError.invalid("XPC connection invalidated")))
        }
        connection.interruptionHandler = {
            outcome.finish(.failure(StaticFixtureError.invalid("XPC connection interrupted")))
        }
        connection.resume()
        guard
            let proxy = connection.remoteObjectProxyWithErrorHandler({ error in
                let original = error as NSError
                outcome.finish(
                    .failure(
                        StaticFixtureError.invalid(
                            "\(original.domain) \(original.code): \(original.localizedDescription)")))
            }) as? StaticWorkerJob
        else { throw StaticFixtureError.invalid("Missing typed XPC proxy") }
        proxy.serve(input, output: output, isStream: isStream, requestIdentity: request) {
            [weak self]
            status, operation, request, systemError, stage, source, code, truncated, process,
            sandboxEntitlementVerified, finalizationError, admissionStatus in
            Task {
                guard let self else {
                    outcome.finish(.failure(StaticFixtureError.invalid("XPC peer observer retired")))
                    return
                }
                await self.finish(
                    status: status, operation: operation, request: request, systemError: systemError,
                    stage: stage, source: source, code: code, truncated: truncated, process: process,
                    sandboxEntitlementVerified: sandboxEntitlementVerified, finalizationError: finalizationError,
                    admissionStatus: admissionStatus, generation: generation, outcome: outcome)
            }
        }
    }
    private func finish(
        status: UInt32, operation: UInt32, request: UInt64, systemError: Int32, stage: UInt32,
        source: UInt32, code: Int32, truncated: UInt8, process: Int32, sandboxEntitlementVerified: Bool,
        finalizationError: Int32, admissionStatus: UInt32, generation: UInt64,
        outcome: WorkerOutcome<WorkerControlReceipt>
    ) {
        outcome.finish(
            .success(
                WorkerControlReceipt(
                    status: status, operation: operation, request: request, systemError: systemError,
                    stage: stage, source: source, code: code, truncated: truncated, process: process,
                    sandboxEntitlementVerified: sandboxEntitlementVerified, finalizationError: finalizationError,
                    admissionStatus: admissionStatus, connectionGeneration: generation,
                    observedProcess: connection.processIdentifier)))
    }
    func invalidate() -> Int32 {
        let process = connection.processIdentifier
        connection.invalidate()
        return process
    }
}
func withWorkerXPCPeer<Value>(
    requirement: String, deadline: DispatchTime, operation: (WorkerXPCPeer) throws -> Value
) throws -> Value {
    let peer = WorkerXPCPeer(requirement: requirement)
    let result = Result { try operation(peer) }
    let invalidated = WorkerOutcome<Int32>()
    Task.detached { invalidated.finish(.success(await peer.invalidate())) }
    let closing = Result<Void, any Error> {
        let process = try invalidated.wait(until: deadline)
        print("XPC peer PID=\(process)")
    }
    return try resolveWorkerChannelResults(operation: result, closing: closing)
}
