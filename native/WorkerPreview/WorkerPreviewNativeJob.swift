// SPDX-License-Identifier: MIT
import Dust3DWorkerClient
import Dust3DWorkerRuntime
import Dust3DWorkerXPC
import Foundation

private struct WorkerPreviewCloseFailure: Error {
    let causes: [any Error]
}
private func closeWorkerPreviewHandles(_ handles: [FileHandle]) throws {
    var failures: [any Error] = []
    for handle in handles {
        do { try handle.close() } catch { failures.append(error) }
    }
    if !failures.isEmpty { throw WorkerPreviewCloseFailure(causes: failures) }
}
private func onWorkerPreviewQueue<Value: Sendable>(
    _ queue: DispatchQueue, _ operation: @escaping @Sendable () throws -> Value
) async throws -> Value {
    try await withCheckedThrowingContinuation { (continuation: CheckedContinuation<Value, any Error>) in
        queue.async { continuation.resume(with: Result(catching: operation)) }
    }
}

/// Immutable role owners; only the corresponding phase closes each handle after its blocking call.
struct WorkerPreviewNativeJob<Value: Sendable>: WorkerPreviewJob {
    let identity: WorkerPreviewIdentity
    let source: WorkerPreviewSource
    let transport: any WorkerPreviewTransport
    let copy: @Sendable (OpaquePointer) throws -> Value
    private let request: WorkerSocketChannel
    private let reply: WorkerSocketChannel
    private let writer = DispatchQueue(label: "org.dust3d.worker.preview.writer", qos: .utility)
    private let reader = DispatchQueue(label: "org.dust3d.worker.preview.reader", qos: .utility)
    init(
        identity: WorkerPreviewIdentity, source: WorkerPreviewSource, transport: any WorkerPreviewTransport,
        copy: @escaping @Sendable (OpaquePointer) throws -> Value
    ) throws {
        self.identity = identity
        self.source = source
        self.transport = transport
        self.copy = copy
        let capturedRequest = try makeWorkerSocketChannel()
        let capturedReply: WorkerSocketChannel
        do { capturedReply = try makeWorkerSocketChannel() } catch {
            let original = error
            do { try closeWorkerPreviewHandles([capturedRequest.reader, capturedRequest.writer]) } catch {
                throw WorkerChannelOperationFailure(operation: original, closing: error)
            }
            throw original
        }
        request = capturedRequest
        reply = capturedReply
    }
    private var scope: Dust3DWorkerPreviewArchiveScope {
        var scope = Dust3DWorkerPreviewArchiveScope()
        scope.identity.document = identity.document.uuid
        scope.identity.request = identity.request
        scope.identity.revision = identity.revision
        scope.connection_generation = identity.generation
        return scope
    }
    private var context: Dust3DWorkerPreviewArchiveContext {
        Dust3DWorkerPreviewArchiveContext(
            archive_bytes: source.options.archiveBytes,
            maximum_source_bytes: source.options.maximumSourceBytes,
            maximum_indexed_bytes: source.options.maximumIndexedBytes)
    }
    func write() async throws {
        try await onWorkerPreviewQueue(writer) {
            try withOwnedWorkerChannel(request.writer) {
                let policy = Dust3DWorkerPreviewChunkPolicy(
                    maximum_transfer_bytes: source.options.maximumInputTransferBytes,
                    chunk_payload_bytes: source.options.inputChunkBytes)
                let receipt = source.archive.withUnsafeBytes {
                    dust3d_worker_preview_archive_write(
                        request.writer.fileDescriptor, scope, context,
                        $0.bindMemory(to: UInt8.self).baseAddress, $0.count, policy)
                }
                guard receipt.status == DUST3D_WORKER_PREVIEW_ARCHIVE_OK else {
                    throw WorkerPreviewTransferFailure(
                        operation: .archive, status: receipt.status.rawValue,
                        source: 0, code: receipt.detail_code, systemError: receipt.system_error,
                        stage: receipt.io_stage.rawValue, truncated: receipt.message_truncated,
                        cause: withUnsafeBytes(of: receipt.message) { Data($0.prefix { $0 != 0 }) })
                }
            }
        }
    }
    func read() async throws -> Value {
        try await onWorkerPreviewQueue(reader) {
            try withOwnedWorkerChannel(reply.reader) {
                let policy = Dust3DWorkerPreviewChunkPolicy(
                    maximum_transfer_bytes: source.options.maximumOutputTransferBytes,
                    chunk_payload_bytes: source.options.outputChunkBytes)
                var receipt = Dust3DWorkerPreviewSessionResult()
                guard
                    let owner = dust3d_worker_preview_session_read(
                        reply.reader.fileDescriptor, scope, context, policy, &receipt)
                else {
                    throw WorkerPreviewTransferFailure(
                        operation: .reply, status: receipt.status.rawValue,
                        source: receipt.detail_source.rawValue, code: receipt.detail_code,
                        systemError: receipt.system_error, stage: receipt.io_stage.rawValue,
                        truncated: receipt.message_truncated,
                        cause: withUnsafeBytes(of: receipt.message) { Data($0.prefix { $0 != 0 }) })
                }
                // Native owner never escapes this dedicated reader worker, including a throwing copier.
                defer { dust3d_worker_session_output_destroy(owner) }
                return try copy(owner)
            }
        }
    }
    func control() async throws -> WorkerPreviewControlReceipt {
        let operation: Result<WorkerPreviewControlReceipt, any Error>
        do {
            operation = .success(
                try await transport.submitPreview(
                    input: request.reader, output: reply.writer,
                    isStream: true, identity: identity, options: source.options))
        } catch { operation = .failure(error) }
        // These are only the parent's transferred-role copies, never reader/writer phase handles.
        return try resolveWorkerChannelResults(
            operation: operation,
            closing: Result { try closeWorkerPreviewHandles([request.reader, reply.writer]) })
    }
}
