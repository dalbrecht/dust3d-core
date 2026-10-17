import Dust3DEngine
import Foundation

private func copiedSources(_ data: Data, session: OpaquePointer? = nil) throws -> (count: Int, identifiers: [String]) {
    var error = Dust3DError()
    var options = Dust3DSourceOptions(
        struct_size: MemoryLayout<Dust3DSourceOptions>.size, maximum_payload_bytes: 0)
    let owner = data.withUnsafeBytes {
        if let session {
            return dust3d_generation_session_load_sourced(
                session, $0.bindMemory(to: UInt8.self).baseAddress, $0.count, &options, &error)
        }
        return dust3d_sourced_mesh_load($0.bindMemory(to: UInt8.self).baseAddress, $0.count, &options, &error)
    }
    guard let owner else { throw CocoaError(.coderInvalidValue) }
    defer { dust3d_sourced_mesh_destroy(owner) }
    guard let geometry = dust3d_sourced_mesh_geometry(owner), let corners = dust3d_sourced_mesh_corners(owner) else {
        throw CocoaError(.coderInvalidValue)
    }
    let count = dust3d_sourced_mesh_corner_count(owner)
    precondition(count > 0 && count == dust3d_mesh_vertex_count(geometry))
    let copied = Array(UnsafeBufferPointer(start: corners, count: count))
    let identities = (0..<dust3d_sourced_mesh_identity_count(owner)).map {
        String(cString: dust3d_sourced_mesh_node_id(owner, $0))
    }
    precondition(!identities.isEmpty && copied.contains { $0.kind == DUST3D_SOURCE_NODE_HINT })
    return (copied.count, identities)
}

@main struct SourceSmoke {
    static func main() throws {
        precondition(Int(DUST3D_DEFAULT_SOURCE_PAYLOAD_BYTES) == 64 * 1024 * 1024)
        precondition(Int(DUST3D_MAX_SOURCE_PAYLOAD_BYTES) == 256 * 1024 * 1024)
        let data = try Data(contentsOf: URL(fileURLWithPath: CommandLine.arguments[1]))
        let first = try copiedSources(data)
        let second = try copiedSources(data)
        precondition(first.count == second.count && first.identifiers == second.identifiers)
        var error = Dust3DError()
        guard let session = dust3d_generation_session_create(Int(DUST3D_DEFAULT_CACHE_PAYLOAD_BYTES), &error) else {
            throw CocoaError(.coderInvalidValue)
        }
        defer { dust3d_generation_session_destroy(session) }
        let cachedFirst = try copiedSources(data, session: session)
        let cachedSecond = try copiedSources(data, session: session)
        var stats = Dust3DGenerationStats()
        precondition(
            dust3d_generation_session_get_stats(session, &stats, MemoryLayout.size(ofValue: stats), &error)
                == DUST3D_STATUS_OK)
        precondition(stats.reused_components > 0 && cachedFirst.count == cachedSecond.count)
        precondition(cachedSecond.identifiers == first.identifiers)
        print(
            "Swift 6 source smoke passed: \(first.count) copied corners; \(first.identifiers.count) original identities"
        )
    }
}
