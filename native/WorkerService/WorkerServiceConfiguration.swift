// SPDX-License-Identifier: MIT
import Foundation
import Security

/// Explicit service-side trust input. This value has no QA pins, default host or fallback policy.
public struct WorkerServiceConfiguration: Sendable {
    /// Signing requirement enforced by Foundation before an invocation can claim ownership.
    public let hostSigningRequirement: String
    /// Validate requirement syntax without authenticating a peer or adopting a distribution policy.
    /// - Parameter hostSigningRequirement: Complete requirement chosen by the embedding application.
    /// - Throws: `WorkerServiceBootstrapError` retaining the Security status on compilation failure.
    /// - Example: Supply your release host's designated requirement from sealed bundle configuration.
    public init(hostSigningRequirement: String) throws {
        guard !hostSigningRequirement.isEmpty else { throw WorkerServiceBootstrapError.emptyHostRequirement }
        var compiled: SecRequirement?
        let status = SecRequirementCreateWithString(hostSigningRequirement as CFString, [], &compiled)
        guard status == errSecSuccess, compiled != nil else {
            throw WorkerServiceBootstrapError.signingRequirement(status)
        }
        self.hostSigningRequirement = hostSigningRequirement
    }
}

/// Original preflight failures; no library error terminates the embedding process.
public enum WorkerServiceBootstrapError: Error, Sendable {
    case emptyHostRequirement
    case signingRequirement(OSStatus)
    case sandboxPreflight(String)
    case invalidRequest(String)
}
