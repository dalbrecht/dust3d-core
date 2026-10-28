// SPDX-License-Identifier: MIT
import Dust3DWorkerRuntime
import Foundation

struct WorkerSessionBoundaryError: Error, Sendable {
    let status: UInt32
    let source: UInt32
    let code: Int32
    let isTruncated: Bool
    let cause: Data
    init(_ result: Dust3DWorkerDecodeResult) {
        status = result.status.rawValue
        source = result.detail_source.rawValue
        code = result.detail_code
        isTruncated = result.message_truncated != 0
        cause = withUnsafeBytes(of: result.message) { Data($0.prefix { $0 != 0 }) }
    }
}
struct WorkerSessionChannelJob: Sendable {
    let expected: Dust3DWorkerRequestIdentity
    let input: Int32
    let output: Int32
    let limits: Dust3DWorkerSessionOutputLimits
}
/// Native adapter used by the ordinary Swift consumer; no authentication, channel
/// ownership or global policy is established here. All calls remain in its owner lock.
struct WorkerNativeServiceSession: WorkerServiceSessionBackend {
    func create(_ scope: WorkerServiceSessionScope) throws -> OpaquePointer {
        var configuration = Dust3DWorkerSessionConfiguration()
        configuration.document = scope.document.uuid
        configuration.connection_generation = scope.connectionGeneration
        configuration.maximum_cache_payload_bytes = scope.maximumCachePayloadBytes
        var result = Dust3DWorkerDecodeResult()
        guard let handle = dust3d_worker_session_create(configuration, &result) else {
            throw WorkerSessionBoundaryError(result)
        }
        return handle
    }
    func serve(_ owner: OpaquePointer, scope: WorkerServiceSessionScope, job: WorkerSessionChannelJob)
        -> Dust3DWorkerJobResult
    {
        dust3d_worker_session_serve_correlated(
            owner, job.expected, scope.connectionGeneration, job.input, job.output, job.limits)
    }
    func reset(_ owner: OpaquePointer, scope: WorkerServiceSessionScope) throws {
        var result = Dust3DWorkerDecodeResult()
        guard dust3d_worker_session_reset(owner, scope.connectionGeneration, &result) == 1 else {
            throw WorkerSessionBoundaryError(result)
        }
    }
    func close(_ owner: OpaquePointer) throws {
        var result = Dust3DWorkerDecodeResult()
        guard dust3d_worker_session_close(owner, &result) == 1 else { throw WorkerSessionBoundaryError(result) }
    }
    func destroy(_ owner: OpaquePointer) { dust3d_worker_session_destroy(owner) }
}

/// Captured controls for the segmented profile; never derived from channel bytes.
struct WorkerPreviewChannelJob: Sendable {
    let expected: Dust3DWorkerRequestIdentity
    let input: Int32
    let output: Int32
    let context: Dust3DWorkerPreviewArchiveContext
    let inputPolicy: Dust3DWorkerPreviewChunkPolicy
    let outputPolicy: Dust3DWorkerPreviewChunkPolicy
}
enum WorkerProfileChannelJob: Sendable {
    case legacy(WorkerSessionChannelJob)
    case preview(WorkerPreviewChannelJob)
}
enum WorkerProfileReceipt: Sendable {
    case legacy(Dust3DWorkerJobResult)
    case preview(Dust3DWorkerPreviewSessionResult)
}
/// Both profiles share the coordinator's single native owner, cache and replay state.
/// Keep receipts distinct because preview statuses do not use legacy job numbering.
struct WorkerProfileServiceSession: WorkerServiceSessionBackend {
    private let lifecycle = WorkerNativeServiceSession()
    func create(_ scope: WorkerServiceSessionScope) throws -> OpaquePointer {
        try lifecycle.create(scope)
    }
    func serve(_ owner: OpaquePointer, scope: WorkerServiceSessionScope, job: WorkerProfileChannelJob)
        -> WorkerProfileReceipt
    {
        switch job {
        case .legacy(let job):
            return .legacy(lifecycle.serve(owner, scope: scope, job: job))
        case .preview(let job):
            var correlation = Dust3DWorkerPreviewSessionScope()
            correlation.identity = job.expected
            correlation.connection_generation = scope.connectionGeneration
            return .preview(
                dust3d_worker_preview_session_serve(
                    owner, job.input, job.output, correlation, job.context, job.inputPolicy, job.outputPolicy))
        }
    }
    func reset(_ owner: OpaquePointer, scope: WorkerServiceSessionScope) throws {
        try lifecycle.reset(owner, scope: scope)
    }
    func close(_ owner: OpaquePointer) throws { try lifecycle.close(owner) }
    func destroy(_ owner: OpaquePointer) { lifecycle.destroy(owner) }
}
