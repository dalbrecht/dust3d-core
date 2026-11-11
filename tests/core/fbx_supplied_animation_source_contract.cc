// SPDX-License-Identifier: MIT
// Unregistered source contract. Compile only while the animated writer is quarantined.
#include "FbxMeshScene.h"
#include <cassert>
#include <string_view>

void checkSuppliedFbxAnimation()
{
    using namespace dust3d;
    using namespace dust3d::native;
    Object mesh;
    mesh.vertices = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } };
    mesh.triangles = { { 0, 1, 2 } };
    const RigStructure rig { "Fish", "", { { "Root", "", 0, 0, 0, 0, 1, 0 } } };
    const std::vector<GlbSkinWeights> bindings(3, { { 0, 0, 0, 0 }, { 1, 0, 0, 0 } });

    BoneAnimationFrame start;
    start.boneWorldTransforms.emplace("Root", Matrix4x4 {});
    auto finish = start;
    finish.time = 1;
    finish.boneWorldTransforms.at("Root").translate(Vector3(0.25, 0, 0));
    RigAnimationClip clip;
    clip.name = "Authored Swim";
    clip.animationType = "FishIdle";
    clip.frames = { start, finish };

    const auto output = encodeFbxRestMeshScene(mesh, nullptr, rig, bindings, {}, { clip });
    const std::string_view bytes(reinterpret_cast<const char*>(output.bytes().data()), output.bytes().size());
    assert(bytes.find("Authored Swim") != std::string_view::npos);
    assert(bytes.find("AnimationStack") != std::string_view::npos);
    assert(bytes.find("KeyValueFloat") != std::string_view::npos);
}
