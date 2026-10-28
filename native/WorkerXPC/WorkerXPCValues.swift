// SPDX-License-Identifier: MIT
import Foundation

/// Original legacy control receipt plus independently captured host generation/OS peer.
/// Receiving this value does not establish EOF, current source or publication admission.
public struct WorkerXPCLegacyReceipt: Sendable {
    public let status: UInt32
    public let operation: UInt32
    public let request: UInt64
    public let systemError: Int32
    public let stage: UInt32
    public let source: UInt32
    public let code: Int32
    public let truncated: UInt8
    public let process: Int32
    public let sandboxEntitlementVerified: Bool
    public let finalizationError: Int32
    public let admissionStatus: UInt32
    public let connectionGeneration: UInt64
    public let observedProcess: Int32
    public let cause: Data
}

/// Passive original legacy session quotas. No default or normalization is applied here.
public struct WorkerXPCSessionOptions: Sendable {
    public let maximumCachePayloadBytes: UInt64
    public let maximumWireBytes: UInt64
    public let maximumDecodedBytes: UInt64
    public let maximumSourceBytes: UInt64
    public init(
        maximumCachePayloadBytes: UInt64, maximumWireBytes: UInt64,
        maximumDecodedBytes: UInt64, maximumSourceBytes: UInt64
    ) {
        self.maximumCachePayloadBytes = maximumCachePayloadBytes
        self.maximumWireBytes = maximumWireBytes
        self.maximumDecodedBytes = maximumDecodedBytes
        self.maximumSourceBytes = maximumSourceBytes
    }
}
/// Passive original native controls and bounded chunk policies; no C engine dependency.
public struct WorkerXPCPreviewOptions: Sendable {
    public let maximumCachePayloadBytes: UInt64
    public let archiveBytes: UInt64
    public let maximumIndexedBytes: UInt64
    public let maximumSourceBytes: UInt64
    public let maximumInputTransferBytes: UInt64
    public let inputChunkBytes: UInt32
    public let maximumOutputTransferBytes: UInt64
    public let outputChunkBytes: UInt32
    public init(
        maximumCachePayloadBytes: UInt64, archiveBytes: UInt64,
        maximumIndexedBytes: UInt64, maximumSourceBytes: UInt64,
        maximumInputTransferBytes: UInt64, inputChunkBytes: UInt32,
        maximumOutputTransferBytes: UInt64, outputChunkBytes: UInt32
    ) {
        self.maximumCachePayloadBytes = maximumCachePayloadBytes
        self.archiveBytes = archiveBytes
        self.maximumIndexedBytes = maximumIndexedBytes
        self.maximumSourceBytes = maximumSourceBytes
        self.maximumInputTransferBytes = maximumInputTransferBytes
        self.inputChunkBytes = inputChunkBytes
        self.maximumOutputTransferBytes = maximumOutputTransferBytes
        self.outputChunkBytes = outputChunkBytes
    }
}

/// Service-process incarnation for correlation after authenticated transport, never an authorization credential.
/// Zero is retained for original rejection receipts but cannot authenticate a lifetime.
public struct WorkerXPCServiceInstance: Equatable, Sendable {
    public let high: UInt64
    public let low: UInt64
    /// Preserve the original scalar process incarnation without normalization.
    public init(high: UInt64, low: UInt64) {
        self.high = high
        self.low = low
    }
    public var isValid: Bool { high != 0 || low != 0 }
}

/// Captured authenticated lifetime before job channels. Retain independently after control failure.
/// This does not establish native/job completion, EOF, process disappearance or release trust policy.
public struct WorkerXPCHandshakeReceipt: Sendable {
    public let instance: WorkerXPCServiceInstance
    public let process: Int32
    public let sandboxEntitlementVerified: Bool
    public let admissionStatus: UInt32
    public let connectionGeneration: UInt64
    public let observedProcess: Int32
}
