// SPDX-License-Identifier: MIT
#include <Dust3DBridge.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <rapidxml.hpp>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
Dust3DRigTemplateBoneInput root(const char* name = "Root", size_t bytes = 4)
{
    Dust3DRigTemplateBoneInput value {};
    value.name = name;
    value.name_length = bytes;
    value.to_position[1] = 1;
    return value;
}
void ownershipAndRendering()
{
    Dust3DError error {};
    auto options = dust3d_rig_template_default_options();
    require(options.version == 1 && options.struct_size == sizeof(options), "Versioned options");
    options.selected_bone_name = "Root";
    options.selected_bone_name_length = 4;
    auto bone = root();
    auto* owner = dust3d_rig_template_mesh_create(&bone, 1, &options, &error);
    require(owner && error.status == DUST3D_STATUS_OK && error.message[0] == 0, "Owned template result");
    require(dust3d_rig_template_mesh_vertex_count(owner) == 1764, "Complete expanded triangles");
    require(dust3d_rig_template_mesh_bone_count(owner) == 1, "Owned bone metadata");
    size_t size = 99;
    const auto* name = dust3d_rig_template_mesh_bone_name(owner, 0, &size);
    require(name && size == 4 && std::memcmp(name, "Root", 4) == 0, "Owned exact name");
    const auto* vertices = dust3d_rig_template_mesh_vertices(owner);
    size_t selected = 0, rings = 0;
    std::vector<Dust3DRigTemplateDrawVertex> copied(vertices, vertices + 1764);
    for (const auto& vertex : copied) {
        require(vertex.bone_index == 0, "Source bone index");
        double squared = 0;
        for (size_t axis = 0; axis < 3; ++axis) {
            require(std::isfinite(vertex.position[axis]) && std::isfinite(vertex.normal[axis]), "Finite draw data");
            squared += vertex.normal[axis] * vertex.normal[axis];
        }
        require(std::abs(squared - 1) < .00001, "Reference flat unit normal");
        if (vertex.role == DUST3D_RIG_TEMPLATE_SELECTED_BONE)
            ++selected;
        if (vertex.role == DUST3D_RIG_TEMPLATE_RING)
            ++rings;
    }
    require(selected == 36 && rings == 1728, "Selected solids and separate ring roles");
    const auto charge = dust3d_rig_template_mesh_output_bytes(owner);
    require(charge == 1764 * sizeof(Dust3DRigTemplateDrawVertex) + 5, "Documented retained logical output");
    options.maximum_output_bytes = charge;
    auto* second = dust3d_rig_template_mesh_create(&bone, 1, &options, &error);
    require(second != nullptr, "Exact quota accepted");
    dust3d_rig_template_mesh_destroy(second);
    --options.maximum_output_bytes;
    require(!dust3d_rig_template_mesh_create(&bone, 1, &options, &error) && error.status == DUST3D_STATUS_LIMIT_EXCEEDED, "Short quota atomically rejects");
    require(std::memcmp(vertices, copied.data(), copied.size() * sizeof(copied[0])) == 0, "Later success/failure cannot change prior owner");
    dust3d_rig_template_mesh_destroy(owner);
    require(copied.size() == 1764 && std::isfinite(copied[0].position[0]), "Caller copy outlives owner");
}
void exactUtf8AndInvalidInputs()
{
    Dust3DError error {};
    std::string composed = "Caf\xc3\xa9", decomposed = "Cafe\xcc\x81";
    Dust3DRigTemplateBoneInput bones[] = { root(composed.data(), composed.size()), root(decomposed.data(), decomposed.size()) };
    auto options = dust3d_rig_template_default_options();
    options.selected_bone_name = composed.data();
    options.selected_bone_name_length = composed.size();
    auto* owner = dust3d_rig_template_mesh_create(bones, 2, &options, &error);
    require(owner != nullptr, "Byte-distinct names remain distinct");
    composed.assign("Other");
    decomposed.assign("Other");
    size_t size = 0;
    const auto* name = dust3d_rig_template_mesh_bone_name(owner, 0, &size);
    require(size == 5 && std::memcmp(name, "Caf\xc3\xa9", size) == 0, "Names copied before call returns");
    dust3d_rig_template_mesh_destroy(owner);
    const char invalid[] = { char(0xc0), char(0xaf) };
    auto bone = root(invalid, sizeof(invalid));
    require(!dust3d_rig_template_mesh_create(&bone, 1, nullptr, &error) && error.status == DUST3D_STATUS_INVALID_DOCUMENT, "Malformed UTF8 rejects");
    bone = root();
    options = dust3d_rig_template_default_options();
    options.version = 2;
    require(!dust3d_rig_template_mesh_create(&bone, 1, &options, &error) && error.status == DUST3D_STATUS_INVALID_DOCUMENT, "Unknown version rejects");
    options = dust3d_rig_template_default_options();
    options.struct_size = 0;
    require(!dust3d_rig_template_mesh_create(&bone, 1, &options, &error), "Wrong struct size rejects");
    options = dust3d_rig_template_default_options();
    options.normalize = 2;
    require(!dust3d_rig_template_mesh_create(&bone, 1, &options, &error), "Invalid boolean rejects");
    require(!dust3d_rig_template_mesh_create(nullptr, 1, nullptr, &error), "Null input span rejects");
    auto* empty = dust3d_rig_template_mesh_create(nullptr, 0, nullptr, &error);
    require(empty && dust3d_rig_template_mesh_vertex_count(empty) == 0 && !dust3d_rig_template_mesh_vertices(empty), "Owned empty distinct from failure");
    require(error.status == DUST3D_STATUS_OK && error.message[0] == 0, "Success clears original error");
    dust3d_rig_template_mesh_destroy(empty);
}
void licensedTemplateCorpus(const std::string& directory)
{
    size_t total = 0;
    for (const auto* type : { "biped", "bird", "fish", "insect", "quadruped", "snake", "spider" }) {
        std::ifstream file(directory + "/rig_" + type + ".xml");
        require(file.good(), "Licensed XML input exists");
        std::ostringstream stream;
        stream << file.rdbuf();
        std::string xml = stream.str();
        require(xml.size() <= 16384, "Pinned XML input bound");
        xml.push_back(0);
        rapidxml::xml_document<> document;
        document.parse<0>(xml.data());
        auto* rig = document.first_node("rig");
        require(rig != nullptr, "Template root");
        std::vector<Dust3DRigTemplateBoneInput> bones;
        auto coordinate = [](rapidxml::xml_node<>* node, const char* axis) {
            require(node != nullptr, "Coordinate element");
            auto* value = node->first_attribute(axis);
            require(value != nullptr, "Coordinate attribute");
            char* end = nullptr;
            float result = std::strtof(value->value(), &end);
            require(end != value->value() && *end == 0 && std::isfinite(result), "Qt Float32 coordinate");
            return result;
        };
        for (auto* element = rig->first_node("bone"); element; element = element->next_sibling("bone")) {
            auto* name = element->first_attribute("name");
            require(name != nullptr, "Exact name");
            Dust3DRigTemplateBoneInput bone {};
            bone.name = name->value();
            bone.name_length = name->value_size();
            size_t axis = 0;
            for (auto* key : { "x", "y", "z" }) {
                bone.from_position[axis] = coordinate(element->first_node("position"), key);
                bone.to_position[axis] = coordinate(element->first_node("endPosition"), key);
                ++axis;
            }
            bones.push_back(bone);
        }
        require(!bones.empty(), "Corpus bones");
        auto options = dust3d_rig_template_default_options();
        options.selected_bone_name = bones[0].name;
        options.selected_bone_name_length = bones[0].name_length;
        Dust3DError error {};
        auto* owner = dust3d_rig_template_mesh_create(bones.data(), bones.size(), &options, &error);
        require(owner != nullptr, "Every licensed template crosses the real C boundary");
        require(dust3d_rig_template_mesh_vertex_count(owner) == bones.size() * 1764, "Corpus complete flat geometry");
        require(dust3d_rig_template_mesh_bone_count(owner) == bones.size(), "Corpus source indices");
        size_t selected = 0;
        for (size_t i = 0; i < bones.size(); ++i) {
            size_t bytes = 0;
            const auto* name = dust3d_rig_template_mesh_bone_name(owner, i, &bytes);
            require(name && bytes == bones[i].name_length && !std::memcmp(name, bones[i].name, bytes), "Corpus exact name order");
        }
        const auto* draw = dust3d_rig_template_mesh_vertices(owner);
        for (size_t i = 0; i < dust3d_rig_template_mesh_vertex_count(owner); ++i) {
            require(draw[i].bone_index < bones.size(), "Corpus valid draw identity");
            if (draw[i].role == DUST3D_RIG_TEMPLATE_SELECTED_BONE)
                ++selected;
            for (size_t axis = 0; axis < 3; ++axis)
                require(std::isfinite(draw[i].position[axis]) && std::isfinite(draw[i].normal[axis]), "Corpus finite C draw data");
        }
        require(selected == 36, "Only selected solid receives selected role");
        total += bones.size();
        dust3d_rig_template_mesh_destroy(owner);
    }
    require(total == 172, "All seven source templates covered");
}
void boundedBoundaryFailures()
{
    Dust3DError error {};
    auto bone = root();
    auto rejects = [&](Dust3DRigTemplateOptions options, Dust3DStatus status) {
        auto* result = dust3d_rig_template_mesh_create(&bone, 1, &options, &error);
        const bool valid = !result && error.status == status && error.message[0];
        dust3d_rig_template_mesh_destroy(result);
        require(valid, "Boundary rejects with status and original cause");
    };
    auto options = dust3d_rig_template_default_options();
    options.reserved = 1;
    rejects(options, DUST3D_STATUS_INVALID_DOCUMENT);
    options = dust3d_rig_template_default_options();
    options.maximum_bones = 1;
    auto two = std::vector<Dust3DRigTemplateBoneInput> { bone, bone };
    require(!dust3d_rig_template_mesh_create(two.data(), two.size(), &options, &error) && error.status == DUST3D_STATUS_LIMIT_EXCEEDED, "Count rejects before copying names");
    options.maximum_bones = 0;
    rejects(options, DUST3D_STATUS_INVALID_DOCUMENT);
    options.maximum_bones = 257;
    rejects(options, DUST3D_STATUS_INVALID_DOCUMENT);
    options = dust3d_rig_template_default_options();
    options.maximum_vertices = 1763;
    rejects(options, DUST3D_STATUS_LIMIT_EXCEEDED);
    options = dust3d_rig_template_default_options();
    options.maximum_triangles = 587;
    rejects(options, DUST3D_STATUS_LIMIT_EXCEEDED);
    options = dust3d_rig_template_default_options();
    options.selected_bone_name_length = 1;
    rejects(options, DUST3D_STATUS_INVALID_DOCUMENT);
    options = dust3d_rig_template_default_options();
    options.start_radius = 0;
    rejects(options, DUST3D_STATUS_INVALID_DOCUMENT);
    options = dust3d_rig_template_default_options();
    bone.capsule_radius = -1;
    rejects(options, DUST3D_STATUS_INVALID_DOCUMENT);
    bone = root();
    bone.to_position[1] = INFINITY;
    rejects(options, DUST3D_STATUS_INVALID_DOCUMENT);
    bone = root();
    bone.name = nullptr;
    rejects(options, DUST3D_STATUS_INVALID_DOCUMENT);
    bone = root();
    bone.name_length = 1025;
    rejects(options, DUST3D_STATUS_LIMIT_EXCEEDED);
    bone = root();
    auto* owner = dust3d_rig_template_mesh_create(&bone, 1, nullptr, nullptr);
    require(owner != nullptr, "Optional error output");
    size_t bytes = 9;
    require(!dust3d_rig_template_mesh_bone_name(owner, 1, &bytes) && bytes == 0, "Invalid bone index clears length");
    require(dust3d_rig_template_mesh_bone_name(owner, 0, nullptr) != nullptr, "Optional name length output");
    dust3d_rig_template_mesh_destroy(owner);
}
void nullAccessorsAndCancellation()
{
    size_t size = 9;
    require(!dust3d_rig_template_mesh_bone_name(nullptr, 0, &size) && size == 0, "Null name accessor clears output");
    require(!dust3d_rig_template_mesh_vertices(nullptr) && dust3d_rig_template_mesh_vertex_count(nullptr) == 0, "Null draw accessors");
    require(dust3d_rig_template_mesh_bone_count(nullptr) == 0 && dust3d_rig_template_mesh_output_bytes(nullptr) == 0, "Null metadata accessors");
    dust3d_rig_template_mesh_destroy(nullptr);
    Dust3DError error {};
    auto* token = dust3d_cancellation_token_create(&error);
    require(token != nullptr, "Fresh cooperative cancellation token");
    dust3d_cancellation_token_cancel(token);
    auto options = dust3d_rig_template_default_options();
    options.cancellation = token;
    auto bone = root();
    require(!dust3d_rig_template_mesh_create(&bone, 1, &options, &error) && error.status == DUST3D_STATUS_CANCELLED, "Cancellation before publication");
    dust3d_cancellation_token_destroy(token);
}
}
int main(int argc, char** argv)
{
    try {
        require(argc == 2, "Pass licensed resources directory");
        licensedTemplateCorpus(argv[1]);
        ownershipAndRendering();
        boundedBoundaryFailures();
        exactUtf8AndInvalidInputs();
        nullAccessorsAndCancellation();
        std::cout << "Owned rig template bridge contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
