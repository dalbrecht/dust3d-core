// SPDX-License-Identifier: MIT
import Dust3DEngine
import Foundation

private enum ExportSmokeError: Error {
    case invalidOutput(String)
    case native(Int, String)
}

private struct CopiedExport: Sendable, Equatable {
    let bytes: Data
    let payloadBytes: Int
    let warning: String
}

private func copiedExport(_ archive: Data) throws -> CopiedExport {
    var options = Dust3DFbxExportOptions()
    options.struct_size = MemoryLayout<Dust3DFbxExportOptions>.size
    options.mode = UInt32(DUST3D_FBX_EXPORT_BAKED)
    options.bake.struct_size = MemoryLayout<Dust3DBakeOptions>.size
    options.bake.atlas_texture_size = 64
    options.bake.color_size = 64
    options.bake.normal_size = 64
    options.bake.material_size = 64
    var diagnostic = Dust3DError()
    let owner = archive.withUnsafeBytes { bytes in
        dust3d_fbx_export_load(
            bytes.bindMemory(to: UInt8.self).baseAddress, bytes.count, &options, &diagnostic)
    }
    guard let owner else {
        let message = withUnsafeBytes(of: diagnostic.message) { bytes in
            String(decoding: bytes.prefix { $0 != 0 }, as: UTF8.self)
        }
        throw ExportSmokeError.native(Int(diagnostic.status.rawValue), message)
    }
    defer { dust3d_fbx_export_destroy(owner) }
    let count = dust3d_fbx_export_byte_count(owner)
    let payload = dust3d_fbx_export_payload_bytes(owner)
    guard count > 0, payload > count, let bytes = dust3d_fbx_export_bytes(owner),
        let warning = dust3d_fbx_export_warning(owner)
    else {
        throw ExportSmokeError.invalidOutput("Missing bounded FBX buffer or warning")
    }
    return CopiedExport(
        bytes: Data(bytes: bytes, count: count), payloadBytes: payload,
        warning: String(cString: warning))
}

private actor ExportReceiver {
    private var stored: CopiedExport?
    func receive(_ output: CopiedExport) { stored = output }
    func snapshot() -> CopiedExport? { stored }
}

@main
private enum FbxExportSmoke {
    static func main() async throws {
        guard CommandLine.arguments.count == 2 else {
            throw ExportSmokeError.invalidOutput("Expected controlled Capsule fixture")
        }
        let archive = try Data(contentsOf: URL(fileURLWithPath: CommandLine.arguments[1]))
        let first = try await Task.detached { try copiedExport(archive) }.value
        let second = try await Task.detached { try copiedExport(archive) }.value
        guard first.bytes.starts(with: "Kaydara FBX Binary".utf8),
            second.bytes.starts(with: "Kaydara FBX Binary".utf8)
        else {
            throw ExportSmokeError.invalidOutput("Copied export lost its FBX container")
        }
        let receiver = ExportReceiver()
        await receiver.receive(first)
        guard await receiver.snapshot() == first else {
            throw ExportSmokeError.invalidOutput("Owned export copy changed across actors")
        }
        print("Swift 6 FBX C importer and owned Sendable copy passed")
    }
}
