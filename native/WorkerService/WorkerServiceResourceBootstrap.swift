// SPDX-License-Identifier: MIT
import Dust3DWorkerRuntime

extension WorkerServiceResourcePolicy {
    /// Exact scalar transfer only; constructing the profile never inspects or changes process limits.
    var nativeProfile: Dust3DWorkerResourceProfile {
        Dust3DWorkerResourceProfile(
            abi_version: UInt32(DUST3D_WORKER_RESOURCE_ABI_VERSION),
            cpu_seconds: Dust3DWorkerResourceBudget(soft: cpuSeconds.soft, hard: cpuSeconds.hard),
            file_bytes: Dust3DWorkerResourceBudget(soft: fileBytes.soft, hard: fileBytes.hard),
            descriptors: Dust3DWorkerResourceBudget(soft: descriptors.soft, hard: descriptors.hard),
            minimum_descriptor_ceiling: minimumDescriptorCeiling)
    }
}

/// Copied original setup diagnostics. Counts describe partial effects; they never imply rollback.
public struct WorkerServiceResourceObservation: Sendable {
    public let abiVersion: UInt32
    public let status: UInt32
    public let operation: UInt32
    public let runtime: UInt32
    public let resource: UInt32?
    public let detailCode: Int32
    public let systemError: Int32
    public let applicationStarted: Bool
    public let appliedCount: UInt32
    public let verifiedCount: UInt32
    /// Original bounded bytes, retained even when diagnostic text is not UTF-8.
    public let messageBytes: [UInt8]
    public let message: String
    public let messageTruncated: Bool

    init(_ result: Dust3DWorkerResourceResult) {
        abiVersion = result.abi_version
        status = result.status.rawValue
        operation = result.operation.rawValue
        runtime = result.runtime
        resource = result.has_detail_resource == 0 ? nil : result.detail_resource
        detailCode = result.detail_code
        systemError = result.system_error
        applicationStarted = result.application_started != 0
        appliedCount = result.applied_count
        verifiedCount = result.verified_count
        messageBytes = withUnsafeBytes(of: result.message) { bytes in Array(bytes.prefix { $0 != 0 }) }
        message = String(decoding: messageBytes, as: UTF8.self)
        messageTruncated = result.message_truncated != 0
    }
}

/// Setup failure or inconsistent success, retaining the original observation in either case.
public enum WorkerServiceResourceBootstrapError: Error, Sendable {
    case setup(WorkerServiceResourceObservation)
    case receipt(field: String, observation: WorkerServiceResourceObservation)
}

/// A verified process-lifetime setup receipt. There is no public initializer or fallback receipt.
/// This proves resource application/readback only, not sandboxing, a memory cap or eventual process exit.
public struct WorkerServicePreparedResources: Sendable {
    public let cpuSeconds: WorkerServiceResourceBudget
    public let fileBytes: WorkerServiceResourceBudget
    public let coreBytes: WorkerServiceResourceBudget
    public let descriptors: WorkerServiceResourceBudget

    init(result: Dust3DWorkerResourceResult, policy: WorkerServiceResourcePolicy) throws {
        let observation = WorkerServiceResourceObservation(result)
        guard result.status == DUST3D_WORKER_RESOURCE_READY else {
            throw WorkerServiceResourceBootstrapError.setup(observation)
        }
        func require(_ predicate: Bool, _ field: String) throws {
            guard predicate else {
                throw WorkerServiceResourceBootstrapError.receipt(field: field, observation: observation)
            }
        }
        try require(result.abi_version == DUST3D_WORKER_RESOURCE_ABI_VERSION, "abiVersion")
        try require(result.runtime == 1 || result.runtime == 2, "runtime")
        try require(
            result.application_started == 1 && result.applied_count == 4 && result.verified_count == 4, "progress")
        try require(
            result.operation == DUST3D_WORKER_RESOURCE_OPERATION_NONE && result.has_detail_resource == 0
                && result.detail_code == 0 && result.system_error == 0 && observation.message.isEmpty
                && result.message_truncated == 0, "diagnostics")
        func checked(_ actual: Dust3DWorkerResourceBudget, _ requested: WorkerServiceResourceBudget, _ field: String)
            throws -> WorkerServiceResourceBudget
        {
            try require(
                actual.soft > 0 && actual.soft <= actual.hard && actual.soft <= requested.soft
                    && actual.hard <= requested.hard, field)
            return WorkerServiceResourceBudget(soft: actual.soft, hard: actual.hard)
        }
        cpuSeconds = try checked(result.effective.0, policy.cpuSeconds, "cpuSeconds")
        fileBytes = try checked(result.effective.1, policy.fileBytes, "fileBytes")
        try require(result.effective.2.soft == 0 && result.effective.2.hard == 0, "coreBytes")
        coreBytes = WorkerServiceResourceBudget(soft: 0, hard: 0)
        descriptors = try checked(result.effective.3, policy.descriptors, "descriptors")
        try require(descriptors.soft >= policy.minimumDescriptorCeiling, "minimumDescriptorCeiling")
    }

    /// MUTATES the current process's limits. Call exclusively in a fresh dedicated helper before listener/native work.
    /// Never call from the host app or test runner. Failure may leave partial caps; report it and exit normally.
    /// - Parameter policy: Explicit trusted own-bundle requests; no document, environment or argument fallback.
    /// - Returns: Verified effective CPU/file/core/descriptor limits; stricter inherited limits remain in force.
    /// - Throws: Original native cause/progress, or an inconsistent-receipt error. Never retry or assume rollback.
    /// - Example: The bundle-only executable prepares resources before constructing its listener delegate.
    public static func prepareFreshHelper(policy: WorkerServiceResourcePolicy) throws -> Self {
        try Self(result: dust3d_worker_resources_apply(policy.nativeProfile), policy: policy)
    }
}
