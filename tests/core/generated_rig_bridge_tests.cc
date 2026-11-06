// SPDX-License-Identifier: MIT
#include <Dust3DBridge.h>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
static_assert(sizeof(Dust3DGeneratedRigJoint) == 296 && offsetof(Dust3DGeneratedRigJoint, rest_world) == 40);
static_assert(sizeof(Dust3DGeneratedRigWeights) == 32);
void require(bool v, const char* m)
{
    if (!v)
        throw std::runtime_error(m);
}
std::vector<uint8_t> assignedCapsule(const char* path)
{
    std::ifstream file(path, std::ios::binary);
    require(file.good(), "Cannot read licensed Capsule fixture");
    std::string original((std::istreambuf_iterator<char>(file)), {});
    const auto start = original.find("<canvas");
    require(start != std::string::npos, "Capsule model missing");
    std::string model = original.substr(start);
    size_t pos = 0, count = 0;
    while ((pos = model.find("<edge ", pos)) != std::string::npos) {
        model.insert(pos + 6, "boneName=\"Body\" ");
        pos += 22;
        ++count;
    }
    require(count == 2, "Unexpected Capsule edge corpus");
    const std::string header = "<ds3><model name=\"model.xml\" offset=\"0\" size=\"" + std::to_string(model.size()) + "\"/></ds3>";
    const std::string prefix = "DUST3D 1.0 xml ";
    auto offset = std::to_string(prefix.size() + 10 + 1 + header.size());
    offset.insert(0, 10 - offset.size(), '0');
    const auto archive = prefix + offset + "\n" + header + model;
    return { archive.begin(), archive.end() };
}
int main(int argc, char** argv)
{
    try {
        require(argc == 2, "Expected Capsule path");
        auto bytes = assignedCapsule(argv[1]);
        Dust3DGeneratedRigBoneInput bone {};
        bone.name = "Body";
        bone.name_length = 4;
        Dust3DError error {};
        auto* owner = dust3d_generated_rig_load(bytes.data(), bytes.size(), "Test", 4, &bone, 1, nullptr, &error);
        if (!owner)
            throw std::runtime_error(error.message);
        require(error.status == DUST3D_STATUS_OK && !*error.message, "Success error not cleared");
        require(dust3d_generated_rig_joint_count(owner) == 1, "Actual joint count");
        const auto* joint = dust3d_generated_rig_joints(owner);
        require(joint && joint->parent_index == UINT32_MAX && !joint->reserved, "Root/sentinel/layout");
        require(std::abs(std::abs(joint->tail_position[1] - joint->head_position[1]) - .5) < 1e-5, "Bone not generated from assigned Capsule");
        const auto* mesh = dust3d_generated_rig_mesh(owner);
        const auto n = dust3d_mesh_vertex_count(mesh);
        const auto* weights = dust3d_generated_rig_weights(owner);
        require(n && n % 3 == 0 && weights && dust3d_mesh_node_count(mesh) == 3, "Grounded draw data");
        for (size_t i = 0; i < n; ++i) {
            require(weights[i].joints[0] == 0 && weights[i].weights[0] == 1, "Draw map lost source weights");
            for (size_t j = 1; j < 4; ++j)
                require(weights[i].joints[j] == UINT32_MAX && weights[i].weights[j] == 0, "Unused slot changed");
        }
        for (size_t c = 0; c < 4; ++c)
            for (size_t r = 0; r < 4; ++r) {
                double sum = 0;
                for (size_t k = 0; k < 4; ++k)
                    sum += joint->rest_world[k * 4 + r] * joint->inverse_bind[c * 4 + k];
                require(std::abs(sum - (c == r ? 1 : 0)) < 1e-6, "C inverse convention");
            }
        size_t length = 99;
        const auto* name = dust3d_generated_rig_joint_name(owner, 0, &length);
        require(name && length == 4 && std::string(name, length) == "Body", "Exact owned name");
        std::vector<Dust3DVertex> copied(dust3d_mesh_vertices(mesh), dust3d_mesh_vertices(mesh) + n);
        const Dust3DGeneratedRigJoint copiedJoint = *joint;
        const auto copiedWeight = weights[0];
        auto options = dust3d_generated_rig_default_options();
        options.reserved = 1;
        require(!dust3d_generated_rig_load(nullptr, 0, "Test", 4, &bone, 1, &options, &error) && error.status == DUST3D_STATUS_INVALID_DOCUMENT, "Versioned options reject");
        options = dust3d_generated_rig_default_options();
        options.maximum_output_bytes = 1;
        require(!dust3d_generated_rig_load(nullptr, 0, "Test", 4, &bone, 1, &options, &error) && error.status == DUST3D_STATUS_LIMIT_EXCEEDED, "Quota rejects before archive generation");
        Dust3DGeneratedRigBoneInput duplicate[2] = { bone, bone };
        require(!dust3d_generated_rig_load(nullptr, 0, "Test", 4, duplicate, 2, nullptr, &error) && error.status == DUST3D_STATUS_INVALID_DOCUMENT, "Duplicate names reject before archive");
        auto missingParent = bone;
        missingParent.parent = "Missing";
        missingParent.parent_length = 7;
        require(!dust3d_generated_rig_load(nullptr, 0, "Test", 4, &missingParent, 1, nullptr, &error) && error.status == DUST3D_STATUS_INVALID_DOCUMENT, "Missing parent rejects before archive");
        require(!dust3d_generated_rig_load(nullptr, 0, "T\0x", 3, &bone, 1, nullptr, &error) && error.status == DUST3D_STATUS_INVALID_DOCUMENT, "Embedded NUL type rejects");
        options = dust3d_generated_rig_default_options();
        options.maximum_draw_vertices = 1;
        require(!dust3d_generated_rig_load(bytes.data(), bytes.size(), "Test", 4, &bone, 1, &options, &error) && error.status == DUST3D_STATUS_LIMIT_EXCEEDED, "Draw staging quota rejects");
        Dust3DGeneratedRigBoneInput forward[2] = {};
        forward[0].name = "Child";
        forward[0].name_length = 5;
        forward[0].parent = "Body";
        forward[0].parent_length = 4;
        forward[1] = bone;
        auto* forest = dust3d_generated_rig_load(bytes.data(), bytes.size(), "Test", 4, forward, 2, nullptr, &error);
        require(forest && dust3d_generated_rig_joint_count(forest) == 2, "Forward parent owner");
        require(dust3d_generated_rig_joints(forest)[0].parent_index == 1 && dust3d_generated_rig_weights(forest)[0].joints[0] == 1, "Source joint order remapped incorrectly");
        dust3d_generated_rig_destroy(forest);
        options = dust3d_generated_rig_default_options();
        auto* token = dust3d_cancellation_token_create(&error);
        require(token, "Token create");
        dust3d_cancellation_token_cancel(token);
        options.cancellation = token;
        require(!dust3d_generated_rig_load(nullptr, 0, "Test", 4, &bone, 1, &options, &error) && error.status == DUST3D_STATUS_CANCELLED, "Typed precancel");
        dust3d_cancellation_token_destroy(token);
        require(dust3d_generated_rig_joint_name(owner, 9, &length) == nullptr && length == 0, "Invalid index clears span");
        require(dust3d_generated_rig_output_bytes(owner) > n * sizeof(Dust3DVertex) && !dust3d_generated_rig_eyelids_generated(owner), "Retained quota/flag");
        require(std::memcmp(joint, &copiedJoint, sizeof(copiedJoint)) == 0, "Later failure mutated prior owner");
        dust3d_generated_rig_destroy(owner);
        bytes.clear();
        require(copiedWeight.weights[0] == 1 && !copied.empty() && std::isfinite(copied[0].position[0]), "Copies did not outlive owner");
        require(!dust3d_generated_rig_mesh(nullptr) && !dust3d_generated_rig_joints(nullptr) && !dust3d_generated_rig_weights(nullptr) && !dust3d_generated_rig_joint_count(nullptr), "NULL accessors");
        dust3d_generated_rig_destroy(nullptr);
        std::cout << "Generated rig C ownership contracts passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
