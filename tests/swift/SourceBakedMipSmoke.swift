// SPDX-License-Identifier: MIT
import Dust3DEngine
import Foundation

private enum MipSmokeError: Error {
    case invalidOutput(String)
}

private struct CopiedLevel: Sendable, Equatable {
    let width: Int
    let height: Int
    let pixels: Data
}

private struct CopiedBake: Sendable, Equatable {
    let maps: [[CopiedLevel]]
    let maximumSamplingLod: Double
    let triangleCount: Int
}

private func copiedBake(_ archive: Data) throws -> CopiedBake {
    var options = Dust3DSourceBakeMipOptions()
    options.struct_size = MemoryLayout<Dust3DSourceBakeMipOptions>.size
    options.source.struct_size = MemoryLayout<Dust3DSourceBakeOptions>.size
    options.source.legacy.struct_size = MemoryLayout<Dust3DBakeOptions>.size
    options.source.legacy.atlas_texture_size = 512
    options.source.legacy.color_size = 512
    options.source.legacy.normal_size = 512
    options.source.legacy.material_size = 512
    options.maximum_levels = 2
    var diagnostic = Dust3DError()
    let owner = archive.withUnsafeBytes { bytes in
        dust3d_source_baked_mesh_load_with_mips(
            bytes.bindMemory(to: UInt8.self).baseAddress, bytes.count, &options, &diagnostic)
    }
    guard let owner else {
        let message = withUnsafeBytes(of: diagnostic.message) { bytes in
            String(decoding: bytes.prefix { $0 != 0 }, as: UTF8.self)
        }
        throw MipSmokeError.invalidOutput(message)
    }
    defer { dust3d_source_baked_mesh_destroy(owner) }
    let cap = dust3d_source_baked_mesh_maximum_sampling_lod(owner)
    guard cap == 1, let geometry = dust3d_source_baked_mesh_geometry(owner) else {
        throw MipSmokeError.invalidOutput("Fixture must retain a real mip level")
    }
    let triangleCount = dust3d_source_baked_mesh_triangle_count(owner)
    guard triangleCount > 0, triangleCount <= Int.max / 3,
        dust3d_mesh_vertex_count(geometry) == triangleCount * 3
    else { throw MipSmokeError.invalidOutput("Misaligned source geometry") }
    var maps: [[CopiedLevel]] = []
    for map in UInt32(0)..<5 {
        let count = dust3d_source_baked_mesh_texture_level_count(owner, map)
        guard count == 0 || count == 2 else {
            throw MipSmokeError.invalidOutput("Inconsistent map level count")
        }
        var levels: [CopiedLevel] = []
        for level in 0..<count {
            let view = dust3d_source_baked_mesh_texture_level(owner, map, level)
            let dimension = level == 0 ? 512 : 256
            guard view.width == dimension, view.height == dimension,
                view.row_stride == dimension * 4, view.byte_count == dimension * dimension * 4,
                let pixels = view.pixels
            else { throw MipSmokeError.invalidOutput("Invalid retained image span") }
            levels.append(
                CopiedLevel(
                    width: view.width, height: view.height,
                    pixels: Data(bytes: pixels, count: view.byte_count)))
        }
        maps.append(levels)
    }
    guard maps[0].count == 2 else { throw MipSmokeError.invalidOutput("Missing color chain") }
    return CopiedBake(maps: maps, maximumSamplingLod: cap, triangleCount: triangleCount)
}

@main private struct SourceBakedMipSmoke {
    static func main() async throws {
        guard CommandLine.arguments.count == 2 else {
            throw MipSmokeError.invalidOutput("Expected licensed Capsule.ds3 path")
        }
        let archive = try Data(contentsOf: URL(fileURLWithPath: CommandLine.arguments[1]))
        let first = try copiedBake(archive)
        let second = try copiedBake(archive)
        // Native owners are already destroyed; only immutable copied bytes cross actors.
        let transferred = await Task.detached { first }.value
        guard transferred == second else {
            throw MipSmokeError.invalidOutput("Copied mip output changed after owner release")
        }
        print(
            "Swift 6 source mip ownership passed: \(first.triangleCount) triangles, cap \(first.maximumSamplingLod)"
        )
    }
}
