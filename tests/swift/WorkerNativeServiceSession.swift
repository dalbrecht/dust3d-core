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
        dust3d_worker_session_serve(owner, scope.connectionGeneration, job.input, job.output, job.limits)
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
