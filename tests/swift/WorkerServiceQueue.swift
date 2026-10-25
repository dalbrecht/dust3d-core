// SPDX-License-Identifier: MIT
import Dust3DWorkerRuntime
import Foundation

/// Original numeric bridge receipt and owned opaque diagnostic bytes; never log as commands.
struct WorkerServiceQueueError: Error, Sendable {
    enum Operation: String, Sendable { case create, submit, begin, abandon, collector, admit }
    let operation: Operation
    let status: UInt32
    let source: UInt32
    let code: Int32
    let truncated: Bool
    let cause: Data
}
struct WorkerServiceRequest: Sendable {
    let identity: Dust3DWorkerRequestIdentity
    let frame: Data
}
enum WorkerServiceBegin: Sendable {
    case idle
    case busy
    case started(WorkerServiceRequest)
}

/// Collecting-thread owner only: deliberately not Sendable and never captured across queues.
/// Only copied requests, receipts and closure results may cross concurrency boundaries.
/// This fixture adapter does not authenticate observations, perform I/O or publish app state.
final class WorkerServiceQueue {
    private let owner: OpaquePointer
    private let maximumInputBytes: UInt64

    /// Create a bounded C coordinator, retaining original typed failure fields.
    init(limits: Dust3DWorkerWireLimits) throws {
        var result = Dust3DWorkerQueueResult()
        guard let created = dust3d_worker_service_queue_create(limits, &result) else {
            throw Self.failure(.create, result)
        }
        owner = created
        maximumInputBytes = limits.maximum_input_bytes
    }
    deinit { dust3d_worker_service_queue_destroy(owner) }

    /// Copy/validate before replacing waiting work. The returned receipt owns replacement identity.
    @discardableResult
    func submit(_ frame: Data) throws -> Dust3DWorkerQueueResult {
        let result = frame.withUnsafeBytes {
            dust3d_worker_service_queue_submit(owner, $0.bindMemory(to: UInt8.self).baseAddress, $0.count)
        }
        guard result.status == DUST3D_WORKER_QUEUE_READY else { throw Self.failure(.submit, result) }
        return result
    }
    /// Freeze trusted generation before transport submission; return only owned copied values.
    func begin(generation: UInt64) throws -> WorkerServiceBegin {
        let result = dust3d_worker_service_queue_begin(owner, generation)
        switch result.status {
        case DUST3D_WORKER_QUEUE_IDLE: return .idle
        case DUST3D_WORKER_QUEUE_BUSY: return .busy
        case DUST3D_WORKER_QUEUE_BEGUN:
            var count = 0
            guard let bytes = dust3d_worker_service_queue_request_bytes(owner, &count),
                count > 64, UInt64(count) <= maximumInputBytes + 64
            else { throw Self.missing(.begin) }
            return .started(.init(identity: result.active_identity, frame: Data(bytes: bytes, count: count)))
        default: throw Self.failure(.begin, result)
        }
    }
    /// Borrow on this collecting thread only. Never destroy/retain/transfer the collector.
    func withReplyCollector<Value>(_ body: (OpaquePointer) throws -> Value) throws -> Value {
        guard let collector = dust3d_worker_service_queue_reply(owner) else { throw Self.missing(.collector) }
        return try body(collector)
    }
    /// Retire publication; transport still drains. Matching admission/abandon releases the slot.
    func cancel(_ identity: Dust3DWorkerRequestIdentity) { dust3d_worker_service_queue_cancel(owner, identity) }
    /// Seal submission/publication without stopping native execution or collecting transport.
    func close() { dust3d_worker_service_queue_close(owner) }
    /// Call after owned transport cleanup; preserve its primary error separately.
    func abandon(_ identity: Dust3DWorkerRequestIdentity) throws {
        let result = dust3d_worker_service_queue_abandon(owner, identity)
        guard result.status == DUST3D_WORKER_QUEUE_READY else { throw Self.failure(.abandon, result) }
    }
    /// Admit current source once, using real caller observations; copy values inside body.
    /// The independent decoded owner is released even when body throws its original Swift error.
    func withDecodedReply<Value>(
        observed: Dust3DWorkerServiceObservation, receipt: Dust3DWorkerServiceReceipt,
        maximumDecodedBytes: UInt64, _ body: (OpaquePointer) throws -> Value
    ) throws -> Value {
        var result = Dust3DWorkerDecodeResult()
        guard let decoded = dust3d_worker_service_queue_admit(owner, observed, receipt, maximumDecodedBytes, &result)
        else {
            throw WorkerServiceQueueError(
                operation: .admit, status: result.status.rawValue, source: result.detail_source.rawValue,
                code: result.detail_code, truncated: result.message_truncated != 0,
                cause: withUnsafeBytes(of: result.message) { Data($0.prefix { $0 != 0 }) })
        }
        defer { dust3d_worker_decoded_destroy(decoded) }
        return try body(decoded)
    }
    private static func failure(_ operation: WorkerServiceQueueError.Operation, _ result: Dust3DWorkerQueueResult)
        -> WorkerServiceQueueError
    {
        WorkerServiceQueueError(
            operation: operation, status: result.status.rawValue, source: result.detail_source.rawValue,
            code: result.detail_code, truncated: result.message_truncated != 0,
            cause: withUnsafeBytes(of: result.message) { Data($0.prefix { $0 != 0 }) })
    }
    private static func missing(_ operation: WorkerServiceQueueError.Operation) -> WorkerServiceQueueError {
        WorkerServiceQueueError(
            operation: operation, status: DUST3D_WORKER_QUEUE_INTERNAL_FAILED.rawValue,
            source: DUST3D_WORKER_DETAIL_RUNTIME.rawValue, code: 0, truncated: false,
            cause: Data("Coordinator has no valid active borrowed view".utf8))
    }
}
