import Dust3DEngine
import Foundation

struct CopiedResult: Sendable {
    let positions: [SIMD3<Float>]
    let reused: Int
    let retained: Int
}

// The native API serializes mutation. ARC keeps this immutable handle alive
// throughout each synchronous operation; no borrowed native storage escapes.
private final class NativeOwner: @unchecked Sendable {
    let handle: OpaquePointer
    init() throws {
        var error = Dust3DError()
        guard let value = dust3d_generation_session_create(Int(DUST3D_DEFAULT_CACHE_PAYLOAD_BYTES), &error) else {
            throw CocoaError(.coderInvalidValue)
        }
        handle = value
    }
    deinit { dust3d_generation_session_destroy(handle) }
}

actor Session {
    private let owner: NativeOwner
    init() throws { owner = try NativeOwner() }
    func load(_ data: Data) throws -> CopiedResult {
        try Task.checkCancellation()
        var error = Dust3DError()
        let mesh = data.withUnsafeBytes {
            dust3d_generation_session_load(owner.handle, $0.bindMemory(to: UInt8.self).baseAddress, $0.count, &error)
        }
        guard let mesh else { throw CocoaError(.coderInvalidValue) }
        defer { dust3d_mesh_destroy(mesh) }
        var stats = Dust3DGenerationStats()
        guard
            dust3d_generation_session_get_stats(owner.handle, &stats, MemoryLayout.size(ofValue: stats), &error)
                == DUST3D_STATUS_OK,
            let vertices = dust3d_mesh_vertices(mesh)
        else { throw CocoaError(.coderInvalidValue) }
        let positions = UnsafeBufferPointer(start: vertices, count: Int(dust3d_mesh_vertex_count(mesh))).map {
            SIMD3($0.position.0, $0.position.1, $0.position.2)
        }
        try Task.checkCancellation()
        return CopiedResult(
            positions: positions, reused: stats.reused_components, retained: stats.retained_payload_bytes)
    }
    func reset() throws {
        guard dust3d_generation_session_reset(owner.handle, nil) == DUST3D_STATUS_OK else {
            throw CocoaError(.coderInvalidValue)
        }
    }
}

@main struct Smoke {
    static func main() async throws {
        precondition(Int(DUST3D_DEFAULT_CACHE_PAYLOAD_BYTES) == 64 * 1024 * 1024)
        precondition(Int(DUST3D_MAX_CACHE_PAYLOAD_BYTES) == 256 * 1024 * 1024)
        let data = try Data(contentsOf: URL(fileURLWithPath: CommandLine.arguments[1]))
        let session = try Session()
        let first = try await session.load(data)
        let second = try await session.load(data)
        precondition(first.reused == 0 && second.reused > 0 && second.retained > 0)
        precondition(first.positions == second.positions && !first.positions.isEmpty)
        try await session.reset()
        let rebuilt = try await session.load(data)
        precondition(rebuilt.reused == 0 && first.positions == rebuilt.positions)
        print(
            "Swift 6 strict-concurrency session smoke passed: \(first.positions.count) copied vertices; \(second.reused) cache hits; \(second.retained) retained payload bytes"
        )
    }
}
