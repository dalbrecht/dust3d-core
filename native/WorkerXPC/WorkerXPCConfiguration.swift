// SPDX-License-Identifier: MIT
import Foundation
import Security

/// Explicit trusted host configuration, never inferred from an archive or development default.
/// Compiling a requirement validates its syntax, not its distribution policy or a remote peer.
public struct WorkerXPCConfiguration: Sendable {
    /// Exact embedded service identifier; no QA identifier is supplied by this library.
    public let serviceIdentifier: String
    /// Original caller-selected requirement; no signing identity or broad fallback is installed.
    public let signingRequirement: String
    /// Validate local configuration before creating or resuming an XPC connection.
    /// - Parameters:
    ///   - serviceIdentifier: Nonempty reverse-domain identifier using ASCII letters, digits, dot and hyphen.
    ///   - signingRequirement: Trusted host-selected code requirement, compiled unchanged by Security.
    /// - Throws: WorkerXPCTransportError with the original Security status on compilation failure.
    public init(serviceIdentifier: String, signingRequirement: String) throws {
        let segments = serviceIdentifier.split(separator: ".", omittingEmptySubsequences: false)
        guard segments.count >= 2, segments.allSatisfy({ !$0.isEmpty }),
            serviceIdentifier.utf8.allSatisfy({
                (48...57).contains($0) || (65...90).contains($0) || (97...122).contains($0) || $0 == 45 || $0 == 46
            })
        else { throw WorkerXPCTransportError.invalidServiceIdentifier }
        guard !signingRequirement.isEmpty else { throw WorkerXPCTransportError.emptySigningRequirement }
        var requirement: SecRequirement?
        let status = SecRequirementCreateWithString(signingRequirement as CFString, [], &requirement)
        guard status == errSecSuccess, requirement != nil else {
            throw WorkerXPCTransportError.signingRequirement(status: status)
        }
        self.serviceIdentifier = serviceIdentifier
        self.signingRequirement = signingRequirement
    }
}
