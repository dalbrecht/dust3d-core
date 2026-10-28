// SPDX-License-Identifier: MIT
import Dust3DWorkerRuntime
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
    let cause: Data
}
/// All mutable Foundation connection access stays on this actor.
actor WorkerXPCPeer {
    private let connection: NSXPCConnection
    private var isResumed = false
    init(requirement: String) {
        connection = NSXPCConnection(serviceName: staticServiceIdentifier)
        connection.setCodeSigningRequirement(requirement)
        connection.remoteObjectInterface = NSXPCInterface(with: StaticWorkerJob.self)
    }
    func submit(
        input: FileHandle, output: FileHandle, isStream: Bool, request: UInt64,
        generation: UInt64, outcome: WorkerOutcome<WorkerControlReceipt>
    ) throws {
        let proxy = try proxy(outcome: outcome)
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
    private func proxy<Receipt: Sendable>(outcome: WorkerOutcome<Receipt>) throws -> any StaticWorkerJob {
        connection.invalidationHandler = {
            outcome.finish(.failure(StaticFixtureError.invalid("XPC connection invalidated")))
        }
        connection.interruptionHandler = {
            outcome.finish(.failure(StaticFixtureError.invalid("XPC connection interrupted")))
        }
        if !isResumed {
            connection.resume()
            isResumed = true
        }
        guard
            let proxy = connection.remoteObjectProxyWithErrorHandler({ error in
                let original = error as NSError
                outcome.finish(
                    .failure(
                        StaticFixtureError.invalid(
                            "\(original.domain) \(original.code): \(original.localizedDescription)")))
            }) as? StaticWorkerJob
        else { throw StaticFixtureError.invalid("Missing typed XPC proxy") }
        return proxy
    }
    func submitSession(
        input: FileHandle, output: FileHandle, identity: Dust3DWorkerRequestIdentity,
        generation: UInt64, maximumCachePayloadBytes: UInt64, limits: Dust3DWorkerSessionOutputLimits,
        outcome: WorkerOutcome<WorkerControlReceipt>
    ) throws {
        let proxy = try proxy(outcome: outcome)
        proxy.serveSession(
            input, output: output, isStream: false, document: UUID(uuid: identity.document) as NSUUID,
            requestIdentity: identity.request, revision: identity.revision, connectionGeneration: generation,
            maximumCachePayloadBytes: maximumCachePayloadBytes, maximumWireBytes: limits.maximum_wire_bytes,
            maximumDecodedBytes: limits.maximum_decoded_bytes, maximumSourceBytes: limits.maximum_source_bytes
        ) {
            [weak self]
            status, operation, request, systemError, stage, source, code, truncated, process,
            sandbox, finalizationError, admissionStatus, cause in
            Task {
                guard let self else {
                    outcome.finish(.failure(StaticFixtureError.invalid("Session peer observer retired")))
                    return
                }
                await self.finish(
                    status: status, operation: operation, request: request, systemError: systemError,
                    stage: stage, source: source, code: code, truncated: truncated, process: process,
                    sandboxEntitlementVerified: sandbox, finalizationError: finalizationError,
                    admissionStatus: admissionStatus, generation: generation, outcome: outcome, cause: cause)
            }
        }
    }
    func submitPreview(
        input: FileHandle, output: FileHandle, identity: WorkerPreviewIdentity,
        maximumCachePayloadBytes: UInt64, context: Dust3DWorkerPreviewArchiveContext,
        inputPolicy: Dust3DWorkerPreviewChunkPolicy, outputPolicy: Dust3DWorkerPreviewChunkPolicy,
        outcome: WorkerOutcome<WorkerPreviewControlReceipt>
    ) throws {
        let proxy = try proxy(outcome: outcome)
        proxy.servePreview(
            input, output: output, isStream: false, document: identity.document as NSUUID,
            requestIdentity: identity.request, revision: identity.revision, connectionGeneration: identity.generation,
            maximumCachePayloadBytes: maximumCachePayloadBytes, archiveBytes: context.archive_bytes,
            maximumIndexedBytes: context.maximum_indexed_bytes, maximumSourceBytes: context.maximum_source_bytes,
            maximumInputTransferBytes: inputPolicy.maximum_transfer_bytes,
            inputChunkBytes: inputPolicy.chunk_payload_bytes,
            maximumOutputTransferBytes: outputPolicy.maximum_transfer_bytes,
            outputChunkBytes: outputPolicy.chunk_payload_bytes
        ) {
            [weak self]
            status, request, systemError, stage, source, code, truncated, process,
            sandbox, finalizationError, admissionStatus, boundaryStatus, cause in
            Task {
                guard let self else {
                    outcome.finish(.failure(StaticFixtureError.invalid("Preview peer observer retired")))
                    return
                }
                await self.finishPreview(
                    status: status, request: request, systemError: systemError, stage: stage,
                    source: source, code: code, truncated: truncated, process: process,
                    sandbox: sandbox, finalizationError: finalizationError, admissionStatus: admissionStatus,
                    boundaryStatus: boundaryStatus, generation: identity.generation, cause: cause, outcome: outcome)
            }
        }
    }
    private func finishPreview(
        status: UInt32, request: UInt64, systemError: Int32, stage: UInt32, source: UInt32,
        code: Int32, truncated: UInt8, process: Int32, sandbox: Bool, finalizationError: Int32,
        admissionStatus: UInt32, boundaryStatus: UInt32, generation: UInt64, cause: Data,
        outcome: WorkerOutcome<WorkerPreviewControlReceipt>
    ) {
        guard cause.count <= 511 else {
            outcome.finish(.failure(StaticFixtureError.invalid("Preview diagnostic exceeds control bound")))
            return
        }
        outcome.finish(
            .success(
                WorkerPreviewControlReceipt(
                    status: status, request: request,
                    systemError: systemError, stage: stage, source: source, code: code, truncated: truncated,
                    process: process, sandboxEntitlementVerified: sandbox, finalizationError: finalizationError,
                    admissionStatus: admissionStatus, boundaryStatus: boundaryStatus, connectionGeneration: generation,
                    observedProcess: connection.processIdentifier, cause: cause)))
    }
    func resetSession(
        document: UUID, request: UInt64, generation: UInt64, maximumCachePayloadBytes: UInt64,
        outcome: WorkerOutcome<WorkerControlReceipt>
    ) throws {
        let proxy = try proxy(outcome: outcome)
        proxy.resetSession(
            document as NSUUID, requestIdentity: request, connectionGeneration: generation,
            maximumCachePayloadBytes: maximumCachePayloadBytes
        ) {
            [weak self]
            status, operation, request, systemError, stage, source, code, truncated, process,
            sandbox, finalizationError, admissionStatus, cause in
            Task {
                guard let self else {
                    outcome.finish(.failure(StaticFixtureError.invalid("Reset peer observer retired")))
                    return
                }
                await self.finish(
                    status: status, operation: operation, request: request, systemError: systemError,
                    stage: stage, source: source, code: code, truncated: truncated, process: process,
                    sandboxEntitlementVerified: sandbox, finalizationError: finalizationError,
                    admissionStatus: admissionStatus, generation: generation, outcome: outcome, cause: cause)
            }
        }
    }
    func closeSession(request: UInt64, generation: UInt64, outcome: WorkerOutcome<WorkerControlReceipt>) throws {
        let proxy = try proxy(outcome: outcome)
        proxy.closeSession(request) {
            [weak self]
            status, operation, request, systemError, stage, source, code, truncated, process,
            sandbox, finalizationError, admissionStatus, cause in
            Task {
                guard let self else {
                    outcome.finish(.failure(StaticFixtureError.invalid("Close peer observer retired")))
                    return
                }
                await self.finish(
                    status: status, operation: operation, request: request, systemError: systemError,
                    stage: stage, source: source, code: code, truncated: truncated, process: process,
                    sandboxEntitlementVerified: sandbox, finalizationError: finalizationError,
                    admissionStatus: admissionStatus, generation: generation, outcome: outcome, cause: cause)
            }
        }
    }
    private func finish(
        status: UInt32, operation: UInt32, request: UInt64, systemError: Int32, stage: UInt32,
        source: UInt32, code: Int32, truncated: UInt8, process: Int32, sandboxEntitlementVerified: Bool,
        finalizationError: Int32, admissionStatus: UInt32, generation: UInt64,
        outcome: WorkerOutcome<WorkerControlReceipt>, cause: Data = Data()
    ) {
        guard cause.count <= 511 else {
            outcome.finish(.failure(StaticFixtureError.invalid("Session diagnostic exceeds control bound")))
            return
        }
        outcome.finish(
            .success(
                WorkerControlReceipt(
                    status: status, operation: operation, request: request, systemError: systemError,
                    stage: stage, source: source, code: code, truncated: truncated, process: process,
                    sandboxEntitlementVerified: sandboxEntitlementVerified, finalizationError: finalizationError,
                    admissionStatus: admissionStatus, connectionGeneration: generation,
                    observedProcess: connection.processIdentifier, cause: cause)))
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
