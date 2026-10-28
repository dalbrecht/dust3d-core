// SPDX-License-Identifier: MIT
import Foundation

/// Exact-revision descriptor RPC contract. Scalar status domains remain profile-specific;
/// callbacks are original receipts, never independent peer, EOF or publication evidence.
@objc(Dust3DWorkerXPCProtocol)
public protocol Dust3DWorkerXPCProtocol {
    /// Authenticate/claim a connection candidate without descriptors, request IDs or native session creation.
    /// Reply preserves service PID, sandbox self-check and ownership-admission status; never job/drain proof.
    func handshake(withReply reply: @escaping @Sendable (Int32, Bool, UInt32, UInt64, UInt64) -> Void)
    func serve(
        _ input: FileHandle, output: FileHandle, isStream: Bool, requestIdentity: UInt64,
        withReply reply:
            @escaping @Sendable (
                UInt32, UInt32, UInt64, Int32, UInt32, UInt32, Int32, UInt8, Int32, Bool, Int32, UInt32
            ) ->
            Void)
    func serveSession(
        _ input: FileHandle, output: FileHandle, isStream: Bool, document: NSUUID,
        requestIdentity: UInt64, revision: UInt64, connectionGeneration: UInt64,
        maximumCachePayloadBytes: UInt64, maximumWireBytes: UInt64, maximumDecodedBytes: UInt64,
        maximumSourceBytes: UInt64,
        withReply reply:
            @escaping @Sendable (
                UInt32, UInt32, UInt64, Int32, UInt32, UInt32, Int32, UInt8, Int32, Bool, Int32, UInt32, Data
            ) -> Void)
    /// Explicit segmented profile. Reply status is PreviewSessionStatus, never legacy JobStatus.
    /// boundaryStatus separately retains a C session-creation decode failure, or zero.
    func servePreview(
        _ input: FileHandle, output: FileHandle, isStream: Bool, document: NSUUID,
        requestIdentity: UInt64, revision: UInt64, connectionGeneration: UInt64,
        maximumCachePayloadBytes: UInt64, archiveBytes: UInt64, maximumIndexedBytes: UInt64,
        maximumSourceBytes: UInt64, maximumInputTransferBytes: UInt64, inputChunkBytes: UInt32,
        maximumOutputTransferBytes: UInt64, outputChunkBytes: UInt32,
        withReply reply:
            @escaping @Sendable (
                UInt32, UInt64, Int32, UInt32, UInt32, Int32, UInt8, Int32, Bool, Int32, UInt32, UInt32, Data
            ) -> Void)
    func resetSession(
        _ document: NSUUID, requestIdentity: UInt64, connectionGeneration: UInt64, maximumCachePayloadBytes: UInt64,
        withReply reply:
            @escaping @Sendable (
                UInt32, UInt32, UInt64, Int32, UInt32, UInt32, Int32, UInt8, Int32, Bool, Int32, UInt32, Data
            ) -> Void)
    func closeSession(
        _ requestIdentity: UInt64,
        withReply reply:
            @escaping @Sendable (
                UInt32, UInt32, UInt64, Int32, UInt32, UInt32, Int32, UInt8, Int32, Bool, Int32, UInt32, Data
            ) -> Void)
    func cancelQueued(_ requestIdentity: UInt64, withReply reply: @escaping @Sendable (UInt32) -> Void)
}

/// Construct a fresh per-connection interface; no service identity or trust policy is installed.
public enum WorkerXPCContract {
    /// Caller separately configures code-signing requirements and connection lifetime.
    /// - Returns: Independent interface for the exact-revision worker protocol.
    public static func makeInterface() -> NSXPCInterface {
        NSXPCInterface(with: Dust3DWorkerXPCProtocol.self)
    }
}
