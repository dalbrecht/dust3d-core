import Dust3DEngine
import Foundation

private func copiedSources(_ data: Data) throws -> (count: Int, identifiers: [String]) {
    var error = Dust3DError()
    var options = Dust3DSourceOptions(
        struct_size: MemoryLayout<Dust3DSourceOptions>.size, maximum_payload_bytes: 0)
    let owner = data.withUnsafeBytes {
        dust3d_sourced_mesh_load($0.bindMemory(to: UInt8.self).baseAddress, $0.count, &options, &error)
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
        print(
            "Swift 6 source smoke passed: \(first.count) copied corners; \(first.identifiers.count) original identities"
        )
    }
}
