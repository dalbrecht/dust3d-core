// SPDX-License-Identifier: MIT
import Darwin
import Dust3DWorkerXPC
import Foundation
import Security

private func checkedSandboxEntitlement() throws -> Bool {
    guard let task = SecTaskCreateFromSelf(nil) else {
        throw WorkerServiceBootstrapError.sandboxPreflight("Could not obtain service SecTask")
    }
    var failure: Unmanaged<CFError>?
    let value = SecTaskCopyValueForEntitlement(task, "com.apple.security.app-sandbox" as CFString, &failure)
    if let cause = failure?.takeRetainedValue() {
        let domain = CFErrorGetDomain(cause).map { $0 as String } ?? "Unavailable CFError domain"
        throw WorkerServiceBootstrapError.sandboxPreflight(
            "Service entitlement query failed: domain=\(domain) code=\(CFErrorGetCode(cause)) cause=\(cause)"
        )
    }
    guard let value, CFGetTypeID(value) == CFBooleanGetTypeID(), (value as? Bool) == true else {
        throw WorkerServiceBootstrapError.sandboxPreflight(
            "Service must have the App Sandbox entitlement before native work")
    }
    return true
}

public final class WorkerServiceListenerDelegate: NSObject, NSXPCListenerDelegate {
    private let requirement: String
    private let scheduler: WorkerServiceScheduler
    private let sandboxEntitlementVerified: Bool
    /// Verify this process's App Sandbox entitlement before accepting any connection.
    /// - Parameters:
    ///   - configuration: Explicit, syntax-validated host signing requirement.
    ///   - onRetired: Called after admitted work and native-owner cleanup drain.
    ///     The embedding executable owns logging and normal process termination.
    /// - Throws: `WorkerServiceBootstrapError` if sandbox verification fails.
    public init(
        configuration: WorkerServiceConfiguration,
        onRetired: @escaping @Sendable (Result<Void, any Error>) -> Void
    ) throws {
        sandboxEntitlementVerified = try checkedSandboxEntitlement()
        requirement = configuration.hostSigningRequirement
        scheduler = WorkerServiceScheduler(onRetired: onRetired)
        super.init()
    }
    public func listener(_ listener: NSXPCListener, shouldAcceptNewConnection connection: NSXPCConnection) -> Bool {
        guard connection.effectiveUserIdentifier == getuid() else { return false }
        connection.setCodeSigningRequirement(requirement)
        guard let candidate = scheduler.makeConnectionCandidate() else { return false }
        // A candidate reserves no owner; only a requirement-checked invocation may claim it.
        let worker = WorkerServiceObject(
            sandboxEntitlementVerified: sandboxEntitlementVerified,
            scheduler: scheduler, candidate: candidate)
        connection.exportedInterface = WorkerXPCContract.makeInterface()
        connection.exportedObject = worker
        connection.invalidationHandler = { worker.finishWhenIdle() }
        connection.resume()
        return true
    }
}
