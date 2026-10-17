import Dust3DEngine
import Foundation

private enum BakeSmokeError: Error {
    case invalidOutput
}

private struct CopiedBake {
    let vertices: [Dust3DVertex]
    let triangles: [Dust3DUvTriangle]
    let tangents: [Dust3DTextureTangent]
    let pixels: [UInt8]
}

private func copiedBake(_ data: Data) throws -> CopiedBake {
    var error = Dust3DError()
    var options = Dust3DBakeOptions()
    options.struct_size = MemoryLayout<Dust3DBakeOptions>.size
    options.color_size = 8
    options.normal_size = 8
    options.material_size = 8
    let owner = data.withUnsafeBytes {
        dust3d_baked_mesh_load($0.bindMemory(to: UInt8.self).baseAddress, $0.count, &options, &error)
    }
    guard let owner else { throw BakeSmokeError.invalidOutput }
    defer { dust3d_baked_mesh_destroy(owner) }
    guard let uvs = dust3d_baked_mesh_uvs(owner), let geometry = dust3d_uv_mesh_geometry(uvs),
        let vertices = dust3d_mesh_vertices(geometry), let triangles = dust3d_uv_mesh_triangles(uvs),
        let tangents = dust3d_baked_mesh_tangents(owner)
    else { throw BakeSmokeError.invalidOutput }
    let view = dust3d_baked_mesh_texture(owner, DUST3D_TEXTURE_COLOR)
    guard let pixels = view.pixels else { throw BakeSmokeError.invalidOutput }
    let count = dust3d_baked_mesh_tangent_count(owner)
    precondition(count == dust3d_uv_mesh_triangle_count(uvs) && count * 3 == dust3d_mesh_vertex_count(geometry))
    precondition(view.width == 8 && view.height == 8 && view.row_stride == 32 && view.byte_count == 256)
    return CopiedBake(
        vertices: Array(UnsafeBufferPointer(start: vertices, count: count * 3)),
        triangles: Array(UnsafeBufferPointer(start: triangles, count: count)),
        tangents: Array(UnsafeBufferPointer(start: tangents, count: count)),
        pixels: Array(UnsafeBufferPointer(start: pixels, count: view.byte_count)))
}

@main struct BakedMeshSmoke {
    static func main() throws {
        let data = try Data(contentsOf: URL(fileURLWithPath: CommandLine.arguments[1]))
        let first = try copiedBake(data)
        let second = try copiedBake(data)
        precondition(first.pixels == second.pixels && !first.vertices.isEmpty)
        for tangent in first.tangents {
            precondition(tangent.direction.0.isFinite && tangent.direction.1.isFinite && tangent.direction.2.isFinite)
            precondition(tangent.known == 0 || tangent.known == 1)
        }
        precondition(first.triangles.count == first.tangents.count)
        print(
            "Swift 6 baked mesh smoke passed: \(first.tangents.count) copied tangent frames; \(first.pixels.count) pixels bytes"
        )
    }
}
