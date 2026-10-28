// SPDX-License-Identifier: MIT
import Dust3DWorkerRuntime
import Foundation

#if canImport(Darwin)
    import Darwin
#else
    import Glibc
#endif

private enum ServeSmokeError: Error { case invalid }

private struct CopiedNativePreview: Sendable {
    let attributes: [Float]
    let indices: [UInt32]
    let identities: [String]
    let reusedCache: Bool
}

private func job(_ session: OpaquePointer, archive: Data, request: UInt64) throws
    -> CopiedNativePreview
{
    guard let input = tmpfile() else { throw ServeSmokeError.invalid }
    defer { fclose(input) }
    guard let output = tmpfile() else { throw ServeSmokeError.invalid }
    defer { fclose(output) }
    var scope = Dust3DWorkerPreviewSessionScope()
    scope.identity.document.0 = 1
    scope.identity.request = request
    scope.identity.revision = 7
    scope.connection_generation = 3
    let context = Dust3DWorkerPreviewArchiveContext(
        archive_bytes: UInt64(archive.count), maximum_source_bytes: 0, maximum_indexed_bytes: 0)
    let inputPolicy = Dust3DWorkerPreviewChunkPolicy(
        maximum_transfer_bytes: UInt64(archive.count + 24), chunk_payload_bytes: 64)
    let outputPolicy = Dust3DWorkerPreviewChunkPolicy(
        maximum_transfer_bytes: 64 * 1024 * 1024, chunk_payload_bytes: 256)
    let sent = archive.withUnsafeBytes {
        dust3d_worker_preview_archive_write(
            fileno(input), scope, context, $0.bindMemory(to: UInt8.self).baseAddress,
            $0.count, inputPolicy)
    }
    guard sent.status == DUST3D_WORKER_PREVIEW_ARCHIVE_OK else { throw ServeSmokeError.invalid }
    rewind(input)
    let served = dust3d_worker_preview_session_serve(
        session, fileno(input), fileno(output), scope, context, inputPolicy, outputPolicy)
    guard served.status == DUST3D_WORKER_PREVIEW_SESSION_OK else { throw ServeSmokeError.invalid }
    rewind(output)
    var result = Dust3DWorkerPreviewSessionResult()
    guard
        let owner = dust3d_worker_preview_session_read(
            fileno(output), scope, context, outputPolicy, &result)
    else { throw ServeSmokeError.invalid }
    defer { dust3d_worker_session_output_destroy(owner) }
    guard result.status == DUST3D_WORKER_PREVIEW_SESSION_OK,
        dust3d_worker_session_output_summary(owner).outcome == DUST3D_WORKER_SESSION_SOURCED,
        let primary = dust3d_worker_session_output_primary(owner)
    else { throw ServeSmokeError.invalid }
    let summary = dust3d_worker_decoded_summary(primary)
    let count = dust3d_worker_decoded_index_count(primary)
    let sources = dust3d_worker_decoded_source_summary(primary)
    // Expectations for this licensed tiny fixture, not product/native profile limits.
    guard summary.vertex_count > 0, summary.vertex_count < 100_000,
        count > 0, count < 100_000, sources.identity_count > 0,
        sources.identity_count < 100_000
    else { throw ServeSmokeError.invalid }
    var vertices = [Dust3DWorkerVertex](
        repeating: Dust3DWorkerVertex(), count: summary.vertex_count)
    var indices = [UInt32](repeating: 0, count: count)
    let hasVertices = vertices.withUnsafeMutableBufferPointer {
        dust3d_worker_decoded_vertices(primary, 0, $0.count, $0.baseAddress)
    }
    let hasIndices = indices.withUnsafeMutableBufferPointer {
        dust3d_worker_decoded_indices(primary, 0, $0.count, $0.baseAddress)
    }
    var identities: [String] = []
    for index in 0..<sources.identity_count {
        var size = 0
        guard let text = dust3d_worker_decoded_source_node(primary, index, &size) else {
            throw ServeSmokeError.invalid
        }
        identities.append(
            String(
                decoding: UnsafeBufferPointer(start: text, count: size).map {
                    UInt8(bitPattern: $0)
                }, as: UTF8.self))
    }
    var statistics = Dust3DWorkerSessionStatistics()
    let available = dust3d_worker_session_output_statistics(
        owner, &statistics, MemoryLayout<Dust3DWorkerSessionStatistics>.size)
    guard hasVertices == 1, hasIndices == 1, available == 1,
        statistics.retained_payload_bytes > 0
    else { throw ServeSmokeError.invalid }
    return CopiedNativePreview(
        attributes: vertices.flatMap {
            [
                $0.position.0, $0.position.1, $0.position.2, $0.normal.0, $0.normal.1,
                $0.normal.2, $0.color.0, $0.color.1, $0.color.2, $0.color.3,
            ]
        }, indices: indices, identities: identities,
        reusedCache: statistics.reused_components > 0)
}

private func run(_ path: String) throws -> CopiedNativePreview {
    let archive = try Data(contentsOf: URL(fileURLWithPath: path))
    guard !archive.isEmpty, archive.count < 1_048_576 else { throw ServeSmokeError.invalid }
    var configuration = Dust3DWorkerSessionConfiguration()
    configuration.document.0 = 1
    configuration.connection_generation = 3
    configuration.maximum_cache_payload_bytes = 64 * 1024 * 1024
    var result = Dust3DWorkerDecodeResult()
    guard let session = dust3d_worker_session_create(configuration, &result) else {
        throw ServeSmokeError.invalid
    }
    defer { dust3d_worker_session_destroy(session) }
    guard result.status == DUST3D_WORKER_DECODE_OK else { throw ServeSmokeError.invalid }
    let first = try job(session, archive: archive, request: 1)
    let second = try job(session, archive: archive, request: 2)
    guard first.attributes == second.attributes, first.indices == second.indices,
        first.identities == second.identities, !first.reusedCache, second.reusedCache
    else { throw ServeSmokeError.invalid }
    return second
}

private actor Inspector {
    func accepts(_ value: CopiedNativePreview) -> Bool {
        !value.attributes.isEmpty && value.attributes.count % 10 == 0
            && value.indices.allSatisfy { UInt64($0) < UInt64(value.attributes.count / 10) }
            && value.identities.allSatisfy { !$0.isEmpty } && value.reusedCache
    }
}

@main
private enum PreviewServeSmoke {
    static func main() async throws {
        guard CommandLine.arguments.count == 2 else { throw ServeSmokeError.invalid }
        let path = CommandLine.arguments[1]
        let copy = try await Task.detached { try run(path) }.value
        guard await Inspector().accepts(copy) else { throw ServeSmokeError.invalid }
    }
}
