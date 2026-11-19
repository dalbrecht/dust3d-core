// SPDX-License-Identifier: MIT
import Foundation
import Dust3DEngine
let data = try Data(contentsOf: URL(fileURLWithPath: CommandLine.arguments[1]))
var error = Dust3DError()
var bone = Dust3DGeneratedRigBoneInput()
let owner = "Body".withCString { name in
    "Test".withCString { type in
        bone.name = name
        bone.name_length = 4
        return data.withUnsafeBytes { bytes in
            withUnsafePointer(to: &bone) { input in
                dust3d_generated_rig_load(bytes.bindMemory(to: UInt8.self).baseAddress, bytes.count, type, 4, input, 1, nil, &error)
            }
        }
    }
}
guard let owner else { fatalError("Generated rig load failed: \(error.status)") }
precondition(error.status == DUST3D_STATUS_OK)
let options = dust3d_generated_rig_default_options()
precondition(options.version == 1 && Int(options.struct_size) == MemoryLayout<Dust3DGeneratedRigOptions>.size)
guard let mesh = dust3d_generated_rig_mesh(owner), let vertices = dust3d_mesh_vertices(mesh),
      let joints = dust3d_generated_rig_joints(owner), let weights = dust3d_generated_rig_weights(owner),
      let name = dust3d_generated_rig_joint_name(owner, 0, nil) else { fatalError("Missing generated rig views") }
let count = dust3d_mesh_vertex_count(mesh)
let copiedVertices = Array(UnsafeBufferPointer(start: vertices, count: count))
let copiedWeights = Array(UnsafeBufferPointer(start: weights, count: count))
let copiedJoint = joints.pointee
let copiedName = String(cString: name)
precondition(dust3d_generated_rig_joint_count(owner) == 1 && count > 0)
dust3d_generated_rig_destroy(owner)
precondition(copiedName == "Body" && copiedJoint.parent_index == UInt32.max)
precondition(copiedVertices[0].position.0.isFinite && copiedWeights[0].joints.0 == UInt32.max && copiedWeights[0].weights.0 == 0)
print("Swift generated rig ownership passed")
