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
    init(sandboxEntitlementVerified: Bool, scheduler: WorkerServiceScheduler, candidate: WorkerServiceConnection) {
        self.sandboxEntitlementVerified = sandboxEntitlementVerified
        self.scheduler = scheduler
        self.candidate = candidate
    }
    func serve(
        _ input: FileHandle, output: FileHandle, isStream: Bool,
        withReply reply:
            @escaping @Sendable (
                UInt32, UInt32, UInt64, Int32, UInt32, UInt32, Int32, UInt8, Int32, Bool, Int32, UInt32
            ) ->
            Void
    ) {
        let decision = scheduler.submit(
            owner: candidate,
            onCancelled: {
                reply(
                    DUST3D_WORKER_JOB_INTERNAL_FAILED.rawValue, DUST3D_WORKER_OPERATION_UNKNOWN.rawValue,
                    0, 0, 0, 0, 0, 0, getpid(), self.sandboxEntitlementVerified, 0,
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
    func finishWhenIdle() {
        // Seal admission immediately; normal stop is ordered after every accepted submission.
        scheduler.retire(owner: candidate)
    }
}

final class StaticListenerDelegate: NSObject, NSXPCListenerDelegate {
    private let requirement: String
    private let scheduler = WorkerServiceScheduler(onRetired: { exit(EXIT_SUCCESS) })
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
