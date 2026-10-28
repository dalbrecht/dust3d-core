// SPDX-License-Identifier: MIT
import Foundation

private enum FixtureError: Error { case expectation }
private func require(_ value: Bool) throws { if !value { throw FixtureError.expectation } }
private func control(
    status: UInt32 = 0, request: UInt64 = 1, system: Int32 = 0, stage: UInt32 = 0,
    source: UInt32 = 0, code: Int32 = 0, truncated: UInt8 = 0, process: Int32 = 42,
    sandbox: Bool = true, finalization: Int32 = 0, admission: UInt32 = 0,
    boundary: UInt32 = 0, generation: UInt64 = 9, observed: Int32 = 42, cause: Data = Data()
) -> WorkerPreviewControlReceipt {
    WorkerPreviewControlReceipt(
        status: status, request: request, systemError: system, stage: stage,
        source: source, code: code, truncated: truncated, process: process, sandboxEntitlementVerified: sandbox,
        finalizationError: finalization, admissionStatus: admission, boundaryStatus: boundary,
        connectionGeneration: generation, observedProcess: observed, cause: cause)
}
@main
private enum PreviewAdmissionSmoke {
    static func main() throws {
        let identity = try WorkerPreviewIdentity(document: UUID(), request: 1, revision: 7, generation: 9)
        let receipt = control()
        let gate = WorkerPreviewPublicationGate(identity: identity)
        try gate.admit(current: identity, receipt: receipt, reachedEOF: true)
        do {
            try gate.admit(current: identity, receipt: receipt, reachedEOF: true)
            throw FixtureError.expectation
        } catch WorkerPreviewAdmissionError.retired {}
        let stale = try WorkerPreviewIdentity(document: identity.document, request: 2, revision: 8, generation: 9)
        let obsolete = WorkerPreviewPublicationGate(identity: identity)
        do {
            try obsolete.admit(current: stale, receipt: receipt, reachedEOF: true)
            throw FixtureError.expectation
        } catch WorkerPreviewAdmissionError.stale {}
        try require(obsolete.isRetired)
        for invalid in [
            control(status: 4), control(request: 2), control(system: 5), control(stage: 1),
            control(source: 33), control(code: -7), control(truncated: 1), control(sandbox: false),
            control(finalization: 5), control(admission: 1), control(boundary: 3),
            control(cause: Data([255])),
        ] {
            let rejected = WorkerPreviewPublicationGate(identity: identity)
            do {
                try rejected.admit(current: identity, receipt: invalid, reachedEOF: true)
                throw FixtureError.expectation
            } catch WorkerPreviewAdmissionError.receipt(let original) {
                try require(
                    original.status == invalid.status && original.code == invalid.code
                        && original.cause == invalid.cause && original.boundaryStatus == invalid.boundaryStatus)
            }
            try require(rejected.isRetired)
        }
        for invalid in [control(observed: 0), control(process: 43), control(generation: 10)] {
            let rejected = WorkerPreviewPublicationGate(identity: identity)
            do {
                try rejected.admit(current: identity, receipt: invalid, reachedEOF: true)
                throw FixtureError.expectation
            } catch WorkerPreviewAdmissionError.observation {}
            try require(rejected.isRetired)
        }
        let closed = WorkerPreviewPublicationGate(identity: identity)
        closed.retire()
        closed.retire()
        do {
            try closed.admit(current: identity, receipt: receipt, reachedEOF: true)
            throw FixtureError.expectation
        } catch WorkerPreviewAdmissionError.retired {}
        for invalid in [
            (UUID(), UInt64(0), UInt64(9)), (UUID(), UInt64(1), UInt64(0)),
            (UUID(uuid: (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)), UInt64(1), UInt64(9)),
        ] {
            do {
                _ = try WorkerPreviewIdentity(
                    document: invalid.0, request: invalid.1,
                    revision: 0, generation: invalid.2)
                throw FixtureError.expectation
            } catch WorkerPreviewAdmissionError.invalidIdentity {}
        }
        let incomplete = WorkerPreviewPublicationGate(identity: identity)
        do {
            try incomplete.admit(current: identity, receipt: receipt, reachedEOF: false)
            throw FixtureError.expectation
        } catch WorkerPreviewAdmissionError.observation {}
        try require(incomplete.isRetired)
    }
}
