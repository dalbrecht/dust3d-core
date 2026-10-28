// SPDX-License-Identifier: MIT
import Dust3DWorkerClient
import Dust3DWorkerXPC
import Foundation

protocol WorkerPreviewTransport: Sendable {
    func handshake(generation: UInt64) async throws -> WorkerXPCHandshakeReceipt
    func submitPreview(
        input: FileHandle, output: FileHandle, isStream: Bool, identity: WorkerPreviewIdentity,
        options: WorkerXPCPreviewOptions
    ) async throws -> WorkerPreviewControlReceipt
    func resetSession(
        document: UUID, request: UInt64, generation: UInt64,
        maximumCachePayloadBytes: UInt64
    ) async throws -> WorkerXPCLegacyReceipt
    func invalidate() async throws -> Int32
}
extension WorkerXPCConnection: WorkerPreviewTransport {}

protocol WorkerPreviewExitWatching: Sendable {
    func register(timeoutMilliseconds: UInt64) async throws
    func waitForExit(timeoutMilliseconds: UInt64) async throws -> WorkerXPCExitObservation
}
extension WorkerXPCExitObserver: WorkerPreviewExitWatching {}

/// Internal resource-owning phases permit fake orchestration tests without real descriptors/native parsing.
protocol WorkerPreviewJob<Value>: Sendable {
    associatedtype Value: Sendable
    func write() async throws
    func read() async throws -> Value
    func control() async throws -> WorkerPreviewControlReceipt
}

/// One original phase failure. Diagnostic bytes stay opaque inside the original typed error.
public struct WorkerPreviewPhaseFailure: Sendable {
    public enum Phase: Int, Sendable { case preparation, writing, reading, control, retirement }
    public let phase: Phase
    public let cause: any Error
}

/// Invalidation is a cleanup request, never proof of native drain or process disappearance.
public struct WorkerPreviewRetirementObservation: Sendable {
    public let peer: WorkerXPCHandshakeReceipt?
    public let invalidationRequested: Bool
}
