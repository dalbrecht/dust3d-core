// SPDX-License-Identifier: MIT
import Foundation

/// Copied configuration from the service's own sealed bundle. No filesystem, Security or native calls.
public struct WorkerServiceBundleMetadata: Sendable {
    /// Exact reverse-domain identifier, also used as the staged XPC bundle's basename.
    public let serviceIdentifier: String
    /// Explicit host requirement; compilation and authentication remain separate boundaries.
    public let hostSigningRequirement: String
    /// Explicit lifetime caps copied from sealed own-bundle metadata; decoding has no OS effects.
    public let resourcePolicy: WorkerServiceResourcePolicy
    /// Explicit per-admitted-job wall policy, independent of cumulative process CPU limits.
    public let jobBudget: WorkerServiceJobBudget
    /// Validate the supported bundle layout and configuration schema before service setup.
    /// - Parameters:
    ///   - bundleURL: Current executable bundle URL, never a caller-selected trust override.
    ///   - infoDictionary: Current bundle's Info.plist values.
    /// - Throws: `WorkerServiceBundleError` naming only the invalid metadata field.
    /// - Example: `try WorkerServiceBundleMetadata(bundleURL: Bundle.main.bundleURL, infoDictionary: info)`.
    public init(bundleURL: URL, infoDictionary: [String: Any]) throws {
        func field(_ key: String) throws -> String {
            guard let value = infoDictionary[key] as? String else { throw WorkerServiceBundleError.field(key) }
            return value
        }
        let identifier = try field("CFBundleIdentifier")
        let segments = identifier.split(separator: ".", omittingEmptySubsequences: false)
        guard identifier.utf8.count <= 255, segments.count >= 2, segments.allSatisfy({ !$0.isEmpty }),
            identifier.utf8.allSatisfy({
                (48...57).contains($0) || (65...90).contains($0) || (97...122).contains($0) || $0 == 45 || $0 == 46
            })
        else { throw WorkerServiceBundleError.field("CFBundleIdentifier") }
        guard bundleURL.isFileURL, bundleURL.lastPathComponent == "\(identifier).xpc" else {
            throw WorkerServiceBundleError.field("bundleURL")
        }
        for (key, expected) in [
            ("CFBundleExecutable", "dust3d-xpc-service"), ("CFBundlePackageType", "XPC!"),
            ("CFBundleInfoDictionaryVersion", "6.0"), ("Dust3DWorkerConfigurationVersion", "3"),
        ] {
            guard try field(key) == expected else { throw WorkerServiceBundleError.field(key) }
        }
        let version = try field("CFBundleVersion")
        let versionComponents = version.split(separator: ".", omittingEmptySubsequences: false)
        guard version.utf8.count <= 64,
            versionComponents.allSatisfy({ !$0.isEmpty && $0.utf8.allSatisfy { (48...57).contains($0) } })
        else { throw WorkerServiceBundleError.field("CFBundleVersion") }
        guard let service = infoDictionary["XPCService"] as? [String: Any],
            service["ServiceType"] as? String == "Application", service["RunLoopType"] as? String == "dispatch_main"
        else { throw WorkerServiceBundleError.field("XPCService") }
        let requirement = try field("Dust3DWorkerHostRequirement")
        guard !requirement.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty,
            requirement.utf8.count <= 4096, !requirement.utf8.contains(0)
        else { throw WorkerServiceBundleError.field("Dust3DWorkerHostRequirement") }
        guard let resourceFields = infoDictionary["Dust3DWorkerResourcePolicy"] as? [String: String] else {
            throw WorkerServiceBundleError.field("Dust3DWorkerResourcePolicy")
        }
        let wall = try field("Dust3DWorkerJobWallMilliseconds")
        guard !wall.isEmpty, wall.utf8.count <= 7, wall.utf8.allSatisfy({ (48...57).contains($0) }),
            wall.utf8.first != 48, let milliseconds = UInt64(wall)
        else { throw WorkerServiceBundleError.field("Dust3DWorkerJobWallMilliseconds") }
        jobBudget = try WorkerServiceJobBudget(milliseconds: milliseconds)
        resourcePolicy = try WorkerServiceResourcePolicy(fields: resourceFields)
        serviceIdentifier = identifier
        hostSigningRequirement = requirement
    }
}

/// A metadata-field error never includes the signing expression or document data.
public enum WorkerServiceBundleError: Error, Sendable {
    case field(String)
}
