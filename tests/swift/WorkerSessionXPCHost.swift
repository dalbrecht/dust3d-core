// SPDX-License-Identifier: MIT
import Darwin
import Dust3DWorkerClient
import Dust3DWorkerRuntime
import Foundation

private struct SessionXPCSnapshot {
    let indices: [UInt32]
    let positions: [Float]
    let rebuilt: UInt64
    let reused: UInt64
    let retained: UInt64
}
private func copySessionXPCOutput(_ owner: OpaquePointer) throws -> SessionXPCSnapshot {
    guard dust3d_worker_session_output_summary(owner).outcome == DUST3D_WORKER_SESSION_SOURCED,
        let primary = dust3d_worker_session_output_primary(owner)
    else { throw StaticFixtureError.invalid("Expected ordinary sourced Capsule output") }
    let summary = dust3d_worker_decoded_summary(primary)
    let count = dust3d_worker_decoded_index_count(primary)
    guard count == 240, summary.vertex_count > 0, summary.vertex_count <= 240,
        dust3d_worker_decoded_source_summary(primary).corner_count == count
    else { throw StaticFixtureError.invalid("Capsule indexed/source correspondence") }
    var vertices = [Dust3DWorkerVertex](repeating: Dust3DWorkerVertex(), count: summary.vertex_count)
    var indices = [UInt32](repeating: 0, count: count)
    guard
        vertices.withUnsafeMutableBufferPointer({ dust3d_worker_decoded_vertices(primary, 0, $0.count, $0.baseAddress) }
        ) == 1,
        indices.withUnsafeMutableBufferPointer({ dust3d_worker_decoded_indices(primary, 0, $0.count, $0.baseAddress) })
            == 1
    else { throw StaticFixtureError.invalid("Owned indexed copies") }
    let positions = vertices.flatMap { vertex in
        withUnsafeBytes(of: vertex.position) { Array($0.bindMemory(to: Float.self)) }
    }
    guard positions.allSatisfy(\.isFinite), indices.allSatisfy({ $0 < summary.vertex_count }) else {
        throw StaticFixtureError.invalid("Finite indexed output")
    }
    var statistics = Dust3DWorkerSessionStatistics()
    guard dust3d_worker_session_output_statistics(owner, &statistics, MemoryLayout.size(ofValue: statistics)) == 1,
        statistics.maximum_cache_payload_bytes == 64 * 1024 * 1024,
        dust3d_worker_session_output_source_issue(owner) == nil,
        dust3d_worker_session_output_statistics_issue(owner) == nil
    else { throw StaticFixtureError.invalid("Actual session observations") }
    return SessionXPCSnapshot(
        indices: indices, positions: positions, rebuilt: statistics.rebuilt_components,
        reused: statistics.reused_components,
        retained: statistics.retained_payload_bytes)
}
private struct PreviewSourceSnapshot: Sendable {
    let archive: Data
    let context: Dust3DWorkerPreviewArchiveContext
    let inputPolicy: Dust3DWorkerPreviewChunkPolicy
    let outputPolicy: Dust3DWorkerPreviewChunkPolicy
    let maximumCachePayloadBytes: UInt64
}
private func segmentedSessionXPC(
    peer: WorkerXPCPeer, queue: WorkerPreviewSourceCoordinator<PreviewSourceSnapshot>, archive: Data, root: URL,
    process: Int32,
    deadline: DispatchTime
) throws -> SessionXPCSnapshot {
    let captured = try WorkerPreviewIdentity(
        document: UUID(uuid: (1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)),
        request: 2, revision: 7, generation: 9)
    let source = PreviewSourceSnapshot(
        archive: archive,
        context: Dust3DWorkerPreviewArchiveContext(
            archive_bytes: UInt64(archive.count),
            maximum_source_bytes: 0, maximum_indexed_bytes: 0),
        inputPolicy: Dust3DWorkerPreviewChunkPolicy(
            maximum_transfer_bytes: UInt64(archive.count + 24), chunk_payload_bytes: 64),
        outputPolicy: Dust3DWorkerPreviewChunkPolicy(
            maximum_transfer_bytes: 64 * 1024 * 1024, chunk_payload_bytes: 256),
        maximumCachePayloadBytes: 64 * 1024 * 1024)
    try queue.submit(captured, source: source)
    guard queue.begin() == captured else { throw StaticFixtureError.invalid("Preview queue must freeze this identity") }
    do {
        var scope = Dust3DWorkerPreviewSessionScope()
        scope.identity.document = captured.document.uuid
        scope.identity.request = captured.request
        scope.identity.revision = captured.revision
        scope.connection_generation = captured.generation
        let snapshot = try queue.source(for: captured)
        let context = snapshot.context
        let inputPolicy = snapshot.inputPolicy
        let outputPolicy = snapshot.outputPolicy
        let inputURL = root.appendingPathComponent("preview-request-2")
        let outputURL = root.appendingPathComponent("preview-reply-2")
        try Data().write(to: inputURL)
        try Data().write(to: outputURL)
        let writer = try FileHandle(forWritingTo: inputURL)
        defer { try? writer.close() }
        let encoded = snapshot.archive.withUnsafeBytes {
            dust3d_worker_preview_archive_write(
                writer.fileDescriptor, scope, context,
                $0.bindMemory(to: UInt8.self).baseAddress, $0.count, inputPolicy)
        }
        guard encoded.status == DUST3D_WORKER_PREVIEW_ARCHIVE_OK else {
            throw WorkerPreviewTransferFailure(
                operation: .archive, status: encoded.status.rawValue,
                source: 0, code: encoded.detail_code, systemError: encoded.system_error,
                stage: encoded.io_stage.rawValue,
                truncated: encoded.message_truncated,
                cause: withUnsafeBytes(of: encoded.message) { Data($0.prefix { $0 != 0 }) })
        }
        try writer.close()
        let input = try FileHandle(forReadingFrom: inputURL)
        let output = try FileHandle(forWritingTo: outputURL)
        defer {
            try? input.close()
            try? output.close()
        }
        let outcome = WorkerOutcome<WorkerPreviewControlReceipt>()
        Task.detached {
            do {
                try await peer.submitPreview(
                    input: input, output: output, identity: captured,
                    maximumCachePayloadBytes: snapshot.maximumCachePayloadBytes, context: context,
                    inputPolicy: inputPolicy, outputPolicy: outputPolicy, outcome: outcome)
            } catch { outcome.finish(.failure(error)) }
        }
        let receipt = try outcome.wait(until: deadline)
        guard receipt.status == 0, receipt.boundaryStatus == 0, receipt.admissionStatus == 0 else {
            throw WorkerPreviewAdmissionError.receipt(receipt)
        }
        guard receipt.observedProcess == process, process > 0, process != getpid() else {
            throw StaticFixtureError.invalid("Preview must retain the independently observed legacy peer")
        }
        let reader = try FileHandle(forReadingFrom: outputURL)
        defer { try? reader.close() }
        var decoded = Dust3DWorkerPreviewSessionResult()
        guard
            let owner = dust3d_worker_preview_session_read(
                reader.fileDescriptor, scope, context, outputPolicy, &decoded)
        else {
            throw WorkerPreviewTransferFailure(
                operation: .reply, status: decoded.status.rawValue,
                source: decoded.detail_source.rawValue, code: decoded.detail_code, systemError: decoded.system_error,
                stage: decoded.io_stage.rawValue, truncated: decoded.message_truncated,
                cause: withUnsafeBytes(of: decoded.message) { Data($0.prefix { $0 != 0 }) })
        }
        defer { dust3d_worker_session_output_destroy(owner) }
        guard decoded.status == DUST3D_WORKER_PREVIEW_SESSION_OK else {
            throw StaticFixtureError.invalid("Segmented reply must finish exact framing and actual EOF")
        }
        // The immutable fixture source remains current. A production coordinator must supply
        // its latest source here; callback correlation cannot establish current document state.
        try reader.close()
        try input.close()
        try output.close()
        try queue.complete(captured, receipt: receipt, reachedEOF: true)
        print("Segmented XPC preview admitted after actual EOF and independent OS peer observation")
        return try copySessionXPCOutput(owner)
    } catch {
        let primary: Result<SessionXPCSnapshot, any Error> = .failure(error)
        let cleanup = Result<Void, any Error> {
            if queue.activeIdentity == captured { try queue.abandon(captured) }
        }
        return try resolveWorkerChannelResults(operation: primary, closing: cleanup)
    }
}
func runSessionXPCFixture(archive: Data, root: URL, requirement: String, deadline: DispatchTime) throws {
    let generation: UInt64 = 9
    let wire = Dust3DWorkerWireLimits(
        maximum_input_bytes: 1_048_576, maximum_output_bytes: 1_048_576, maximum_failure_bytes: 4096)
    let policy = Dust3DWorkerSessionOutputLimits(
        maximum_wire_bytes: 1_048_576, maximum_decoded_bytes: 1_048_576, maximum_source_bytes: 0)
    let queue = try WorkerServiceQueue(limits: wire)
    let previewQueue = WorkerPreviewSourceCoordinator<PreviewSourceSnapshot>()
    var snapshots: [SessionXPCSnapshot] = []
    var observedProcess: Int32 = 0
    try withWorkerXPCPeer(requirement: requirement, deadline: deadline) { peer in
        for requestID: UInt64 in [1, 2, 4] {
            if requestID == 4 {
                let reset = WorkerOutcome<WorkerControlReceipt>()
                Task.detached {
                    do {
                        try await peer.resetSession(
                            document: UUID(uuid: (1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)),
                            request: 3, generation: generation, maximumCachePayloadBytes: 64 * 1024 * 1024,
                            outcome: reset)
                    } catch { reset.finish(.failure(error)) }
                }
                let receipt = try reset.wait(until: deadline)
                guard receipt.status == 0, receipt.admissionStatus == 0, receipt.request == 3,
                    receipt.process == observedProcess, receipt.observedProcess == observedProcess,
                    receipt.operation == 0, receipt.systemError == 0, receipt.stage == 0,
                    receipt.source == 0, receipt.code == 0, receipt.truncated == 0,
                    receipt.finalizationError == 0, receipt.sandboxEntitlementVerified, receipt.cause.isEmpty
                else { throw StaticFixtureError.invalid("Explicit initialized cache reset") }
            }
            if requestID == 2 {
                snapshots.append(
                    try segmentedSessionXPC(
                        peer: peer, queue: previewQueue, archive: archive, root: root,
                        process: observedProcess, deadline: deadline))
                continue
            }
            var identity = Dust3DWorkerRequestIdentity()
            identity.document.0 = 1
            identity.request = requestID
            identity.revision = 7
            var result = Dust3DWorkerRequestResult()
            let created = archive.withUnsafeBytes {
                dust3d_worker_session_request_create(
                    identity, $0.bindMemory(to: UInt8.self).baseAddress, $0.count, 0, wire, &result)
            }
            guard let request = created, result.status == DUST3D_WORKER_REQUEST_OK else {
                throw StaticFixtureError.invalid("Session request encoding")
            }
            defer { dust3d_worker_request_destroy(request) }
            var size = 0
            guard let bytes = dust3d_worker_request_bytes(request, &size) else {
                throw StaticFixtureError.invalid("Owned request")
            }
            try queue.submit(Data(bytes: bytes, count: size))
            guard case .started(let pending) = try queue.begin(generation: generation) else {
                throw StaticFixtureError.invalid("Session queue begin")
            }
            let inputURL = root.appendingPathComponent("session-request-\(requestID)")
            let outputURL = root.appendingPathComponent("session-reply-\(requestID)")
            try pending.frame.write(to: inputURL)
            try Data().write(to: outputURL)
            let input = try FileHandle(forReadingFrom: inputURL)
            let output = try FileHandle(forWritingTo: outputURL)
            defer {
                try? input.close()
                try? output.close()
            }
            let outcome = WorkerOutcome<WorkerControlReceipt>()
            let admitted = pending.identity
            Task.detached {
                do {
                    try await peer.submitSession(
                        input: input, output: output, identity: admitted, generation: generation,
                        maximumCachePayloadBytes: 64 * 1024 * 1024, limits: policy, outcome: outcome)
                } catch { outcome.finish(.failure(error)) }
            }
            let control = try outcome.wait(until: deadline)
            guard control.status == DUST3D_WORKER_JOB_COMPLETE.rawValue, control.operation == 9,
                control.request == requestID,
                control.admissionStatus == 0, control.finalizationError == 0, control.sandboxEntitlementVerified,
                control.systemError == 0, control.stage == 0, control.source == 0, control.code == 0,
                control.truncated == 0, control.cause.isEmpty,
                control.process > 0, control.process != getpid(), control.process == control.observedProcess
            else {
                throw StaticFixtureError.invalid(
                    "Session receipt/OS peer: status=\(control.status) operation=\(control.operation) request=\(control.request) admission=\(control.admissionStatus) system=\(control.systemError) stage=\(control.stage) source=\(control.source) code=\(control.code) process=\(control.process) observed=\(control.observedProcess)"
                )
            }
            if observedProcess != 0 && observedProcess != control.observedProcess {
                throw StaticFixtureError.invalid("Session peer changed")
            }
            observedProcess = control.observedProcess
            let reader = try FileHandle(forReadingFrom: outputURL)
            defer { try? reader.close() }
            try queue.withReplyCollector { collector in
                while let data = try reader.read(upToCount: 97), !data.isEmpty {
                    let copied = data.withUnsafeBytes {
                        dust3d_worker_reply_append(collector, $0.bindMemory(to: UInt8.self).baseAddress, $0.count)
                    }
                    guard copied.status == DUST3D_WORKER_REPLY_READY else {
                        throw StaticFixtureError.invalid("Session frame copy")
                    }
                }
                guard dust3d_worker_reply_finish(collector).status == DUST3D_WORKER_REPLY_FRAMED else {
                    throw StaticFixtureError.invalid("Session actual EOF")
                }
            }
            let observed = Dust3DWorkerServiceObservation(
                connection_generation: generation, process_identifier: Int64(control.observedProcess))
            let receipt = Dust3DWorkerServiceReceipt(
                runtime_status: control.status, admission_status: control.admissionStatus, operation: control.operation,
                request_identity: control.request, process_identifier: Int64(control.process),
                system_error: control.systemError, io_stage: control.stage,
                detail_source: control.source, detail_code: control.code, message_truncated: control.truncated,
                finalization_error: control.finalizationError)
            snapshots.append(
                try queue.withSessionDecodedReply(observed: observed, receipt: receipt, limits: policy) {
                    try copySessionXPCOutput($0)
                })
        }
        guard snapshots.count == 3, snapshots[1].reused > 0, snapshots[1].retained > 0,
            snapshots[2].rebuilt > 0, snapshots[2].reused == 0,
            snapshots[0].positions == snapshots[2].positions, snapshots[0].indices == snapshots[2].indices,
            snapshots[0].positions == snapshots[1].positions, snapshots[0].indices == snapshots[1].indices
        else { throw StaticFixtureError.invalid("Actual retained cache reuse") }
        print("XPC service runtime sandbox entitlement verified")
        print("Service one-slot native admission accepted")
        print("Persistent service C admission accepted after actual EOF and OS peer observation")
        print("Queue-authoritative service completion admitted")
        print("Actual session cache reuse observed across two requests")
        print("Actual session cache rebuild observed after authenticated reset")
        let closed = WorkerOutcome<WorkerControlReceipt>()
        Task.detached {
            do { try await peer.closeSession(request: 5, generation: generation, outcome: closed) } catch {
                closed.finish(.failure(error))
            }
        }
        let receipt = try closed.wait(until: deadline)
        guard receipt.status == 0, receipt.admissionStatus == 0, receipt.request == 5,
            receipt.process == observedProcess, receipt.observedProcess == observedProcess, receipt.operation == 0,
            receipt.systemError == 0, receipt.stage == 0, receipt.source == 0, receipt.code == 0,
            receipt.truncated == 0,
            receipt.finalizationError == 0, receipt.sandboxEntitlementVerified, receipt.cause.isEmpty
        else { throw StaticFixtureError.invalid("Explicit initialized cache close") }
        print("Session cache closed before connection invalidation")
    }
    guard snapshots.count == 3 && !snapshots[0].positions.isEmpty else {
        throw StaticFixtureError.invalid("Copies after connection retirement")
    }
    print("Isolated XPC session cache reuse passed: servicePID=\(observedProcess); hostPID=\(getpid())")
}
