// SPDX-License-Identifier: MIT
import Dust3DWorkerClient
import Dust3DWorkerXPC
import Foundation

/// Immutable archive/control snapshot. Data must own its storage, never borrow mutable external memory.
public struct WorkerPreviewSource: Sendable {
    public let archive: Data
    public let options: WorkerXPCPreviewOptions
    /// Validate captured scalar controls before any transport or native parsing.
    /// - Parameters:
    ///   - archive: Independently owned opaque document bytes; not parsed here.
    ///   - options: Original cache/native/chunk settings, including raw zero/default policies.
    /// - Throws: WorkerPreviewExecutorError.invalidControls naming the invalid field.
    public init(archive: Data, options: WorkerXPCPreviewOptions) throws {
        // Data.count is nonnegative and every supported macOS size fits UInt64.
        try WorkerPreviewExecutionPlan.validate(byteCount: UInt64(archive.count), options: options)
        self.archive = archive
        self.options = options
    }
}

/// Exact-revision native metadata preflight; no allocation, descriptor, Security or codec calls.
enum WorkerPreviewExecutionPlan {
    static func validate(byteCount: UInt64, options: WorkerXPCPreviewOptions) throws {
        // Mirrors checkedPreviewArchiveContext/measurePreviewChunking and service scope.
        // Preserve native addressable indexed/transfer budgets, never legacy count caps.
        let maximumArchiveBytes: UInt64 = 256 * 1024 * 1024
        guard byteCount > 0, byteCount <= maximumArchiveBytes, byteCount == options.archiveBytes else {
            throw WorkerPreviewExecutorError.invalidControls("archiveBytes")
        }
        guard options.maximumSourceBytes <= maximumArchiveBytes else {
            throw WorkerPreviewExecutorError.invalidControls("maximumSourceBytes")
        }
        guard options.maximumCachePayloadBytes <= maximumArchiveBytes else {
            throw WorkerPreviewExecutorError.invalidControls("maximumCachePayloadBytes")
        }
        guard options.maximumIndexedBytes <= UInt64(UInt.max) else {
            throw WorkerPreviewExecutorError.invalidControls("maximumIndexedBytes")
        }
        // Archive bound above proves adding the fixed 24-byte source prefix cannot overflow.
        guard options.maximumInputTransferBytes >= byteCount + 24,
            options.maximumInputTransferBytes <= UInt64(UInt.max),
            options.inputChunkBytes > 0, options.inputChunkBytes <= 65536
        else {
            throw WorkerPreviewExecutorError.invalidControls("inputPolicy")
        }
        guard options.maximumOutputTransferBytes > 0, options.maximumOutputTransferBytes <= UInt64(UInt.max),
            options.outputChunkBytes > 0, options.outputChunkBytes <= 65536
        else {
            throw WorkerPreviewExecutorError.invalidControls("outputPolicy")
        }
    }
}

/// Parent-side setup/lifecycle errors; transport/native errors retain their original types.
public enum WorkerPreviewExecutorError: Error {
    case invalidControls(String)
    case busy, retired, foreignScope, replayed, uninitialized, missingPhase
    case handshake(WorkerXPCHandshakeReceipt)
    case handshakeBarrier(expected: WorkerXPCHandshakeReceipt, received: WorkerXPCHandshakeReceipt)
    case exitObservation(expected: WorkerXPCHandshakeReceipt, received: WorkerXPCExitObservation)
    case peerReceipt(expected: WorkerXPCHandshakeReceipt, received: WorkerPreviewControlReceipt)
    case resetReceipt(WorkerXPCLegacyReceipt)
    case phases([WorkerPreviewPhaseFailure])
}
