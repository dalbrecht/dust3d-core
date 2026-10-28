// SPDX-License-Identifier: MIT
import Dust3DWorkerRuntime
import Foundation

/// Normalized job outcome with original boundary classification and opaque cause.
/// No I/O or native calls; descriptions are data, never commands or log formats.
struct WorkerSessionRPCFailure: Sendable {
    let status: UInt32
    let source: UInt32
    let code: Int32
    let truncated: UInt8
    let cause: Data
    let admission: WorkerServiceAdmissionStatus
    init(_ error: any Error) {
        if let original = error as? WorkerSessionBoundaryError {
            switch original.status {
            case DUST3D_WORKER_DECODE_PROTOCOL_REJECTED.rawValue, DUST3D_WORKER_DECODE_ADMISSION_REJECTED.rawValue:
                status = DUST3D_WORKER_JOB_PROTOCOL_REJECTED.rawValue
            case DUST3D_WORKER_DECODE_LIMIT_EXCEEDED.rawValue:
                status = DUST3D_WORKER_JOB_LIMIT_EXCEEDED.rawValue
            default: status = DUST3D_WORKER_JOB_INTERNAL_FAILED.rawValue
            }
            source = original.source
            code = original.code
            truncated = original.isTruncated ? 1 : 0
            cause = original.cause
            admission = .accepted
        } else {
            status =
                error is WorkerServiceSessionError
                ? DUST3D_WORKER_JOB_PROTOCOL_REJECTED.rawValue : DUST3D_WORKER_JOB_INTERNAL_FAILED.rawValue
            source = DUST3D_WORKER_DETAIL_RUNTIME.rawValue
            code = 0
            let bytes = Data(String(describing: error).utf8)
            cause = Data(bytes.prefix(511))
            truncated = bytes.count > 511 ? 1 : 0
            admission = error is WorkerServiceSessionError ? .invalidSessionScope : .accepted
        }
    }
}

/// Preview status and original C decode classification are separate numeric domains.
/// Zero boundaryStatus means no C decode receipt; native preview receipts use their own fields.
struct WorkerPreviewRPCFailure: Sendable {
    let status: UInt32
    let boundaryStatus: UInt32
    let source: UInt32
    let code: Int32
    let truncated: UInt8
    let cause: Data
    let admission: WorkerServiceAdmissionStatus
    init(_ error: any Error) {
        let original = WorkerSessionRPCFailure(error)
        source = original.source
        code = original.code
        truncated = original.truncated
        cause = original.cause
        admission = original.admission
        if let boundary = error as? WorkerSessionBoundaryError {
            boundaryStatus = boundary.status
            switch boundary.status {
            case DUST3D_WORKER_DECODE_ADMISSION_REJECTED.rawValue:
                status = DUST3D_WORKER_PREVIEW_SESSION_ADMISSION_REJECTED.rawValue
            case DUST3D_WORKER_DECODE_PROTOCOL_REJECTED.rawValue:
                status = DUST3D_WORKER_PREVIEW_SESSION_PROTOCOL_REJECTED.rawValue
            case DUST3D_WORKER_DECODE_LIMIT_EXCEEDED.rawValue:
                status = DUST3D_WORKER_PREVIEW_SESSION_RANGE_REJECTED.rawValue
            default: status = DUST3D_WORKER_PREVIEW_SESSION_INTERNAL_FAILED.rawValue
            }
        } else {
            boundaryStatus = 0
            status =
                error is WorkerServiceSessionError
                ? DUST3D_WORKER_PREVIEW_SESSION_ADMISSION_REJECTED.rawValue
                : DUST3D_WORKER_PREVIEW_SESSION_INTERNAL_FAILED.rawValue
        }
    }
}
