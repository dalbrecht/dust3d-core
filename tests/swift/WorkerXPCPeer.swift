// SPDX-License-Identifier: MIT
import Dust3DWorkerClient
import Dust3DWorkerRuntime
import Dust3DWorkerXPC
import Foundation

typealias WorkerControlReceipt = WorkerXPCLegacyReceipt
/// Test-only C-value facade. Mutable connection state and original receipts live in the SDK.
actor WorkerXPCPeer {
    private let connection: WorkerXPCConnection
    init(requirement: String) throws {
        connection = WorkerXPCConnection(
            configuration: try WorkerXPCConfiguration(
                serviceIdentifier: staticServiceIdentifier, signingRequirement: requirement))
    }
    func handshake(generation: UInt64) async throws -> WorkerXPCHandshakeReceipt {
        try await connection.handshake(generation: generation)
    }
    func submit(
        input: FileHandle, output: FileHandle, isStream: Bool, request: UInt64,
        generation: UInt64, outcome: WorkerOutcome<WorkerControlReceipt>
    ) async throws {
        outcome.finish(
            .success(
                try await connection.submit(
                    input: input, output: output,
                    isStream: isStream, request: request, generation: generation)))
    }
    func submitSession(
        input: FileHandle, output: FileHandle, identity: Dust3DWorkerRequestIdentity,
        generation: UInt64, maximumCachePayloadBytes: UInt64, limits: Dust3DWorkerSessionOutputLimits,
        outcome: WorkerOutcome<WorkerControlReceipt>
    ) async throws {
        let captured = try WorkerPreviewIdentity(
            document: UUID(uuid: identity.document),
            request: identity.request, revision: identity.revision, generation: generation)
        let options = WorkerXPCSessionOptions(
            maximumCachePayloadBytes: maximumCachePayloadBytes,
            maximumWireBytes: limits.maximum_wire_bytes, maximumDecodedBytes: limits.maximum_decoded_bytes,
            maximumSourceBytes: limits.maximum_source_bytes)
        outcome.finish(
            .success(
                try await connection.submitSession(
                    input: input, output: output,
                    identity: captured, options: options)))
    }
    func submitPreview(
        input: FileHandle, output: FileHandle, identity: WorkerPreviewIdentity,
        maximumCachePayloadBytes: UInt64, context: Dust3DWorkerPreviewArchiveContext,
        inputPolicy: Dust3DWorkerPreviewChunkPolicy, outputPolicy: Dust3DWorkerPreviewChunkPolicy,
        outcome: WorkerOutcome<WorkerPreviewControlReceipt>
    ) async throws {
        let options = WorkerXPCPreviewOptions(
            maximumCachePayloadBytes: maximumCachePayloadBytes,
            archiveBytes: context.archive_bytes, maximumIndexedBytes: context.maximum_indexed_bytes,
            maximumSourceBytes: context.maximum_source_bytes,
            maximumInputTransferBytes: inputPolicy.maximum_transfer_bytes,
            inputChunkBytes: inputPolicy.chunk_payload_bytes,
            maximumOutputTransferBytes: outputPolicy.maximum_transfer_bytes,
            outputChunkBytes: outputPolicy.chunk_payload_bytes)
        outcome.finish(
            .success(
                try await connection.submitPreview(
                    input: input, output: output,
                    identity: identity, options: options)))
    }
    func resetSession(
        document: UUID, request: UInt64, generation: UInt64, maximumCachePayloadBytes: UInt64,
        outcome: WorkerOutcome<WorkerControlReceipt>
    ) async throws {
        outcome.finish(
            .success(
                try await connection.resetSession(
                    document: document, request: request,
                    generation: generation, maximumCachePayloadBytes: maximumCachePayloadBytes)))
    }
    func closeSession(request: UInt64, generation: UInt64, outcome: WorkerOutcome<WorkerControlReceipt>) async throws {
        outcome.finish(.success(try await connection.closeSession(request: request, generation: generation)))
    }
    func invalidate() async throws -> Int32 { try await connection.invalidate() }
}
func withWorkerXPCPeer<Value>(
    requirement: String, generation: UInt64, deadline: DispatchTime, operation: (WorkerXPCPeer) throws -> Value
) throws -> Value {
    let peer = try WorkerXPCPeer(requirement: requirement)
    let identity = WorkerOutcome<WorkerXPCHandshakeReceipt>()
    Task.detached {
        do { identity.finish(.success(try await peer.handshake(generation: generation))) } catch {
            identity.finish(.failure(error))
        }
    }
    let result = Result {
        let captured = try identity.wait(until: deadline)
        print(
            "XPC identity handshake verified before job submission: PID=\(captured.observedProcess) generation=\(captured.connectionGeneration)"
        )
        return try operation(peer)
    }
    let invalidated = WorkerOutcome<Int32>()
    Task.detached {
        do { invalidated.finish(.success(try await peer.invalidate())) } catch { invalidated.finish(.failure(error)) }
    }
    let closing = Result<Void, any Error> {
        let process = try invalidated.wait(until: deadline)
        print("XPC peer PID=\(process)")
    }
    return try resolveWorkerChannelResults(operation: result, closing: closing)
}
