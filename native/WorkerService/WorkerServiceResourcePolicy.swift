// SPDX-License-Identifier: MIT

/// Requested units only. No value represents OS infinity; representability/inherited caps require the native planner.
public struct WorkerServiceResourceBudget: Equatable, Sendable {
    public let soft: UInt64
    public let hard: UInt64
    /// Preserve exact caller-selected values without conversion, normalization or OS operations.
    public init(soft: UInt64, hard: UInt64) {
        self.soft = soft
        self.hard = hard
    }
}

/// Units are cumulative process CPU seconds, regular-file bytes and descriptor-number ceiling.
public enum WorkerServiceResourceKind: Equatable, Sendable { case cpuSeconds, fileBytes, descriptors }

/// Original pure configuration cause. No policy failure applies partial process limits.
public enum WorkerServiceResourcePolicyError: Error {
    case field(String)
    case budget(resource: WorkerServiceResourceKind, requested: WorkerServiceResourceBudget)
    case descriptorFloor(minimum: UInt64, requested: WorkerServiceResourceBudget)
}

/// Trusted process-lifetime requests, not document quotas, per-job deadlines or a memory/RSS guarantee.
/// Creating/importing this value never inspects or mutates the app/test runner's process limits.
public struct WorkerServiceResourcePolicy: Equatable, Sendable {
    public let cpuSeconds: WorkerServiceResourceBudget
    public let fileBytes: WorkerServiceResourceBudget
    public let descriptors: WorkerServiceResourceBudget
    public let minimumDescriptorCeiling: UInt64
    public let coreBytes: WorkerServiceResourceBudget
    /// Validate ordered positive requests and an explicit startup descriptor floor before any OS boundary.
    /// - Parameters:
    ///   - cpuSeconds: Cumulative process CPU limits; never reset or interpreted as per-job wall time.
    ///   - fileBytes: Per-regular-file limits in bytes, separate from memory and channel budgets.
    ///   - descriptors: Highest admissible descriptor number plus one, not a count of open files.
    ///   - minimumDescriptorCeiling: Trusted startup requirement; native inherited caps must also satisfy it.
    /// - Throws: Original typed budget/floor error. Platform conversion/setup remains a separate fail-closed boundary.
    /// Core dumps are always disabled; no optional address-space or resident-memory cap is invented.
    public init(
        cpuSeconds: WorkerServiceResourceBudget, fileBytes: WorkerServiceResourceBudget,
        descriptors: WorkerServiceResourceBudget, minimumDescriptorCeiling: UInt64
    ) throws {
        for (kind, request): (WorkerServiceResourceKind, WorkerServiceResourceBudget) in [
            (.cpuSeconds, cpuSeconds), (.fileBytes, fileBytes), (.descriptors, descriptors),
        ] {
            guard request.soft > 0, request.soft <= request.hard else {
                throw WorkerServiceResourcePolicyError.budget(resource: kind, requested: request)
            }
        }
        guard minimumDescriptorCeiling > 0, minimumDescriptorCeiling <= descriptors.soft else {
            throw WorkerServiceResourcePolicyError.descriptorFloor(
                minimum: minimumDescriptorCeiling, requested: descriptors)
        }
        self.cpuSeconds = cpuSeconds
        self.fileBytes = fileBytes
        self.descriptors = descriptors
        self.minimumDescriptorCeiling = minimumDescriptorCeiling
        coreBytes = WorkerServiceResourceBudget(soft: 0, hard: 0)
    }
    private static let fieldNames: Set<String> = [
        "cpuSoftSeconds", "cpuHardSeconds", "fileSoftBytes", "fileHardBytes",
        "descriptorSoftCeiling", "descriptorHardCeiling", "minimumDescriptorCeiling",
    ]
    /// Decode exact canonical ASCII decimal requests from trusted own-bundle configuration.
    /// - Parameter fields: Exactly the seven documented unit-qualified keys, with string values.
    /// - Throws: field for shape/noncanonical/overflow values, or the original typed budget/floor cause.
    /// This is pure metadata validation; no Security, filesystem, native planner or resource calls occur.
    public init(fields: [String: String]) throws {
        guard Set(fields.keys) == Self.fieldNames else { throw WorkerServiceResourcePolicyError.field("keys") }
        func number(_ key: String) throws -> UInt64 {
            guard let text = fields[key], !text.isEmpty, text.utf8.count <= 20,
                text.utf8.allSatisfy({ (48...57).contains($0) }),
                text == "0" || text.utf8.first != 48, let value = UInt64(text)
            else { throw WorkerServiceResourcePolicyError.field(key) }
            return value
        }
        try self.init(
            cpuSeconds: WorkerServiceResourceBudget(soft: number("cpuSoftSeconds"), hard: number("cpuHardSeconds")),
            fileBytes: WorkerServiceResourceBudget(soft: number("fileSoftBytes"), hard: number("fileHardBytes")),
            descriptors: WorkerServiceResourceBudget(
                soft: number("descriptorSoftCeiling"), hard: number("descriptorHardCeiling")),
            minimumDescriptorCeiling: number("minimumDescriptorCeiling"))
    }
    /// Canonical unit-qualified own-bundle values. No platform limits or infinity sentinel are substituted.
    public var fields: [String: String] {
        [
            "cpuSoftSeconds": String(cpuSeconds.soft), "cpuHardSeconds": String(cpuSeconds.hard),
            "fileSoftBytes": String(fileBytes.soft), "fileHardBytes": String(fileBytes.hard),
            "descriptorSoftCeiling": String(descriptors.soft), "descriptorHardCeiling": String(descriptors.hard),
            "minimumDescriptorCeiling": String(minimumDescriptorCeiling),
        ]
    }

}
