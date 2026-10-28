// SPDX-License-Identifier: MIT
import Darwin
import Dust3DWorkerRuntime
import Dust3DWorkerXPC
import Foundation

#if canImport(Dust3DWorkerAppleImages)
    import Dust3DWorkerAppleImages
#endif

final class WorkerServiceObject: NSObject, Dust3DWorkerXPCProtocol, Sendable {
    private let sandboxEntitlementVerified: Bool
    private let scheduler: WorkerServiceScheduler
    private let candidate: WorkerServiceConnection
    private let session = WorkerServiceSession(backend: WorkerProfileServiceSession())
    init(sandboxEntitlementVerified: Bool, scheduler: WorkerServiceScheduler, candidate: WorkerServiceConnection) {
        self.sandboxEntitlementVerified = sandboxEntitlementVerified
        self.scheduler = scheduler
        self.candidate = candidate
    }
    func serve(
        _ input: FileHandle, output: FileHandle, isStream: Bool, requestIdentity: UInt64,
        withReply reply:
            @escaping @Sendable (
                UInt32, UInt32, UInt64, Int32, UInt32, UInt32, Int32, UInt8, Int32, Bool, Int32, UInt32
            ) ->
            Void
    ) {
        let decision = scheduler.submit(
            owner: candidate, requestIdentity: requestIdentity,
            onCancelled: {
                reply(
                    DUST3D_WORKER_JOB_INTERNAL_FAILED.rawValue, DUST3D_WORKER_OPERATION_UNKNOWN.rawValue,
                    requestIdentity, 0, 0, 0, 0, 0, getpid(), self.sandboxEntitlementVerified, 0,
                    WorkerServiceAdmissionStatus.cancelledBeforeStart.rawValue)
            },
            { lease in
                var decoders = WorkerAppleImageDecoder.configuration()
                // Only procedural PNG is enabled for this healthy fixture stage.
                // Imported/GLB rendering remains quarantined and gets no provider.
                decoders.imported = Dust3DWorkerImageDecoder()
                let result = dust3d_worker_serve_with_decoders(input.fileDescriptor, output.fileDescriptor, &decoders)
                // Explicit stream EOF is independent of NSXPC's handle-retention lifetime.
                // This orderly half-close happens only after all runtime writes return.
                let finalized = isStream ? shutdown(output.fileDescriptor, SHUT_WR) : 0
                let finalizationError: Int32 = finalized == 0 ? 0 : errno
                let admission: WorkerServiceAdmissionStatus =
                    self.scheduler.complete(owner: self.candidate, lease) ? .accepted : .staleCompletion
                reply(
                    result.status.rawValue, result.operation.rawValue, result.request_identity,
                    result.system_error, result.io_stage.rawValue, result.detail_source.rawValue,
                    result.detail_code, result.message_truncated, getpid(), self.sandboxEntitlementVerified,
                    finalizationError, admission.rawValue)
            })
        if case .accepted = decision { return }
        // Rejection does not parse, execute, enqueue or modify possibly aliased channels.
        reply(
            DUST3D_WORKER_JOB_INTERNAL_FAILED.rawValue, DUST3D_WORKER_OPERATION_UNKNOWN.rawValue,
            0, 0, 0, 0, 0, 0, getpid(), sandboxEntitlementVerified, 0, decision.status.rawValue)
    }
    typealias SessionReply =
        @Sendable (
            UInt32, UInt32, UInt64, Int32, UInt32, UInt32, Int32, UInt8, Int32, Bool, Int32, UInt32, Data
        ) -> Void
    private func rejectSession(
        _ error: any Error, request: UInt64, admission: WorkerServiceAdmissionStatus? = nil, reply: SessionReply
    ) {
        let failure = WorkerSessionRPCFailure(error)
        reply(
            failure.status, DUST3D_WORKER_OPERATION_UNKNOWN.rawValue, request, 0, 0,
            failure.source, failure.code, failure.truncated, getpid(), sandboxEntitlementVerified, 0,
            (admission ?? failure.admission).rawValue, failure.cause)
    }
    func serveSession(
        _ input: FileHandle, output: FileHandle, isStream: Bool, document: NSUUID,
        requestIdentity: UInt64, revision: UInt64, connectionGeneration: UInt64,
        maximumCachePayloadBytes: UInt64, maximumWireBytes: UInt64, maximumDecodedBytes: UInt64,
        maximumSourceBytes: UInt64,
        withReply reply: @escaping SessionReply
    ) {
        let scope: WorkerServiceSessionScope
        do {
            scope = try WorkerServiceSessionScope(
                document: document as UUID, connectionGeneration: connectionGeneration,
                maximumCachePayloadBytes: maximumCachePayloadBytes)
        } catch {
            rejectSession(error, request: requestIdentity, reply: reply)
            return
        }
        guard input.fileDescriptor >= 0, output.fileDescriptor >= 0, input.fileDescriptor != output.fileDescriptor
        else {
            rejectSession(
                WorkerServiceBootstrapError.invalidRequest("Session requires separate valid channels"),
                request: requestIdentity,
                admission: .invalidChannels, reply: reply)
            return
        }
        var expected = Dust3DWorkerRequestIdentity()
        expected.document = scope.document.uuid
        expected.request = requestIdentity
        expected.revision = revision
        let job = WorkerSessionChannelJob(
            expected: expected, input: input.fileDescriptor, output: output.fileDescriptor,
            limits: Dust3DWorkerSessionOutputLimits(
                maximum_wire_bytes: maximumWireBytes, maximum_decoded_bytes: maximumDecodedBytes,
                maximum_source_bytes: maximumSourceBytes))
        let decision = scheduler.submit(
            owner: candidate, requestIdentity: requestIdentity,
            onCancelled: {
                self.rejectSession(
                    WorkerServiceBootstrapError.invalidRequest("Session job withdrawn before start"),
                    request: requestIdentity,
                    admission: .cancelledBeforeStart, reply: reply)
            },
            { lease in
                do {
                    // NSXPC owns the received handles only for this invocation. Keep
                    // both owners alive until the queued native call and EOF complete.
                    let result = try withExtendedLifetime((input, output)) {
                        let receipt = try self.session.serve(scope: scope, job: .legacy(job))
                        guard case .legacy(let result) = receipt else {
                            throw WorkerServiceBootstrapError.invalidRequest("Unexpected session receipt profile")
                        }
                        return result
                    }
                    // Only an admitted native invocation finalizes its distinct output stream.
                    let finalized = isStream ? shutdown(output.fileDescriptor, SHUT_WR) : 0
                    let finalizationError: Int32 = finalized == 0 ? 0 : errno
                    let admission: WorkerServiceAdmissionStatus =
                        self.scheduler.complete(owner: self.candidate, lease) ? .accepted : .staleCompletion
                    let cause = withUnsafeBytes(of: result.message) { Data($0.prefix { $0 != 0 }) }
                    reply(
                        result.status.rawValue, result.operation.rawValue, result.request_identity, result.system_error,
                        result.io_stage.rawValue, result.detail_source.rawValue, result.detail_code,
                        result.message_truncated,
                        getpid(), self.sandboxEntitlementVerified, finalizationError, admission.rawValue, cause)
                } catch {
                    let completed = self.scheduler.complete(owner: self.candidate, lease)
                    self.rejectSession(
                        error, request: requestIdentity, admission: completed ? nil : .staleCompletion, reply: reply)
                }
            })
        if case .accepted = decision { return }
        rejectSession(
            WorkerServiceBootstrapError.invalidRequest("Session admission rejected: \(decision.status.rawValue)"),
            request: requestIdentity, admission: decision.status, reply: reply)
    }
    typealias PreviewReply =
        @Sendable (
            UInt32, UInt64, Int32, UInt32, UInt32, Int32, UInt8, Int32, Bool, Int32, UInt32, UInt32, Data
        ) -> Void
    private func rejectPreview(
        _ error: any Error, request: UInt64, admission: WorkerServiceAdmissionStatus? = nil, reply: PreviewReply
    ) {
        let failure = WorkerPreviewRPCFailure(error)
        reply(
            failure.status, request, 0, 0, failure.source, failure.code, failure.truncated,
            getpid(), sandboxEntitlementVerified, 0, (admission ?? failure.admission).rawValue,
            failure.boundaryStatus, failure.cause)
    }
    func servePreview(
        _ input: FileHandle, output: FileHandle, isStream: Bool, document: NSUUID,
        requestIdentity: UInt64, revision: UInt64, connectionGeneration: UInt64,
        maximumCachePayloadBytes: UInt64, archiveBytes: UInt64, maximumIndexedBytes: UInt64,
        maximumSourceBytes: UInt64, maximumInputTransferBytes: UInt64, inputChunkBytes: UInt32,
        maximumOutputTransferBytes: UInt64, outputChunkBytes: UInt32,
        withReply reply: @escaping PreviewReply
    ) {
        let scope: WorkerServiceSessionScope
        do {
            scope = try WorkerServiceSessionScope(
                document: document as UUID,
                connectionGeneration: connectionGeneration, maximumCachePayloadBytes: maximumCachePayloadBytes)
        } catch {
            rejectPreview(error, request: requestIdentity, reply: reply)
            return
        }
        guard input.fileDescriptor >= 0, output.fileDescriptor >= 0,
            input.fileDescriptor != output.fileDescriptor
        else {
            rejectPreview(
                WorkerServiceBootstrapError.invalidRequest("Preview requires separate valid channels"),
                request: requestIdentity, admission: .invalidChannels, reply: reply)
            return
        }
        var identity = Dust3DWorkerRequestIdentity()
        identity.document = scope.document.uuid
        identity.request = requestIdentity
        identity.revision = revision
        let job = WorkerPreviewChannelJob(
            expected: identity,
            input: input.fileDescriptor, output: output.fileDescriptor,
            context: Dust3DWorkerPreviewArchiveContext(
                archive_bytes: archiveBytes,
                maximum_source_bytes: maximumSourceBytes, maximum_indexed_bytes: maximumIndexedBytes),
            inputPolicy: Dust3DWorkerPreviewChunkPolicy(
                maximum_transfer_bytes: maximumInputTransferBytes,
                chunk_payload_bytes: inputChunkBytes),
            outputPolicy: Dust3DWorkerPreviewChunkPolicy(
                maximum_transfer_bytes: maximumOutputTransferBytes,
                chunk_payload_bytes: outputChunkBytes))
        let decision = scheduler.submit(
            owner: candidate, requestIdentity: requestIdentity,
            onCancelled: {
                self.rejectPreview(
                    WorkerServiceBootstrapError.invalidRequest("Preview withdrawn before start"),
                    request: requestIdentity, admission: .cancelledBeforeStart, reply: reply)
            },
            { lease in
                do {
                    let receipt = try withExtendedLifetime((input, output)) {
                        try self.session.serve(scope: scope, job: .preview(job))
                    }
                    guard case .preview(let result) = receipt else {
                        throw WorkerServiceBootstrapError.invalidRequest("Unexpected preview receipt profile")
                    }
                    let finalized = isStream ? shutdown(output.fileDescriptor, SHUT_WR) : 0
                    let finalizationError: Int32 = finalized == 0 ? 0 : errno
                    let admission: WorkerServiceAdmissionStatus =
                        self.scheduler.complete(owner: self.candidate, lease) ? .accepted : .staleCompletion
                    let cause = withUnsafeBytes(of: result.message) { Data($0.prefix { $0 != 0 }) }
                    reply(
                        result.status.rawValue, requestIdentity, result.system_error, result.io_stage.rawValue,
                        result.detail_source.rawValue, result.detail_code, result.message_truncated,
                        getpid(), self.sandboxEntitlementVerified, finalizationError, admission.rawValue, 0, cause)
                } catch {
                    let completed = self.scheduler.complete(owner: self.candidate, lease)
                    self.rejectPreview(
                        error, request: requestIdentity,
                        admission: completed ? nil : .staleCompletion, reply: reply)
                }
            })
        if case .accepted = decision { return }
        rejectPreview(
            WorkerServiceBootstrapError.invalidRequest("Preview admission rejected: \(decision.status.rawValue)"),
            request: requestIdentity, admission: decision.status, reply: reply)
    }
    func resetSession(
        _ document: NSUUID, requestIdentity: UInt64, connectionGeneration: UInt64,
        maximumCachePayloadBytes: UInt64, withReply reply: @escaping SessionReply
    ) {
        let scope: WorkerServiceSessionScope
        do {
            scope = try WorkerServiceSessionScope(
                document: document as UUID, connectionGeneration: connectionGeneration,
                maximumCachePayloadBytes: maximumCachePayloadBytes)
        } catch {
            rejectSession(error, request: requestIdentity, reply: reply)
            return
        }
        let decision = scheduler.submit(
            owner: candidate, requestIdentity: requestIdentity,
            onCancelled: {
                self.rejectSession(
                    WorkerServiceBootstrapError.invalidRequest("Session reset withdrawn before start"),
                    request: requestIdentity,
                    admission: .cancelledBeforeStart, reply: reply)
            },
            { lease in
                do {
                    // The coordinator rejects a foreign scope and never creates a
                    // session merely to reset it. Accepted generation drains first.
                    try self.session.reset(scope: scope)
                    let admission: WorkerServiceAdmissionStatus =
                        self.scheduler.complete(owner: self.candidate, lease) ? .accepted : .staleCompletion
                    reply(
                        DUST3D_WORKER_JOB_COMPLETE.rawValue, DUST3D_WORKER_OPERATION_UNKNOWN.rawValue, requestIdentity,
                        0, 0, 0, 0, 0, getpid(), self.sandboxEntitlementVerified, 0, admission.rawValue, Data())
                } catch {
                    let completed = self.scheduler.complete(owner: self.candidate, lease)
                    self.rejectSession(
                        error, request: requestIdentity, admission: completed ? nil : .staleCompletion, reply: reply)
                }
            })
        if case .accepted = decision { return }
        rejectSession(
            WorkerServiceBootstrapError.invalidRequest("Session reset admission rejected: \(decision.status.rawValue)"),
            request: requestIdentity, admission: decision.status, reply: reply)
    }
    func closeSession(_ requestIdentity: UInt64, withReply reply: @escaping SessionReply) {
        let decision = scheduler.submit(
            owner: candidate, requestIdentity: requestIdentity,
            onCancelled: {
                self.rejectSession(
                    WorkerServiceBootstrapError.invalidRequest("Session close withdrawn before start"),
                    request: requestIdentity,
                    admission: .cancelledBeforeStart, reply: reply)
            },
            { lease in
                do {
                    try self.session.close()
                    let admission: WorkerServiceAdmissionStatus =
                        self.scheduler.complete(owner: self.candidate, lease) ? .accepted : .staleCompletion
                    reply(
                        DUST3D_WORKER_JOB_COMPLETE.rawValue, DUST3D_WORKER_OPERATION_UNKNOWN.rawValue, requestIdentity,
                        0, 0, 0, 0, 0, getpid(), self.sandboxEntitlementVerified, 0, admission.rawValue, Data())
                } catch {
                    let completed = self.scheduler.complete(owner: self.candidate, lease)
                    self.rejectSession(
                        error, request: requestIdentity, admission: completed ? nil : .staleCompletion, reply: reply)
                }
            })
        if case .accepted = decision { return }
        rejectSession(
            WorkerServiceBootstrapError.invalidRequest("Session close admission rejected: \(decision.status.rawValue)"),
            request: requestIdentity, admission: decision.status, reply: reply)
    }
    func cancelQueued(_ requestIdentity: UInt64, withReply reply: @escaping @Sendable (UInt32) -> Void) {
        reply(scheduler.cancelQueued(owner: candidate, requestIdentity: requestIdentity).rawValue)
    }
    func finishWhenIdle() {
        // Seal admission immediately; normal stop is ordered after every accepted submission.
        scheduler.retire(owner: candidate, beforeRetired: { try self.session.close() })
    }
}
