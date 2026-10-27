// SPDX-License-Identifier: MIT
import Darwin
import Dust3DWorkerRuntime
import Foundation
import Security

private func checkedSandboxEntitlement() throws -> Bool {
    guard let task = SecTaskCreateFromSelf(nil) else {
        throw StaticFixtureError.invalid("Could not obtain service SecTask")
    }
    var failure: Unmanaged<CFError>?
    let value = SecTaskCopyValueForEntitlement(task, "com.apple.security.app-sandbox" as CFString, &failure)
    if let cause = failure?.takeRetainedValue() {
        let domain = CFErrorGetDomain(cause).map { $0 as String } ?? "Unavailable CFError domain"
        throw StaticFixtureError.invalid(
            "Service entitlement query failed: domain=\(domain) code=\(CFErrorGetCode(cause)) cause=\(cause)"
        )
    }
    guard let value, CFGetTypeID(value) == CFBooleanGetTypeID(), (value as? Bool) == true else {
        throw StaticFixtureError.invalid("Service must have the App Sandbox entitlement before native work")
    }
    return true
}

final class StaticWorkerObject: NSObject, StaticWorkerJob, Sendable {
    private let sandboxEntitlementVerified: Bool
    private let scheduler: WorkerServiceScheduler
    private let candidate: WorkerServiceConnection
    private let session = WorkerServiceSession(backend: WorkerNativeServiceSession())
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
                let result = dust3d_worker_serve(input.fileDescriptor, output.fileDescriptor)
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
                StaticFixtureError.invalid("Session requires separate valid channels"), request: requestIdentity,
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
                    StaticFixtureError.invalid("Session job withdrawn before start"), request: requestIdentity,
                    admission: .cancelledBeforeStart, reply: reply)
            },
            { lease in
                do {
                    // NSXPC owns the received handles only for this invocation. Keep
                    // both owners alive until the queued native call and EOF complete.
                    let result = try withExtendedLifetime((input, output)) {
                        try self.session.serve(scope: scope, job: job)
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
            StaticFixtureError.invalid("Session admission rejected: \(decision.status.rawValue)"),
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
                    StaticFixtureError.invalid("Session reset withdrawn before start"), request: requestIdentity,
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
            StaticFixtureError.invalid("Session reset admission rejected: \(decision.status.rawValue)"),
            request: requestIdentity, admission: decision.status, reply: reply)
    }
    func closeSession(_ requestIdentity: UInt64, withReply reply: @escaping SessionReply) {
        let decision = scheduler.submit(
            owner: candidate, requestIdentity: requestIdentity,
            onCancelled: {
                self.rejectSession(
                    StaticFixtureError.invalid("Session close withdrawn before start"), request: requestIdentity,
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
            StaticFixtureError.invalid("Session close admission rejected: \(decision.status.rawValue)"),
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

final class StaticListenerDelegate: NSObject, NSXPCListenerDelegate {
    private let requirement: String
    private let scheduler = WorkerServiceScheduler(onRetired: { result in
        switch result {
        case .success: exit(EXIT_SUCCESS)
        case .failure(let cause):
            // The callback retains the original typed cause; logs include only
            // metadata because native diagnostic bytes can contain document text.
            let description: String
            if let original = cause as? WorkerSessionBoundaryError {
                description =
                    "status=\(original.status) source=\(original.source) code=\(original.code) truncated=\(original.isTruncated)"
            } else {
                description = "type=\(String(reflecting: type(of: cause)))"
            }
            FileHandle.standardError.write(Data("Static XPC retirement failed: \(description)\n".utf8))
            exit(EXIT_FAILURE)
        }
    })
    private let sandboxEntitlementVerified: Bool
    init(requirement: String, sandboxEntitlementVerified: Bool) {
        self.requirement = requirement
        self.sandboxEntitlementVerified = sandboxEntitlementVerified
    }
    func listener(_ listener: NSXPCListener, shouldAcceptNewConnection connection: NSXPCConnection) -> Bool {
        guard connection.effectiveUserIdentifier == getuid() else { return false }
        connection.setCodeSigningRequirement(requirement)
        guard let candidate = scheduler.makeConnectionCandidate() else { return false }
        // A candidate reserves no owner; only a requirement-checked invocation may claim it.
        let worker = StaticWorkerObject(
            sandboxEntitlementVerified: sandboxEntitlementVerified,
            scheduler: scheduler, candidate: candidate)
        connection.exportedInterface = NSXPCInterface(with: StaticWorkerJob.self)
        connection.exportedObject = worker
        connection.invalidationHandler = { worker.finishWhenIdle() }
        connection.resume()
        return true
    }
}

@main
struct WorkerXPCService {
    static func main() {
        do {
            let sandboxEntitlementVerified = try checkedSandboxEntitlement()
            let pins = try readDevelopmentPins()
            let delegate = StaticListenerDelegate(
                requirement: try developmentRequirement(pins.host),
                sandboxEntitlementVerified: sandboxEntitlementVerified)
            let listener = NSXPCListener.service()
            listener.delegate = delegate
            withExtendedLifetime(delegate) { listener.resume() }
        } catch {
            FileHandle.standardError.write(Data("Static XPC service preflight failed: \(error)\n".utf8))
            exit(EXIT_FAILURE)
        }
    }
}
