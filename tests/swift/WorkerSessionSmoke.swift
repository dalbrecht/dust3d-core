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
    _ session: OpaquePointer, _ identity: Dust3DWorkerRequestIdentity, _ archive: Data, _ directory: URL
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
    let job = dust3d_worker_session_serve(session, 9, input.fileDescriptor, output.fileDescriptor, limits)
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
private func runSession(_ archive: Data, _ directory: URL) throws -> [Snapshot] {
    var configuration = Dust3DWorkerSessionConfiguration()
    configuration.document.0 = 1
    configuration.connection_generation = 9
    configuration.maximum_cache_payload_bytes = 64 * 1024 * 1024
    var result = Dust3DWorkerDecodeResult()
    guard let owner = dust3d_worker_session_create(configuration, &result), result.status == DUST3D_WORKER_DECODE_OK
    else { throw SessionSmokeError.invalid("Session creation") }
    defer { dust3d_worker_session_destroy(owner) }
    var identity = Dust3DWorkerRequestIdentity()
    identity.document.0 = 1
    identity.request = 1
    identity.revision = 42
    let first = try generate(owner, identity, archive, directory)
    identity.request += 1
    let reused = try generate(owner, identity, archive, directory)
    try require(reused.reused > 0, "Actual reuse")
    try require(dust3d_worker_session_reset(owner, 9, &result) == 1, "Reset")
    identity.request += 1
    let rebuilt = try generate(owner, identity, archive, directory)
    try require(rebuilt.rebuilt > 0, "Actual rebuild")
    try require(dust3d_worker_session_close(owner, &result) == 1, "Close")
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
