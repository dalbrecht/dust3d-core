// SPDX-License-Identifier: MIT
import Dust3DWorkerClient
import Foundation

/// One connection lifetime with actor-owned mutable Foundation state. Async calls retain
/// borrowed channel objects until a control result. Control completion is not EOF/process/
/// source admission: callers independently drain channels and complete their source owner.
/// Task cancellation never interrupts native work, and no automatic reconnect is performed.
public actor WorkerXPCConnection {
    private let channel: any WorkerXPCChannel
    private var state = WorkerXPCRequestState()
    private var isResumed = false
    private var pendingFailure: (@Sendable (any Error) -> Void)?
    private var pendingHandles: [FileHandle] = []
    /// Create an unresumed connection with explicit host-selected service and trust policy.
    public init(configuration: WorkerXPCConfiguration) { channel = FoundationWorkerXPCChannel(configuration) }
    init(factory: @Sendable () -> any WorkerXPCChannel) { channel = factory() }
    /// Bind an authenticated connection lifetime before creating job channels or native work.
    /// - Parameter generation: Positive host-selected generation, fixed for this connection.
    /// - Returns: Original service fields plus independently observed OS peer and captured generation.
    ///   Retain this value separately if later control communication fails.
    /// - Throws: Original transport error or handshakeRejected retaining the original receipt.
    /// A repeated handshake is ownership-only: no native admission lease/request ID is consumed.
    /// It never proves job/channel cleanup, process exit or native-idle state.
    public func handshake(generation: UInt64) async throws -> WorkerXPCHandshakeReceipt {
        try await withCheckedThrowingContinuation { continuation in
            let outcome = WorkerXPCReply(continuation)
            do {
                guard let (proxy, token) = try prepare(outcome: outcome, generation: generation) else { return }
                proxy.handshake { [weak self] process, sandbox, admission in
                    Task {
                        guard let self else {
                            outcome.finish(.failure(WorkerXPCTransportError.retired))
                            return
                        }
                        await self.finishHandshake(
                            token: token, process: process, sandbox: sandbox,
                            admission: admission, generation: generation, outcome: outcome)
                    }
                }
            } catch { outcome.finish(.failure(error)) }
        }
    }
    private func finishHandshake(
        token: UInt64, process: Int32, sandbox: Bool, admission: UInt32,
        generation: UInt64, outcome: WorkerXPCReply<WorkerXPCHandshakeReceipt>
    ) {
        guard let observed = admitCallback(token: token, cause: Data()) else { return }
        let receipt = WorkerXPCHandshakeReceipt(
            process: process, sandboxEntitlementVerified: sandbox,
            admissionStatus: admission, connectionGeneration: generation,
            observedProcess: observed)
        guard process == observed, sandbox, admission == 0 else {
            state.retire()
            channel.invalidate()
            outcome.finish(.failure(WorkerXPCTransportError.handshakeRejected(receipt)))
            return
        }
        outcome.finish(.success(receipt))
    }
    private func validateChannels(_ input: FileHandle, _ output: FileHandle) throws {
        guard input.fileDescriptor >= 0, output.fileDescriptor >= 0,
            input.fileDescriptor != output.fileDescriptor
        else { throw WorkerXPCTransportError.invalidChannels }
    }
    private func prepare<Value: Sendable>(
        outcome: WorkerXPCReply<Value>, generation: UInt64,
        handles: [FileHandle] = []
    ) throws -> ((any Dust3DWorkerXPCProtocol), UInt64)? {
        let token = try state.begin(generation: generation)
        pendingFailure = { outcome.finish(.failure($0)) }
        pendingHandles = handles
        if !isResumed {
            channel.installHandlers(
                invalidated: { [weak self] in
                    Task { await self?.disconnected(WorkerXPCTransportError.connectionInvalidated) }
                },
                interrupted: { [weak self] in
                    Task { await self?.disconnected(WorkerXPCTransportError.connectionInterrupted) }
                })
            channel.resume()
            isResumed = true
        }
        guard
            let proxy = channel.proxy(errorHandler: { [weak self] error in
                Task { await self?.fail(token, error: WorkerXPCTransportError.remote(error)) }
            })
        else {
            fail(token, error: WorkerXPCTransportError.missingProxy)
            return nil
        }
        return (proxy, token)
    }
    private func disconnected(_ error: any Error) {
        state.retire()
        let failure = pendingFailure
        pendingFailure = nil
        pendingHandles = []
        failure?(error)
    }
    private func fail(_ token: UInt64, error: any Error) {
        guard state.activeToken == token else { return }
        disconnected(error)
    }
    private func admitCallback(token: UInt64, cause: Data) -> Int32? {
        guard state.activeToken == token else { return nil }
        do {
            guard cause.count <= 511 else { throw WorkerXPCTransportError.diagnosticExceeded(actual: cause.count) }
            let observed = channel.processIdentifier
            try state.observePeer(observed)
            _ = state.finish(token)
            pendingFailure = nil
            pendingHandles = []
            return observed
        } catch {
            fail(token, error: error)
            return nil
        }
    }
    /// Submit one legacy descriptor job; caller retains and independently drains both channels.
    /// - Parameters: input/output are separate valid borrowed handles; isStream selects service half-close;
    ///   request is the positive host request ID and generation is the fixed positive connection lifetime.
    /// - Returns: Original legacy receipt plus independent OS peer observation, not semantic admission.
    /// - Throws: Original transport/lifecycle error. Task cancellation does not cancel native work.
    public func submit(
        input: FileHandle, output: FileHandle, isStream: Bool, request: UInt64,
        generation: UInt64
    ) async throws -> WorkerXPCLegacyReceipt {
        guard request > 0 else { throw WorkerXPCTransportError.invalidRequest }
        try validateChannels(input, output)
        return try await withCheckedThrowingContinuation { continuation in
            let outcome = WorkerXPCReply(continuation)
            do {
                guard
                    let (proxy, token) = try prepare(
                        outcome: outcome, generation: generation, handles: [input, output])
                else { return }
                proxy.serve(input, output: output, isStream: isStream, requestIdentity: request) {
                    [weak self]
                    status, operation, request, systemError, stage, source, code, truncated, process,
                    sandboxEntitlementVerified, finalizationError, admissionStatus in
                    Task {
                        guard let self else {
                            outcome.finish(.failure(WorkerXPCTransportError.retired))
                            return
                        }
                        await self.finish(
                            token: token,
                            status: status, operation: operation, request: request, systemError: systemError,
                            stage: stage, source: source, code: code, truncated: truncated, process: process,
                            sandboxEntitlementVerified: sandboxEntitlementVerified,
                            finalizationError: finalizationError,
                            admissionStatus: admissionStatus, generation: generation, outcome: outcome)
                    }
                }
            } catch { outcome.finish(.failure(error)) }
        }
    }
    /// Submit an indexed/source session job on this same persistent connection.
    /// - Parameters: input/output remain caller-owned; identity is host-captured and options preserve raw quotas.
    /// - Returns: Original legacy session control receipt; caller separately validates frame/EOF/current source.
    /// - Throws: Original transport/lifecycle error; a failed callback does not prove native drain.
    public func submitSession(
        input: FileHandle, output: FileHandle, identity: WorkerPreviewIdentity,
        options: WorkerXPCSessionOptions
    ) async throws -> WorkerXPCLegacyReceipt {
        try validateChannels(input, output)
        let generation = identity.generation
        return try await withCheckedThrowingContinuation { continuation in
            let outcome = WorkerXPCReply(continuation)
            do {
                guard
                    let (proxy, token) = try prepare(
                        outcome: outcome, generation: generation, handles: [input, output])
                else { return }
                proxy.serveSession(
                    input, output: output, isStream: false, document: identity.document as NSUUID,
                    requestIdentity: identity.request, revision: identity.revision, connectionGeneration: generation,
                    maximumCachePayloadBytes: options.maximumCachePayloadBytes,
                    maximumWireBytes: options.maximumWireBytes,
                    maximumDecodedBytes: options.maximumDecodedBytes, maximumSourceBytes: options.maximumSourceBytes
                ) {
                    [weak self]
                    status, operation, request, systemError, stage, source, code, truncated, process,
                    sandbox, finalizationError, admissionStatus, cause in
                    Task {
                        guard let self else {
                            outcome.finish(.failure(WorkerXPCTransportError.retired))
                            return
                        }
                        await self.finish(
                            token: token,
                            status: status, operation: operation, request: request, systemError: systemError,
                            stage: stage, source: source, code: code, truncated: truncated, process: process,
                            sandboxEntitlementVerified: sandbox, finalizationError: finalizationError,
                            admissionStatus: admissionStatus, generation: generation, outcome: outcome, cause: cause)
                    }
                }
            } catch { outcome.finish(.failure(error)) }
        }
    }
    /// Submit a segmented preview with original native/chunk policies, without a legacy envelope.
    /// - Parameters: input/output are borrowed until control completion; identity/options are frozen by the host.
    /// - Returns: Original dedicated preview receipt, including separate boundary classification.
    /// - Throws: Original transport/lifecycle error; caller still owns channel cleanup and source retirement.
    public func submitPreview(
        input: FileHandle, output: FileHandle, identity: WorkerPreviewIdentity,
        options: WorkerXPCPreviewOptions
    ) async throws -> WorkerPreviewControlReceipt {
        try validateChannels(input, output)
        return try await withCheckedThrowingContinuation { continuation in
            let outcome = WorkerXPCReply(continuation)
            do {
                guard
                    let (proxy, token) = try prepare(
                        outcome: outcome, generation: identity.generation, handles: [input, output])
                else { return }
                proxy.servePreview(
                    input, output: output, isStream: false, document: identity.document as NSUUID,
                    requestIdentity: identity.request, revision: identity.revision,
                    connectionGeneration: identity.generation,
                    maximumCachePayloadBytes: options.maximumCachePayloadBytes, archiveBytes: options.archiveBytes,
                    maximumIndexedBytes: options.maximumIndexedBytes, maximumSourceBytes: options.maximumSourceBytes,
                    maximumInputTransferBytes: options.maximumInputTransferBytes,
                    inputChunkBytes: options.inputChunkBytes,
                    maximumOutputTransferBytes: options.maximumOutputTransferBytes,
                    outputChunkBytes: options.outputChunkBytes
                ) {
                    [weak self]
                    status, request, systemError, stage, source, code, truncated, process,
                    sandbox, finalizationError, admissionStatus, boundaryStatus, cause in
                    Task {
                        guard let self else {
                            outcome.finish(.failure(WorkerXPCTransportError.retired))
                            return
                        }
                        await self.finishPreview(
                            token: token,
                            status: status, request: request, systemError: systemError, stage: stage,
                            source: source, code: code, truncated: truncated, process: process,
                            sandbox: sandbox, finalizationError: finalizationError, admissionStatus: admissionStatus,
                            boundaryStatus: boundaryStatus, generation: identity.generation, cause: cause,
                            outcome: outcome)
                    }
                }
            } catch { outcome.finish(.failure(error)) }
        }
    }
    private func finishPreview(
        token: UInt64,
        status: UInt32, request: UInt64, systemError: Int32, stage: UInt32, source: UInt32,
        code: Int32, truncated: UInt8, process: Int32, sandbox: Bool, finalizationError: Int32,
        admissionStatus: UInt32, boundaryStatus: UInt32, generation: UInt64, cause: Data,
        outcome: WorkerXPCReply<WorkerPreviewControlReceipt>
    ) {
        guard let observed = admitCallback(token: token, cause: cause) else { return }
        outcome.finish(
            .success(
                WorkerPreviewControlReceipt(
                    status: status, request: request,
                    systemError: systemError, stage: stage, source: source, code: code, truncated: truncated,
                    process: process, sandboxEntitlementVerified: sandbox, finalizationError: finalizationError,
                    admissionStatus: admissionStatus, boundaryStatus: boundaryStatus, connectionGeneration: generation,
                    observedProcess: observed, cause: cause)))
    }
    /// Reset an already initialized native cache after earlier work drains; never create a cache implicitly.
    /// - Parameters: document/request/generation are host correlation; maximumCachePayloadBytes preserves scope.
    /// - Returns: Original legacy control receipt; failure remains an explicit service outcome.
    /// - Throws: Original local transport/lifecycle error, separate from service status.
    public func resetSession(
        document: UUID, request: UInt64, generation: UInt64, maximumCachePayloadBytes: UInt64
    ) async throws -> WorkerXPCLegacyReceipt {
        var documentBytes = document.uuid
        guard request > 0, withUnsafeBytes(of: &documentBytes, { $0.contains { $0 != 0 } }) else {
            throw WorkerXPCTransportError.invalidRequest
        }
        return try await withCheckedThrowingContinuation { continuation in
            let outcome = WorkerXPCReply(continuation)
            do {
                guard let (proxy, token) = try prepare(outcome: outcome, generation: generation) else { return }
                proxy.resetSession(
                    document as NSUUID, requestIdentity: request, connectionGeneration: generation,
                    maximumCachePayloadBytes: maximumCachePayloadBytes
                ) {
                    [weak self]
                    status, operation, request, systemError, stage, source, code, truncated, process,
                    sandbox, finalizationError, admissionStatus, cause in
                    Task {
                        guard let self else {
                            outcome.finish(.failure(WorkerXPCTransportError.retired))
                            return
                        }
                        await self.finish(
                            token: token,
                            status: status, operation: operation, request: request, systemError: systemError,
                            stage: stage, source: source, code: code, truncated: truncated, process: process,
                            sandboxEntitlementVerified: sandbox, finalizationError: finalizationError,
                            admissionStatus: admissionStatus, generation: generation, outcome: outcome, cause: cause)
                    }
                }
            } catch { outcome.finish(.failure(error)) }
        }
    }
    /// Ask the service to close its session after earlier work; connection invalidation remains separate.
    /// - Parameters: request is positive and generation is the already bound host connection lifetime.
    /// - Returns: Original legacy close receipt, not evidence of process disappearance.
    /// - Throws: Original transport/lifecycle error. No force termination or native interruption occurs.
    public func closeSession(request: UInt64, generation: UInt64) async throws -> WorkerXPCLegacyReceipt {
        guard request > 0 else { throw WorkerXPCTransportError.invalidRequest }
        return try await withCheckedThrowingContinuation { continuation in
            let outcome = WorkerXPCReply(continuation)
            do {
                guard let (proxy, token) = try prepare(outcome: outcome, generation: generation) else { return }
                proxy.closeSession(request) {
                    [weak self]
                    status, operation, request, systemError, stage, source, code, truncated, process,
                    sandbox, finalizationError, admissionStatus, cause in
                    Task {
                        guard let self else {
                            outcome.finish(.failure(WorkerXPCTransportError.retired))
                            return
                        }
                        await self.finish(
                            token: token,
                            status: status, operation: operation, request: request, systemError: systemError,
                            stage: stage, source: source, code: code, truncated: truncated, process: process,
                            sandboxEntitlementVerified: sandbox, finalizationError: finalizationError,
                            admissionStatus: admissionStatus, generation: generation, outcome: outcome, cause: cause)
                    }
                }
            } catch { outcome.finish(.failure(error)) }
        }
    }
    private func finish(
        token: UInt64,
        status: UInt32, operation: UInt32, request: UInt64, systemError: Int32, stage: UInt32,
        source: UInt32, code: Int32, truncated: UInt8, process: Int32, sandboxEntitlementVerified: Bool,
        finalizationError: Int32, admissionStatus: UInt32, generation: UInt64,
        outcome: WorkerXPCReply<WorkerXPCLegacyReceipt>, cause: Data = Data()
    ) {
        guard let observed = admitCallback(token: token, cause: cause) else { return }
        outcome.finish(
            .success(
                WorkerXPCLegacyReceipt(
                    status: status, operation: operation, request: request, systemError: systemError,
                    stage: stage, source: source, code: code, truncated: truncated, process: process,
                    sandboxEntitlementVerified: sandboxEntitlementVerified, finalizationError: finalizationError,
                    admissionStatus: admissionStatus, connectionGeneration: generation,
                    observedProcess: observed, cause: cause)))
    }
    /// Retire only an idle RPC connection. Caller has separately drained native channels.
    /// - Returns: Independent OS peer PID; this does not establish process disappearance.
    /// - Throws: busy while a control request remains pending.
    public func invalidate() throws -> Int32 {
        try state.close()
        let observed = channel.processIdentifier
        channel.invalidate()
        return observed
    }
}
