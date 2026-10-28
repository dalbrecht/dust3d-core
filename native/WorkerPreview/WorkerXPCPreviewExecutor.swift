// SPDX-License-Identifier: MIT
import Dust3DWorkerClient
import Dust3DWorkerXPC
import Foundation

/// One document/lifetime parent executor. Failed lifetimes never reconnect automatically.
/// Publication/current-source admission remains the pipeline's responsibility.
public actor WorkerXPCPreviewExecutor<Value: Sendable>: WorkerPreviewExecuting {
    public typealias Source = WorkerPreviewSource
    typealias Factory =
        @Sendable (WorkerPreviewIdentity, Source, any WorkerPreviewTransport) throws -> any WorkerPreviewJob<Value>
    private struct Scope: Equatable {
        let document: UUID
        let generation: UInt64
        let cache: UInt64
    }
    private enum Phase: Sendable {
        case write(Result<Void, any Error>)
        case read(Result<Value, any Error>)
        case control(Result<WorkerPreviewControlReceipt, any Error>)
    }
    private let transport: any WorkerPreviewTransport
    private let makeJob: Factory
    private var scope: Scope?
    private var lastRequest: UInt64 = 0
    private var invalidationAttempted = false
    private var invalidationRequested = false
    private var retirementFailure: (any Error)?
    public private(set) var activeIdentity: WorkerPreviewIdentity?
    public private(set) var peerIdentity: WorkerXPCHandshakeReceipt?
    public private(set) var isRetired = false

    /// Create an unresumed executor with explicit service trust and a synchronous copied-value adapter.
    /// - Parameters:
    ///   - configuration: Trusted service identifier/signing requirement; no development defaults.
    ///   - copy: Runs on the dedicated reader worker with a borrowed immutable C session-output owner.
    ///     Copy every required value before returning; never retain/destroy the owner or expose borrowed views.
    /// No job channels are created until authenticated peer handshake completes.
    public init(configuration: WorkerXPCConfiguration, copy: @escaping @Sendable (OpaquePointer) throws -> Value) {
        transport = WorkerXPCConnection(configuration: configuration)
        makeJob = { identity, source, transport in
            try WorkerPreviewNativeJob(identity: identity, source: source, transport: transport, copy: copy)
        }
    }
    init(transport: any WorkerPreviewTransport, makeJob: @escaping Factory) {
        self.transport = transport
        self.makeJob = makeJob
    }
    private func begin(_ identity: WorkerPreviewIdentity, cache: UInt64) throws {
        guard !isRetired else { throw WorkerPreviewExecutorError.retired }
        guard activeIdentity == nil else { throw WorkerPreviewExecutorError.busy }
        let captured = Scope(document: identity.document, generation: identity.generation, cache: cache)
        if let scope, scope != captured { throw WorkerPreviewExecutorError.foreignScope }
        guard identity.request > lastRequest else { throw WorkerPreviewExecutorError.replayed }
        scope = captured
        lastRequest = identity.request
        activeIdentity = identity
    }
    private func peer(generation: UInt64) async throws -> WorkerXPCHandshakeReceipt {
        if let peerIdentity { return peerIdentity }
        let receipt = try await transport.handshake(generation: generation)
        guard receipt.connectionGeneration == generation, receipt.observedProcess > 0,
            receipt.process == receipt.observedProcess,
            receipt.observedProcess != ProcessInfo.processInfo.processIdentifier,
            receipt.sandboxEntitlementVerified, receipt.admissionStatus == 0
        else {
            throw WorkerPreviewExecutorError.handshake(receipt)
        }
        peerIdentity = receipt
        return receipt
    }
    private func requestInvalidation() async -> (any Error)? {
        isRetired = true
        if invalidationAttempted { return retirementFailure }
        invalidationAttempted = true
        do {
            _ = try await transport.invalidate()
            invalidationRequested = true
        } catch { retirementFailure = error }
        return retirementFailure
    }
    /// Execute one snapshot, joining all owned phases even on error/caller cancellation.
    /// - Parameters: identity is host-captured; source retains independently owned archive and raw policies.
    /// - Returns: Copied value plus actual producer observations, not UI publication permission.
    /// - Throws: Typed setup/lifetime error or all original phase/cleanup causes after owned joins.
    /// Remote cleanup/process exit is independent; a failed retired executor cannot launch more jobs.
    public func execute(identity: WorkerPreviewIdentity, source: Source) async throws
        -> WorkerPreviewExecutionCompletion<Value>
    {
        try begin(identity, cache: source.options.maximumCachePayloadBytes)
        defer { activeIdentity = nil }
        let captured: WorkerXPCHandshakeReceipt
        let job: any WorkerPreviewJob<Value>
        do {
            captured = try await peer(generation: identity.generation)
            job = try makeJob(identity, source, transport)
        } catch {
            var failures = [WorkerPreviewPhaseFailure(phase: .preparation, cause: error)]
            if let cleanup = await requestInvalidation() { failures.append(.init(phase: .retirement, cause: cleanup)) }
            throw WorkerPreviewExecutorError.phases(failures)
        }
        let joined: Result<WorkerPreviewExecutionCompletion<Value>, any Error> = await withTaskGroup(of: Phase.self) {
            group in
            group.addTask {
                do {
                    try await job.write()
                    return .write(.success(()))
                } catch { return .write(.failure(error)) }
            }
            group.addTask {
                do { return .read(.success(try await job.read())) } catch { return .read(.failure(error)) }
            }
            group.addTask {
                do { return .control(.success(try await job.control())) } catch { return .control(.failure(error)) }
            }
            var written = false
            var read: Result<Value, any Error>?
            var receipt: WorkerPreviewControlReceipt?
            var controlFinished = false
            var failures: [WorkerPreviewPhaseFailure] = []
            for await phase in group {
                switch phase {
                case .write(.success): written = true
                case .write(.failure(let error)): failures.append(.init(phase: .writing, cause: error))
                case .read(let result):
                    read = result
                    if case .failure(let error) = result { failures.append(.init(phase: .reading, cause: error)) }
                case .control(let result):
                    controlFinished = true
                    do {
                        let original = try result.get()
                        try original.validateControl(identity: identity)
                        guard original.observedProcess == captured.observedProcess else {
                            throw WorkerPreviewExecutorError.peerReceipt(expected: captured, received: original)
                        }
                        receipt = original
                    } catch { failures.append(.init(phase: .control, cause: error)) }
                }
                // Invalidate only after the RPC returned; never interrupt a pending native invocation.
                // Keep waiting for writer/reader completion; control failure is not remote-drain proof.
                if !failures.isEmpty {
                    isRetired = true
                    if controlFinished, !invalidationAttempted, let cleanup = await requestInvalidation() {
                        failures.append(.init(phase: .retirement, cause: cleanup))
                    }
                }
            }
            if !failures.isEmpty {
                return .failure(
                    WorkerPreviewExecutorError.phases(failures.sorted { $0.phase.rawValue < $1.phase.rawValue }))
            }
            guard written, let read, let receipt else { return .failure(WorkerPreviewExecutorError.missingPhase) }
            return read.map {
                WorkerPreviewExecutionCompletion(
                    value: $0, receipt: receipt,
                    requestWritten: true, reachedEOF: true)
            }
        }
        return try joined.get()
    }
    /// Reset the bound native cache only after earlier owned phase joins.
    /// - Parameter request: New positive monotonic host request; no generation/scope replacement.
    /// - Returns: Original validated legacy reset receipt (not preview status numbering).
    /// - Throws: Original receipt/transport/retirement causes. Control failure does not prove remote drain.
    public func reset(request: UInt64) async throws -> WorkerXPCLegacyReceipt {
        guard let scope else { throw WorkerPreviewExecutorError.uninitialized }
        let identity = try WorkerPreviewIdentity(
            document: scope.document, request: request, revision: 0, generation: scope.generation)
        try begin(identity, cache: scope.cache)
        defer { activeIdentity = nil }
        do {
            let receipt = try await transport.resetSession(
                document: scope.document, request: request,
                generation: scope.generation, maximumCachePayloadBytes: scope.cache)
            guard let peerIdentity, receipt.process == peerIdentity.observedProcess,
                receipt.observedProcess == peerIdentity.observedProcess,
                receipt.connectionGeneration == scope.generation,
                receipt.status == 0, receipt.operation == 0, receipt.request == request,
                receipt.systemError == 0, receipt.stage == 0, receipt.source == 0, receipt.code == 0,
                receipt.truncated == 0, receipt.cause.isEmpty, receipt.sandboxEntitlementVerified,
                receipt.admissionStatus == 0, receipt.finalizationError == 0
            else {
                throw WorkerPreviewExecutorError.resetReceipt(receipt)
            }
            return receipt
        } catch {
            var failures = [WorkerPreviewPhaseFailure(phase: .control, cause: error)]
            if let cleanup = await requestInvalidation() { failures.append(.init(phase: .retirement, cause: cleanup)) }
            throw WorkerPreviewExecutorError.phases(failures)
        }
    }
    /// Request normal owner invalidation only when no operation is active. Idempotent, no automatic retry.
    /// - Returns: Captured peer/request observation; caller separately establishes actual process disappearance.
    /// - Throws: busy or original retirement cause. This method never claims bounded native/process completion.
    public func retire() async throws -> WorkerPreviewRetirementObservation {
        guard activeIdentity == nil else { throw WorkerPreviewExecutorError.busy }
        if let error = await requestInvalidation() {
            throw WorkerPreviewExecutorError.phases([.init(phase: .retirement, cause: error)])
        }
        return WorkerPreviewRetirementObservation(peer: peerIdentity, invalidationRequested: invalidationRequested)
    }
}
