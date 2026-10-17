import Dust3DEngine
import Foundation

private struct CopiedUvMetadata {
    let triangles: [Dust3DUvTriangle]
    let charts: [Dust3DUvChart]
    let gradients: [Dust3DUvGradient]
    let componentIdentifiers: [String]
}

private func copiedMetadata(_ data: Data) throws -> CopiedUvMetadata {
    var error = Dust3DError()
    var options = Dust3DUvOptions(
        struct_size: MemoryLayout<Dust3DUvOptions>.size, maximum_payload_bytes: 0,
        texture_size: 0, images: nil, image_count: 0)
    let owner = data.withUnsafeBytes {
        dust3d_uv_mesh_load($0.bindMemory(to: UInt8.self).baseAddress, $0.count, &options, &error)
    }
    guard let owner else { throw CocoaError(.coderInvalidValue) }
    defer { dust3d_uv_mesh_destroy(owner) }
    guard let geometry = dust3d_uv_mesh_geometry(owner), let triangles = dust3d_uv_mesh_triangles(owner),
        let charts = dust3d_uv_mesh_charts(owner)
    else { throw CocoaError(.coderInvalidValue) }
    let count = dust3d_uv_mesh_triangle_count(owner)
    precondition(count > 0 && count * 3 == dust3d_mesh_vertex_count(geometry))
    let copiedTriangles = Array(UnsafeBufferPointer(start: triangles, count: count))
    let copiedCharts = Array(UnsafeBufferPointer(start: charts, count: dust3d_uv_mesh_chart_count(owner)))
    let copiedGradients = Array(
        UnsafeBufferPointer(start: dust3d_uv_mesh_gradients(owner), count: dust3d_uv_mesh_gradient_count(owner)))
    let identifiers = (0..<dust3d_uv_mesh_component_count(owner)).map {
        String(cString: dust3d_uv_mesh_component_id(owner, $0))
    }
    precondition(dust3d_uv_mesh_payload_bytes(owner) > 0 && dust3d_uv_mesh_texture_size(owner) > 0)
    return CopiedUvMetadata(
        triangles: copiedTriangles, charts: copiedCharts, gradients: copiedGradients,
        componentIdentifiers: identifiers)
}

@main struct UvMetadataSmoke {
    static func main() throws {
        precondition(Int(DUST3D_DEFAULT_UV_PAYLOAD_BYTES) == 64 * 1024 * 1024)
        precondition(Int(DUST3D_MAX_UV_PAYLOAD_BYTES) == 256 * 1024 * 1024)
        let data = try Data(contentsOf: URL(fileURLWithPath: CommandLine.arguments[1]))
        let first = try copiedMetadata(data)
        let second = try copiedMetadata(data)
        precondition(first.triangles.count == second.triangles.count)
        precondition(first.componentIdentifiers == second.componentIdentifiers && !first.componentIdentifiers.isEmpty)
        precondition(!first.charts.isEmpty)
        for triangle in first.triangles {
            let coordinates = [
                triangle.uv.0.0, triangle.uv.0.1, triangle.uv.1.0,
                triangle.uv.1.1, triangle.uv.2.0, triangle.uv.2.1,
            ]
            precondition(coordinates.allSatisfy { $0.isFinite && (0...1).contains($0) })
            if triangle.component_kind == DUST3D_UV_COMPONENT {
                precondition(first.componentIdentifiers.indices.contains(triangle.component_index))
            } else {
                precondition(triangle.component_index == Int(bitPattern: UInt.max))
            }
        }
        for gradient in first.gradients {
            precondition(first.charts.indices.contains(gradient.chart_index) && gradient.resolution == 512)
        }
        print(
            "Swift 6 UV metadata smoke passed: \(first.triangles.count) copied triangles; \(first.charts.count) charts")
    }
}
