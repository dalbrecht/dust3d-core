// SPDX-License-Identifier: MIT
import Foundation

/// Immutable host-selected correlation, never reconstructed from a reply.
public struct WorkerPreviewIdentity: Equatable, Sendable {
    /// Host-selected nonzero document UUID.
    public let document: UUID
    /// Globally increasing nonzero request ID within this connection lifetime.
    public let request: UInt64
    /// Semantic source revision; zero is valid.
    public let revision: UInt64
    /// Host-selected nonzero connection lifetime.
    public let generation: UInt64
    /// Capture host identity before submission. Revision zero is valid.
    /// - Throws: invalidIdentity for a zero document, request or connection generation.
    public init(document: UUID, request: UInt64, revision: UInt64, generation: UInt64) throws {
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
public struct WorkerPreviewControlReceipt: Sendable {
    /// Original dedicated preview status; zero denotes completed transfer values.
    public let status: UInt32
    /// Request ID reported by the service, checked against host identity.
    public let request: UInt64
    /// Original errno from the transfer boundary.
    public let systemError: Int32
    /// Original transfer I/O stage.
    public let stage: UInt32
    /// Original typed diagnostic source, without enum coercion.
    public let source: UInt32
    /// Original signed diagnostic code.
    public let code: Int32
    /// Original explicit diagnostic truncation marker.
    public let truncated: UInt8
    /// Service-reported PID; not an independent peer observation.
    public let process: Int32
    /// Service-reported sandbox check, correlated only after peer authentication.
    public let sandboxEntitlementVerified: Bool
    /// Original output-finalization errno.
    public let finalizationError: Int32
    /// Original service scheduling admission status.
    public let admissionStatus: UInt32
    /// Separate C session-creation decode status, or zero when absent.
    public let boundaryStatus: UInt32
    /// Host-captured connection lifetime, never callback-derived.
    public let connectionGeneration: UInt64
    /// PID read independently from the authenticated host connection.
    public let observedProcess: Int32
    /// Owned opaque diagnostic bytes; never a command or format string.
    public let cause: Data
    /// Retain original control fields and independently observed host fields without normalization.
    /// Status/source/code/stage retain their dedicated preview domains; boundaryStatus is the
    /// separate C decode classification. connectionGeneration and observedProcess must come from
    /// the authenticated host connection, never from the callback. cause is opaque diagnostic data.
    /// This initializer does not authenticate, validate EOF, admit or publish the receipt.
    public init(
        status: UInt32, request: UInt64, systemError: Int32, stage: UInt32, source: UInt32, code: Int32,
        truncated: UInt8, process: Int32, sandboxEntitlementVerified: Bool, finalizationError: Int32,
        admissionStatus: UInt32, boundaryStatus: UInt32, connectionGeneration: UInt64, observedProcess: Int32,
        cause: Data
    ) {
        self.status = status
        self.request = request
        self.systemError = systemError
        self.stage = stage
        self.source = source
        self.code = code
        self.truncated = truncated
        self.process = process
        self.sandboxEntitlementVerified = sandboxEntitlementVerified
        self.finalizationError = finalizationError
        self.admissionStatus = admissionStatus
        self.boundaryStatus = boundaryStatus
        self.connectionGeneration = connectionGeneration
        self.observedProcess = observedProcess
        self.cause = cause
    }
    /// Validate original control fields against host correlation, without EOF/current-source/publication authority.
    /// - Parameter identity: Captured host identity; observations must be independently authenticated by transport.
    /// - Throws: Original observation or receipt error, retaining all original fields/diagnostic bytes.
    public func validateControl(identity: WorkerPreviewIdentity) throws {
        guard observedProcess > 0, process == observedProcess, connectionGeneration == identity.generation else {
            throw WorkerPreviewAdmissionError.observation
        }
        guard status == 0, request == identity.request, boundaryStatus == 0,
            admissionStatus == 0, systemError == 0, stage == 0, source == 0, code == 0,
            truncated == 0, cause.isEmpty, finalizationError == 0, sandboxEntitlementVerified
        else {
            throw WorkerPreviewAdmissionError.receipt(self)
        }
    }
}
/// Original publication rejection; a receipt rejection retains all original control fields.
public enum WorkerPreviewAdmissionError: Error {
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
        guard reachedEOF else { throw WorkerPreviewAdmissionError.observation }
        try receipt.validateControl(identity: identity)
    }
}
