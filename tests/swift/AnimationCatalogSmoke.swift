// SPDX-License-Identifier: MIT
import Dust3DEngine
import Foundation

private enum CatalogSmokeError: Error { case invalidMetadata(String) }
private struct CopiedParameter: Sendable, Equatable {
    let name: String
    let value: Double
}
private struct CopiedClip: Sendable, Equatable {
    let type: String
    let rig: String
    let generator: String
    let seconds: Double
    let samples: Int
    let loops: Bool
    let alias: Bool
    let parameters: [CopiedParameter]
}
private struct CopiedRig: Sendable, Equatable {
    let name: String
    let bones: [String]
}
private struct CopiedCatalog: Sendable, Equatable {
    let clips: [CopiedClip]
    let rigs: [CopiedRig]
}

private func copiedString(_ pointer: UnsafePointer<CChar>?) throws -> String {
    guard let pointer, let value = String(validatingCString: pointer), !value.isEmpty else {
        throw CatalogSmokeError.invalidMetadata("Missing or invalid UTF-8 identity")
    }
    return value
}
private func copyCatalog() throws -> CopiedCatalog {
    var error = Dust3DError()
    guard let owner = dust3d_animation_catalog_load(&error) else {
        let cause = withUnsafeBytes(of: error.message) { String(decoding: $0.prefix { $0 != 0 }, as: UTF8.self) }
        throw CatalogSmokeError.invalidMetadata(cause)
    }
    defer { dust3d_animation_catalog_destroy(owner) }
    guard error.status == DUST3D_STATUS_OK, dust3d_animation_catalog_clip_count(owner) == 180,
        dust3d_animation_catalog_rig_count(owner) == 7
    else {
        throw CatalogSmokeError.invalidMetadata("Inherited catalog inventory changed")
    }
    var clips: [CopiedClip] = []
    for index in 0..<dust3d_animation_catalog_clip_count(owner) {
        var clip = Dust3DAnimationClipInfo()
        guard dust3d_animation_catalog_clip_at(owner, index, &clip) == 1,
            clip.duration_seconds.isFinite, clip.duration_seconds > 0, clip.sample_count > 0,
            (0...1).contains(clip.loops), (0...1).contains(clip.is_legacy_alias)
        else {
            throw CatalogSmokeError.invalidMetadata("Invalid copied clip view")
        }
        var parameters: [CopiedParameter] = []
        for parameterIndex in 0..<clip.parameter_count {
            var parameter = Dust3DAnimationParameterInfo()
            guard dust3d_animation_catalog_parameter_at(owner, index, parameterIndex, &parameter) == 1,
                parameter.value.isFinite
            else {
                throw CatalogSmokeError.invalidMetadata("Invalid copied default override")
            }
            parameters.append(CopiedParameter(name: try copiedString(parameter.name), value: parameter.value))
        }
        clips.append(
            CopiedClip(
                type: try copiedString(clip.type), rig: try copiedString(clip.rig),
                generator: try copiedString(clip.generator),
                seconds: clip.duration_seconds, samples: clip.sample_count, loops: clip.loops == 1,
                alias: clip.is_legacy_alias == 1, parameters: parameters))
    }
    var rigs: [CopiedRig] = []
    for index in 0..<dust3d_animation_catalog_rig_count(owner) {
        var bones: [String] = []
        for bone in 0..<dust3d_animation_catalog_bone_count(owner, index) {
            bones.append(try copiedString(dust3d_animation_catalog_bone_name(owner, index, bone)))
        }
        rigs.append(CopiedRig(name: try copiedString(dust3d_animation_catalog_rig_name(owner, index)), bones: bones))
    }
    return CopiedCatalog(clips: clips, rigs: rigs)
}
private actor CatalogReceiver {
    private var catalog: CopiedCatalog?
    func receive(_ value: CopiedCatalog) { catalog = value }
    func snapshot() -> CopiedCatalog? { catalog }
}
@main
private enum AnimationCatalogSmoke {
    static func main() async throws {
        let first = try await Task.detached { try copyCatalog() }.value
        let second = try await Task.detached { try copyCatalog() }.value
        guard first == second, first.clips.filter(\.alias).count == 4,
            let sprint = first.clips.first(where: { $0.type == "BipedSprint" }), sprint.seconds == 0.65,
            sprint.samples == 32, sprint.loops,
            first.rigs.contains(where: { $0.name == "Fish" && $0.bones.contains("Jaw") })
        else { throw CatalogSmokeError.invalidMetadata("Owned catalog values changed after native owner destruction") }
        let receiver = CatalogReceiver()
        await receiver.receive(first)
        guard await receiver.snapshot() == second else {
            throw CatalogSmokeError.invalidMetadata("Catalog copy changed across actors")
        }
        print("Swift 6 animation catalog importer and owned Sendable copies passed")
    }
}
