// SPDX-License-Identifier: MIT
import Darwin
import Dust3DWorkerRuntime
import Foundation

private enum SessionSmokeError: Error { case invalid(String) }
private func require(_ value: Bool, _ cause: String) throws {
    guard value else { throw SessionSmokeError.invalid(cause) }
}
private struct Snapshot: Sendable, Equatable {
    let positions: [Float]
    let indices: [UInt32]
    let node: Data
    let reused: UInt64
    let rebuilt: UInt64
    let cacheCause: Data
}
private actor Receiver {
    private var snapshots: [Snapshot] = []
    func store(_ values: [Snapshot]) { snapshots = values }
    func read() -> [Snapshot] { snapshots }
}
private func copyOutput(_ owner: OpaquePointer) throws -> Snapshot {
    try require(dust3d_worker_session_output_summary(owner).outcome == DUST3D_WORKER_SESSION_SOURCED, "Sourced outcome")
    guard let primary = dust3d_worker_session_output_primary(owner) else {
        throw SessionSmokeError.invalid("Missing primary")
    }
    let summary = dust3d_worker_decoded_summary(primary)
    let count = dust3d_worker_decoded_index_count(primary)
    // This licensed fixture is tiny; this is a test expectation, not a product limit.
    try require(
        summary.vertex_count > 0 && summary.vertex_count < 100_000 && count > 0 && count < 100_000, "Fixture size")
    var vertices = [Dust3DWorkerVertex](repeating: Dust3DWorkerVertex(), count: summary.vertex_count)
    try require(
        vertices.withUnsafeMutableBufferPointer { dust3d_worker_decoded_vertices(primary, 0, $0.count, $0.baseAddress) }
            == 1, "Vertex copy")
    let positions = vertices.flatMap { vertex in
        withUnsafeBytes(of: vertex.position) { Array($0.bindMemory(to: Float.self)) }
    }
    var indices = [UInt32](repeating: 0, count: count)
    try require(
        indices.withUnsafeMutableBufferPointer { dust3d_worker_decoded_indices(primary, 0, $0.count, $0.baseAddress) }
            == 1, "Index copy")
    try require(
        positions.allSatisfy(\.isFinite) && indices.allSatisfy { $0 < summary.vertex_count }, "Geometry integrity")
    let sources = dust3d_worker_decoded_source_summary(primary)
    try require(sources.identity_count > 0 && sources.corner_count == count, "Source correspondence")
    var size = 0
    guard let node = dust3d_worker_decoded_source_node(primary, 0, &size), size > 0 && size <= 38 else {
        throw SessionSmokeError.invalid("Source node bytes")
    }
    let copiedNode = Data(bytes: node, count: size)
    var statistics = Dust3DWorkerSessionStatistics()
    try require(
        dust3d_worker_session_output_statistics(owner, &statistics, MemoryLayout.size(ofValue: statistics)) == 1,
        "Available observations")
    try require(
        statistics.maximum_cache_payload_bytes == 64 * 1024 * 1024 && statistics.retained_payload_bytes > 0,
        "Actual cache")
    let cause = dust3d_worker_session_output_cache_cause(owner, &size)
    let cacheCause: Data
    if let cause {
        cacheCause = Data(bytes: cause, count: size)
    } else {
        try require(size == 0, "Absent cause size")
        cacheCause = Data()
    }
    try require(
        dust3d_worker_session_output_source_issue(owner) == nil
            && dust3d_worker_session_output_statistics_issue(owner) == nil, "Unexpected secondary issue")
    return Snapshot(
        positions: positions, indices: indices, node: copiedNode, reused: statistics.reused_components,
        rebuilt: statistics.rebuilt_components, cacheCause: cacheCause)
}
private func generate(
    _ session: WorkerServiceSession<WorkerProfileServiceSession>, _ scope: WorkerServiceSessionScope,
    _ identity: Dust3DWorkerRequestIdentity, _ archive: Data, _ directory: URL
) throws -> Snapshot {
    let wire = Dust3DWorkerWireLimits(
        maximum_input_bytes: 128 * 1024 * 1024, maximum_output_bytes: 256 * 1024 * 1024, maximum_failure_bytes: 4096)
    let limits = Dust3DWorkerSessionOutputLimits(
        maximum_wire_bytes: wire.maximum_output_bytes, maximum_decoded_bytes: 128 * 1024 * 1024, maximum_source_bytes: 0
    )
    var requestResult = Dust3DWorkerRequestResult()
    let created = archive.withUnsafeBytes {
        dust3d_worker_session_request_create(
            identity, $0.bindMemory(to: UInt8.self).baseAddress, $0.count, 0, wire, &requestResult)
    }
    guard let request = created, requestResult.status == DUST3D_WORKER_REQUEST_OK else {
        throw SessionSmokeError.invalid("Request creation")
    }
    defer { dust3d_worker_request_destroy(request) }
    var size = 0
    guard let bytes = dust3d_worker_request_bytes(request, &size) else {
        throw SessionSmokeError.invalid("Request bytes")
    }
    let frame = Data(bytes: bytes, count: size)
    guard let header = dust3d_worker_request_header(request, &size) else {
        throw SessionSmokeError.invalid("Header bytes")
    }
    let copiedHeader = Data(bytes: header, count: size)
    let inputURL = directory.appendingPathComponent("request-\(identity.request)")
    let outputURL = directory.appendingPathComponent("reply-\(identity.request)")
    try frame.write(to: inputURL)
    try Data().write(to: outputURL)
    let input = try FileHandle(forReadingFrom: inputURL)
    let output = try FileHandle(forWritingTo: outputURL)
    defer {
        try? input.close()
        try? output.close()
    }
    let profileReceipt = try session.serve(
        scope: scope,
        job: .legacy(
            WorkerSessionChannelJob(
                expected: identity, input: input.fileDescriptor, output: output.fileDescriptor, limits: limits)))
    guard case .legacy(let job) = profileReceipt else { throw SessionSmokeError.invalid("Legacy receipt profile") }
    try require(
        job.status == DUST3D_WORKER_JOB_COMPLETE
            && job.operation == DUST3D_WORKER_OPERATION_SESSION_INDEXED_SOURCED_MESH
            && job.request_identity == identity.request, "Serving receipt")
    try require(
        fcntl(input.fileDescriptor, F_GETFD) >= 0 && fcntl(output.fileDescriptor, F_GETFD) >= 0, "Borrowed descriptors")
    let reader = try FileHandle(forReadingFrom: outputURL)
    defer { try? reader.close() }
    let reply = try reader.readToEnd() ?? Data()
    var framing = Dust3DWorkerReplyResult()
    let collected = copiedHeader.withUnsafeBytes {
        dust3d_worker_service_reply_create($0.bindMemory(to: UInt8.self).baseAddress, $0.count, 9, wire, &framing)
    }
    guard let collector = collected, framing.status == DUST3D_WORKER_REPLY_READY else {
        throw SessionSmokeError.invalid("Collector")
    }
    defer { dust3d_worker_reply_destroy(collector) }
    let appended = reply.withUnsafeBytes {
        dust3d_worker_reply_append(collector, $0.bindMemory(to: UInt8.self).baseAddress, $0.count)
    }
    try require(
        appended.status == DUST3D_WORKER_REPLY_READY
            && dust3d_worker_reply_finish(collector).status == DUST3D_WORKER_REPLY_FRAMED, "Actual file EOF")
    var receipt = Dust3DWorkerServiceReceipt()
    receipt.runtime_status = job.status.rawValue
    receipt.operation = job.operation.rawValue
    receipt.request_identity = job.request_identity
    receipt.process_identifier = Int64(getpid())
    receipt.system_error = job.system_error
    receipt.io_stage = job.io_stage.rawValue
    receipt.detail_source = job.detail_source.rawValue
    receipt.detail_code = job.detail_code
    receipt.message_truncated = job.message_truncated
    // Same-process fixture observations exercise correlation, not XPC authentication.
    let observed = Dust3DWorkerServiceObservation(connection_generation: 9, process_identifier: Int64(getpid()))
    var result = Dust3DWorkerDecodeResult()
    guard let owner = dust3d_worker_session_service_admit(collector, identity, observed, receipt, limits, &result),
        result.status == DUST3D_WORKER_DECODE_OK
    else { throw SessionSmokeError.invalid("Session admission") }
    defer { dust3d_worker_session_output_destroy(owner) }
    return try copyOutput(owner)
}
private func generatePreview(
    _ session: WorkerServiceSession<WorkerProfileServiceSession>, _ scope: WorkerServiceSessionScope,
    _ identity: Dust3DWorkerRequestIdentity, _ archive: Data
) throws -> Snapshot {
    guard let input = tmpfile() else { throw SessionSmokeError.invalid("Fixture input") }
    defer { fclose(input) }
    guard let output = tmpfile() else { throw SessionSmokeError.invalid("Fixture output") }
    defer { fclose(output) }
    var correlation = Dust3DWorkerPreviewSessionScope()
    correlation.identity = identity
    correlation.connection_generation = scope.connectionGeneration
    let context = Dust3DWorkerPreviewArchiveContext(
        archive_bytes: UInt64(archive.count), maximum_source_bytes: 0, maximum_indexed_bytes: 0)
    let inputPolicy = Dust3DWorkerPreviewChunkPolicy(
        maximum_transfer_bytes: UInt64(archive.count + 24), chunk_payload_bytes: 64)
    let outputPolicy = Dust3DWorkerPreviewChunkPolicy(
        maximum_transfer_bytes: 64 * 1024 * 1024, chunk_payload_bytes: 256)
    let written = archive.withUnsafeBytes {
        dust3d_worker_preview_archive_write(
            fileno(input), correlation, context, $0.bindMemory(to: UInt8.self).baseAddress, $0.count, inputPolicy)
    }
    try require(written.status == DUST3D_WORKER_PREVIEW_ARCHIVE_OK, "Preview archive write")
    rewind(input)
    let profileReceipt = try session.serve(
        scope: scope,
        job: .preview(
            WorkerPreviewChannelJob(
                expected: identity, input: fileno(input), output: fileno(output), context: context,
                inputPolicy: inputPolicy, outputPolicy: outputPolicy)))
    guard case .preview(let result) = profileReceipt else { throw SessionSmokeError.invalid("Preview receipt profile") }
    try require(result.status == DUST3D_WORKER_PREVIEW_SESSION_OK, "Preview serving receipt")
    rewind(output)
    var decoded = Dust3DWorkerPreviewSessionResult()
    guard
        let owner = dust3d_worker_preview_session_read(
            fileno(output), correlation, context, outputPolicy, &decoded)
    else { throw SessionSmokeError.invalid("Preview output owner") }
    defer { dust3d_worker_session_output_destroy(owner) }
    try require(decoded.status == DUST3D_WORKER_PREVIEW_SESSION_OK, "Preview output receipt")
    return try copyOutput(owner)
}
private func runSession(_ archive: Data, _ directory: URL) throws -> [Snapshot] {
    let document = UUID(uuid: (1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0))
    let scope = try WorkerServiceSessionScope(
        document: document, connectionGeneration: 9, maximumCachePayloadBytes: 64 * 1024 * 1024)
    let owner = WorkerServiceSession(backend: WorkerProfileServiceSession())
    var identity = Dust3DWorkerRequestIdentity()
    identity.document.0 = 1
    identity.request = 1
    identity.revision = 42
    let first = try generate(owner, scope, identity, archive, directory)
    identity.request += 1
    let reused = try generatePreview(owner, scope, identity, archive)
    try require(
        reused.reused > 0 && reused.positions == first.positions && reused.indices == first.indices
            && reused.node == first.node, "Cross-profile actual cache reuse and geometry")
    try owner.reset(scope: scope)
    identity.request += 1
    let rebuilt = try generate(owner, scope, identity, archive, directory)
    try require(rebuilt.rebuilt > 0, "Actual rebuild")
    try owner.close()
    return [first, reused, rebuilt]
}
@main
private enum WorkerSessionSmoke {
    static func main() async {
        do { try await run() } catch {
            FileHandle.standardError.write(Data("Swift session consumer failed: \(error)\n".utf8))
            exit(EXIT_FAILURE)
        }
    }
    static func run() async throws {
        try require(CommandLine.arguments.count == 3, "Fixture and shared scratch required")
        // Synthetic diagnostic values exercise copying only, never a native failure.
        var diagnosticValue = Dust3DWorkerDecodeResult()
        diagnosticValue.status = DUST3D_WORKER_DECODE_ADMISSION_REJECTED
        diagnosticValue.detail_source = DUST3D_WORKER_DETAIL_SESSION_SCOPE
        diagnosticValue.detail_code = -3
        diagnosticValue.message_truncated = 1
        diagnosticValue.message.0 = CChar(bitPattern: 255)
        let diagnostic = WorkerSessionBoundaryError(diagnosticValue)
        let previewDiagnostic = WorkerPreviewRPCFailure(diagnostic)
        try require(
            previewDiagnostic.status == DUST3D_WORKER_PREVIEW_SESSION_ADMISSION_REJECTED.rawValue
                && previewDiagnostic.boundaryStatus == diagnostic.status
                && previewDiagnostic.source == diagnostic.source && previewDiagnostic.code == diagnostic.code
                && previewDiagnostic.truncated == 1 && previewDiagnostic.cause == Data([255]),
            "Preview RPC retains original boundary domain and diagnostic")
        for (original, expected) in [
            (DUST3D_WORKER_DECODE_PROTOCOL_REJECTED, DUST3D_WORKER_PREVIEW_SESSION_PROTOCOL_REJECTED),
            (DUST3D_WORKER_DECODE_LIMIT_EXCEEDED, DUST3D_WORKER_PREVIEW_SESSION_RANGE_REJECTED),
            (DUST3D_WORKER_DECODE_INTERNAL_FAILED, DUST3D_WORKER_PREVIEW_SESSION_INTERNAL_FAILED),
        ] {
            var value = diagnosticValue
            value.status = original
            let mapped = WorkerPreviewRPCFailure(WorkerSessionBoundaryError(value))
            try require(
                mapped.status == expected.rawValue && mapped.boundaryStatus == original.rawValue
                    && mapped.code == -3 && mapped.source == diagnostic.source && mapped.cause == Data([255]),
                "Preview boundary classification must remain independent")
        }
        let scopePreview = WorkerPreviewRPCFailure(WorkerServiceSessionError.closed)
        try require(
            scopePreview.status == DUST3D_WORKER_PREVIEW_SESSION_ADMISSION_REJECTED.rawValue
                && scopePreview.boundaryStatus == 0 && scopePreview.admission == .invalidSessionScope,
            "Preview Swift lifecycle rejection")
        let unexpectedPreview = WorkerPreviewRPCFailure(SessionSmokeError.invalid(String(repeating: "x", count: 600)))
        try require(
            unexpectedPreview.status == DUST3D_WORKER_PREVIEW_SESSION_INTERNAL_FAILED.rawValue
                && unexpectedPreview.boundaryStatus == 0 && unexpectedPreview.truncated == 1
                && unexpectedPreview.cause.count == 511, "Bounded original Swift diagnostic")
        let rpcDiagnostic = WorkerSessionRPCFailure(diagnostic)
        try require(
            rpcDiagnostic.status == DUST3D_WORKER_JOB_PROTOCOL_REJECTED.rawValue
                && rpcDiagnostic.source == diagnostic.source && rpcDiagnostic.code == diagnostic.code
                && rpcDiagnostic.truncated == 1 && rpcDiagnostic.cause == Data([255])
                && rpcDiagnostic.admission == .accepted,
            "Original RPC boundary diagnostic")
        let scopeDiagnostic = WorkerSessionRPCFailure(WorkerServiceSessionError.changedScope)
        try require(
            scopeDiagnostic.admission == .invalidSessionScope && scopeDiagnostic.cause == Data("changedScope".utf8),
            "Original RPC scope diagnostic")

        try require(
            diagnostic.status == DUST3D_WORKER_DECODE_ADMISSION_REJECTED.rawValue
                && diagnostic.source == DUST3D_WORKER_DETAIL_SESSION_SCOPE.rawValue
                && diagnostic.code == -3 && diagnostic.isTruncated && diagnostic.cause == Data([255]),
            "Original opaque diagnostic copy")

        let fixture = URL(fileURLWithPath: CommandLine.arguments[1])
        let directory = URL(fileURLWithPath: CommandLine.arguments[2]).appendingPathComponent(
            "session-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: directory) }
        let archive = try Data(contentsOf: fixture)
        let snapshots = try await Task.detached { try runSession(archive, directory) }.value
        let receiver = Receiver()
        await receiver.store(snapshots)
        let received = await receiver.read()
        try require(
            received == snapshots && snapshots.count == 3
                && snapshots.allSatisfy { !$0.positions.isEmpty && !$0.indices.isEmpty && !$0.node.isEmpty },
            "Copied values after all native owners and actor transfer")
        print("Swift6 session C importer, retained cache and copied Sendable output passed")
    }
}
