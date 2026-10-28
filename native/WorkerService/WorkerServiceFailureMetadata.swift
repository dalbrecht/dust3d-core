// SPDX-License-Identifier: MIT
import Foundation

/// Log-safe classification retaining original native status/source/code without document diagnostic bytes.
public struct WorkerServiceFailureMetadata: Sendable {
    /// Metadata-only text suitable for the thin executable's stderr log.
    public let description: String
    /// Copy typed error metadata without changing or consuming the original callback error.
    /// - Parameter cause: Original preflight or retirement error.
    /// - Example: `WorkerServiceFailureMetadata(error).description`.
    public init(_ cause: any Error) {
        if let original = cause as? WorkerSessionBoundaryError {
            description =
                "status=\(original.status) source=\(original.source) code=\(original.code) truncated=\(original.isTruncated)"
        } else if let original = cause as? WorkerServiceResourceBootstrapError {
            let observation: WorkerServiceResourceObservation
            let prefix: String
            switch original {
            case .setup(let value):
                observation = value
                prefix = ""
            case .receipt(let field, let value):
                observation = value
                prefix = "resourceReceiptField=\(field) "
            }
            let resource = observation.resource.map(String.init) ?? "none"
            description =
                prefix + "resourceStatus=\(observation.status) operation=\(observation.operation)"
                + " runtime=\(observation.runtime) resource=\(resource) code=\(observation.detailCode)"
                + " errno=\(observation.systemError) started=\(observation.applicationStarted)"
                + " applied=\(observation.appliedCount) verified=\(observation.verifiedCount)"
                + " truncated=\(observation.messageTruncated)"
        } else if let original = cause as? WorkerServiceResourcePolicyError {
            switch original {
            case .field(let field): description = "resourcePolicyField=\(field)"
            case .budget(let resource, let request):
                description = "resourcePolicyBudget=\(resource) soft=\(request.soft) hard=\(request.hard)"
            case .descriptorFloor(let minimum, let request):
                description = "resourcePolicyDescriptorFloor=\(minimum) soft=\(request.soft) hard=\(request.hard)"
            }
        } else if let original = cause as? WorkerServiceBundleError {
            switch original {
            case .field(let key): description = "bundleField=\(key)"
            }
        } else if let original = cause as? WorkerServiceBootstrapError {
            switch original {
            case .signingRequirement(let status): description = "signingRequirementStatus=\(status)"
            case .emptyHostRequirement: description = "emptyHostRequirement"
            case .sandboxPreflight(let message): description = "sandboxPreflight=\(message)"
            case .invalidRequest: description = "invalidRequest"
            }
        } else {
            description = "type=\(String(reflecting: type(of: cause)))"
        }
    }
}
