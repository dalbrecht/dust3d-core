// SPDX-License-Identifier: MIT
import Dust3DWorkerRuntime
import Foundation

#if canImport(Darwin)
    import Darwin
#else
    import Glibc
#endif

private enum SmokeError: Error { case invalid }

private struct CopiedPreview: Sendable {
    let positions: [Float]
    let indices: [UInt32]
    let node: String
    let warning: String
    let hasZeroStatistics: Bool
}

private func receive(_ path: String) throws -> CopiedPreview {
    guard let channel = fopen(path, "rb") else { throw SmokeError.invalid }
    defer { fclose(channel) }
    var scope = Dust3DWorkerPreviewSessionScope()
    scope.identity.document.0 = 1
    scope.identity.request = 1
    scope.identity.revision = 2
    scope.connection_generation = 3
    let context = Dust3DWorkerPreviewArchiveContext(
        archive_bytes: 4096, maximum_source_bytes: 0, maximum_indexed_bytes: 0)
    let policy = Dust3DWorkerPreviewChunkPolicy(
        maximum_transfer_bytes: 16384, chunk_payload_bytes: 64)
    var result = Dust3DWorkerPreviewSessionResult()
    guard
        let owner = dust3d_worker_preview_session_read(
            fileno(channel), scope, context, policy, &result)
    else { throw SmokeError.invalid }
    defer { dust3d_worker_session_output_destroy(owner) }
    guard result.status == DUST3D_WORKER_PREVIEW_SESSION_OK else { throw SmokeError.invalid }
    let summary = dust3d_worker_session_output_summary(owner)
    guard summary.outcome == DUST3D_WORKER_SESSION_SOURCED,
        summary.statistics_available == 1,
        let primary = dust3d_worker_session_output_primary(owner)
    else { throw SmokeError.invalid }
    var vertices = [Dust3DWorkerVertex](repeating: Dust3DWorkerVertex(), count: 3)
    let hasVertices = vertices.withUnsafeMutableBufferPointer {
        dust3d_worker_decoded_vertices(primary, 0, $0.count, $0.baseAddress)
    }
    var indices = [UInt32](repeating: 0, count: 3)
    let hasIndices = indices.withUnsafeMutableBufferPointer {
        dust3d_worker_decoded_indices(primary, 0, $0.count, $0.baseAddress)
    }
    var nodeCount = 0
    var warningCount = 0
    guard hasVertices == 1, hasIndices == 1,
        let node = dust3d_worker_decoded_source_node(primary, 0, &nodeCount),
        let warning = dust3d_worker_decoded_warning(primary, &warningCount)
    else { throw SmokeError.invalid }
    var statistics = Dust3DWorkerSessionStatistics()
    let hasStatistics = dust3d_worker_session_output_statistics(
        owner, &statistics, MemoryLayout<Dust3DWorkerSessionStatistics>.size)
    return CopiedPreview(
        positions: vertices.flatMap { [$0.position.0, $0.position.1, $0.position.2] },
        indices: indices,
        node: String(
            decoding: UnsafeBufferPointer(start: node, count: nodeCount).map {
                UInt8(bitPattern: $0)
            }, as: UTF8.self),
        warning: String(
            decoding: UnsafeBufferPointer(start: warning, count: warningCount).map {
                UInt8(bitPattern: $0)
            }, as: UTF8.self),
        hasZeroStatistics: hasStatistics == 1 && statistics.retained_payload_bytes == 0)
}

private actor Inspector {
    func accepts(_ value: CopiedPreview) -> Bool {
        value.positions == [0, 0, 0, 1, 0, 0, 0, 1, 0]
            && value.indices == [0, 1, 2] && value.node == String(repeating: "n", count: 1500)
            && value.warning == String(repeating: "w", count: 5000) && value.hasZeroStatistics
    }
}

@main
private enum PreviewSessionSmoke {
    static func main() async throws {
        guard CommandLine.arguments.count == 2 else { throw SmokeError.invalid }
        let path = CommandLine.arguments[1]
        let copy = try await Task.detached { try receive(path) }.value
        guard await Inspector().accepts(copy) else { throw SmokeError.invalid }
    }
}
