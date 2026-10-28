// SPDX-License-Identifier: MIT
import Foundation

/// Immutable host-selected correlation, never reconstructed from a reply.
struct WorkerPreviewIdentity: Equatable, Sendable {
    let document: UUID
    let request: UInt64
    let revision: UInt64
    let generation: UInt64
    init(document: UUID, request: UInt64, revision: UInt64, generation: UInt64) throws {
        var bytes = document.uuid
        guard request > 0, generation > 0, withUnsafeBytes(of: &bytes, { $0.contains { $0 != 0 } }) else {
            throw WorkerPreviewAdmissionError.invalidIdentity
        }
        self.document = document
        self.request = request
        self.revision = revision
        self.generation = generation
    }
}
/// Preview RPC numbers retain their own domain. Only observedProcess is an OS peer observation;
/// connectionGeneration is independently captured by the host actor, never supplied by the service.
struct WorkerPreviewControlReceipt: Sendable {
    let status: UInt32
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
    let boundaryStatus: UInt32
    let connectionGeneration: UInt64
    let observedProcess: Int32
    let cause: Data
}
/// Original channel boundary receipt; diagnostic bytes are opaque data.
struct WorkerPreviewTransferFailure: Error, Sendable {
    enum Operation: Sendable { case archive, reply }
    let operation: Operation
    let status: UInt32
    let source: UInt32
    let code: Int32
    let systemError: Int32
    let stage: UInt32
    let truncated: UInt8
    let cause: Data
}
enum WorkerPreviewAdmissionError: Error {
    case invalidIdentity, retired, stale, observation
    case receipt(WorkerPreviewControlReceipt)
}
/// Collecting-thread one-shot gate, retaining metadata only. Caller authenticates the connection,
/// decodes with the captured policies through actual EOF, then supplies current source before copying.
/// No scheduling, native cancellation, process control or application publication occurs here.
final class WorkerPreviewPublicationGate {
    let identity: WorkerPreviewIdentity
    private(set) var isRetired = false
    init(identity: WorkerPreviewIdentity) { self.identity = identity }
    func retire() { isRetired = true }
    func admit(current: WorkerPreviewIdentity, receipt: WorkerPreviewControlReceipt, reachedEOF: Bool) throws {
        guard !isRetired else { throw WorkerPreviewAdmissionError.retired }
        isRetired = true
        guard current == identity else { throw WorkerPreviewAdmissionError.stale }
        guard reachedEOF, receipt.observedProcess > 0,
            receipt.process == receipt.observedProcess, receipt.connectionGeneration == identity.generation
        else { throw WorkerPreviewAdmissionError.observation }
        // These are the dedicated preview OK/accepted values, not legacy job status conversion.
        guard receipt.status == 0, receipt.request == identity.request, receipt.boundaryStatus == 0,
            receipt.admissionStatus == 0, receipt.systemError == 0, receipt.stage == 0,
            receipt.source == 0, receipt.code == 0, receipt.truncated == 0, receipt.cause.isEmpty,
            receipt.finalizationError == 0, receipt.sandboxEntitlementVerified
        else { throw WorkerPreviewAdmissionError.receipt(receipt) }
    }
}
