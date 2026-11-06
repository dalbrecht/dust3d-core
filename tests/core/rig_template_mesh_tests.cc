// SPDX-License-Identifier: MIT
// Source-derived cases from application/sources/rig_skeleton_mesh_generator.cc, core #319.
#include "RigTemplateMesh.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <rapidxml.hpp>
#include <sstream>
#include <stdexcept>
using namespace dust3d::native;
namespace {
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
void near(double value, double expected)
{
    require(std::abs(value - expected) < 1e-9, "Source-derived coordinate mismatch");
}
void rejects(const std::function<void()>& operation, RigTemplateMeshFailure expected)
{
    try {
        operation();
    } catch (const RigTemplateMeshError& error) {
        require(error.failure() == expected, "Wrong typed failure");
        return;
    }
    throw std::runtime_error("Expected rejection before geometry publication");
}
RigTemplateBone root(std::string name = "Root")
{
    return { std::move(name), { 0, 0, 0 }, { 0, 1, 0 }, 0 };
}
void geometryAndRoles()
{
    RigTemplateMeshOptions options;
    options.normalize = false;
    options.selectedBoneName = "Root";
    const auto mesh = buildRigTemplateMesh({ root() }, options);
    // 8 tapered-solid vertices + (32 ring samples * 3 corner positions * 3 section vertices).
    require(mesh.vertices.size() == 296, "Full ring geometry must be retained");
    require(mesh.triangles.size() == 588, "576 ring triangles plus 12 bone triangles");
    near(mesh.vertices[0].position.x(), .02);
    near(mesh.vertices[0].position.y(), 0);
    near(mesh.vertices[4].position.x(), .004);
    near(mesh.vertices[4].position.y(), 1);
    require(mesh.triangles[576] == std::array<uint32_t, 3> { 0, 1, 2 }, "Preserve reference face winding/order");
    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        const auto& vertex = mesh.vertices[i];
        require(vertex.boneIndex == 0, "Preserve source bone identity");
        require(vertex.role == (i < 8 ? RigTemplateVertexRole::selectedBone : RigTemplateVertexRole::ring), "Ring is not selected-bone color");
        for (size_t axis = 0; axis < 3; ++axis)
            require(std::isfinite(vertex.position[axis]), "Finite owned vertices");
    }
    for (const auto& triangle : mesh.triangles)
        for (auto index : triangle)
            require(index < mesh.vertices.size(), "Valid owned triangle indices");
    const auto normalized = buildRigTemplateMesh({ root() });
    near(normalized.vertices[0].position.x(), .03);
    near(normalized.vertices[0].position.y(), -.75);
    near(normalized.vertices[4].position.y(), .75);
    require(normalized.vertices[0].role == RigTemplateVertexRole::bone, "Unselected bone role");
    auto tiny = root();
    tiny.toPosition = { 0, 0.0000001, 0 };
    const auto shortMesh = buildRigTemplateMesh({ tiny }, options);
    require(shortMesh.vertices.size() == 8 && shortMesh.triangles.size() == 12, "Reference tiny-axis ring skip");
    require(buildRigTemplateMesh({}).vertices.empty(), "Empty input has no output");
    auto capsule = root();
    capsule.capsuleRadius = .1;
    const auto wideRing = buildRigTemplateMesh({ capsule }, options);
    double largestRingX = 0;
    for (const auto& vertex : wideRing.vertices)
        if (vertex.role == RigTemplateVertexRole::ring)
            largestRingX = std::max(largestRingX, std::abs(vertex.position.x()));
    require(largestRingX > .124 && largestRingX < .127, "Capsule ring uses radius*1.25 plus reference rope section");
}
void exactNamesAndIndependentOwnership()
{
    RigTemplateMeshOptions options;
    options.selectedBoneName = "Caf\xc3\xa9";
    const auto first = buildRigTemplateMesh({ root("Caf\xc3\xa9"), root("Cafe\xcc\x81") }, options);
    require(first.vertices.size() == 592 && first.triangles.size() == 1176, "Both exact names retained");
    require(std::count_if(first.vertices.begin(), first.vertices.end(), [](const auto& vertex) { return vertex.role == RigTemplateVertexRole::selectedBone; }) == 8, "Unicode identity must not normalize");
    const auto copied = first.vertices[0].position;
    options.selectedBoneName = "Other";
    const auto second = buildRigTemplateMesh({ root() }, options);
    near(first.vertices[0].position.x(), copied.x());
    require(second.vertices[0].role == RigTemplateVertexRole::bone, "Later loads do not share ownership");
}
void validationAndQuotas()
{
    rejects([] { buildRigTemplateMesh({ root(), root() }); }, RigTemplateMeshFailure::invalidInput);
    auto invalid = root();
    invalid.fromPosition = { std::numeric_limits<double>::quiet_NaN(), 0, 0 };
    rejects([&] { buildRigTemplateMesh({ invalid }); }, RigTemplateMeshFailure::invalidInput);
    invalid = root();
    invalid.capsuleRadius = -1;
    rejects([&] { buildRigTemplateMesh({ invalid }); }, RigTemplateMeshFailure::invalidInput);
    invalid = root();
    invalid.toPosition = invalid.fromPosition;
    rejects([&] { buildRigTemplateMesh({ invalid }); }, RigTemplateMeshFailure::invalidInput);
    rejects([] { buildRigTemplateMesh({ root("") }); }, RigTemplateMeshFailure::invalidInput);
    rejects([] { buildRigTemplateMesh({ root(std::string(1025, 'a')) }); }, RigTemplateMeshFailure::limitExceeded);
    rejects([] { buildRigTemplateMesh(std::vector<RigTemplateBone>(257, root())); }, RigTemplateMeshFailure::limitExceeded);
    RigTemplateMeshOptions options;
    options.maximumVertices = 295;
    rejects([&] { buildRigTemplateMesh({ root() }, options); }, RigTemplateMeshFailure::limitExceeded);
    options = {};
    options.maximumTriangles = 587;
    rejects([&] { buildRigTemplateMesh({ root() }, options); }, RigTemplateMeshFailure::limitExceeded);
    options = {};
    options.startRadius = 0;
    rejects([&] { buildRigTemplateMesh({ root() }, options); }, RigTemplateMeshFailure::invalidInput);
    for (size_t RigTemplateMeshOptions::* limit : { &RigTemplateMeshOptions::maximumBones, &RigTemplateMeshOptions::maximumVertices, &RigTemplateMeshOptions::maximumTriangles, &RigTemplateMeshOptions::maximumOutputBytes }) {
        options = {};
        options.*limit = 0;
        rejects([&] { buildRigTemplateMesh({ root() }, options); }, RigTemplateMeshFailure::invalidInput);
    }
    options = {};
    options.maximumBones = 257;
    rejects([&] { buildRigTemplateMesh({ root() }, options); }, RigTemplateMeshFailure::invalidInput);
    options = {};
    options.startRadius = std::numeric_limits<double>::infinity();
    rejects([&] { buildRigTemplateMesh({ root() }, options); }, RigTemplateMeshFailure::invalidInput);
    options = {};
    options.startRadius = 1000001;
    rejects([&] { buildRigTemplateMesh({ root() }, options); }, RigTemplateMeshFailure::limitExceeded);
    options = {};
    options.selectedBoneName = std::string(1025, 'x');
    rejects([&] { buildRigTemplateMesh({ root() }, options); }, RigTemplateMeshFailure::limitExceeded);
    rejects([] { buildRigTemplateMesh({ root("bad\nname") }); }, RigTemplateMeshFailure::invalidInput);
    invalid = root();
    invalid.toPosition = { 1000001, 0, 0 };
    rejects([&] { buildRigTemplateMesh({ invalid }); }, RigTemplateMeshFailure::limitExceeded);
    invalid = root();
    invalid.capsuleRadius = 1000001;
    rejects([&] { buildRigTemplateMesh({ invalid }); }, RigTemplateMeshFailure::limitExceeded);
    options = {};
    options.startRadius = .00001;
    auto tiny = root();
    tiny.toPosition = { 0, .0000001, 0 };
    const auto unscaled = buildRigTemplateMesh({ tiny }, options);
    near(unscaled.vertices[0].position.x(), .00001);
    options = {};
    options.maximumOutputBytes = 296 * sizeof(RigTemplateVertex) + 588 * sizeof(std::array<uint32_t, 3>);
    require(buildRigTemplateMesh({ root() }, options).vertices.size() == 296, "Exact logical payload quota accepted");
    --options.maximumOutputBytes;
    rejects([&] { buildRigTemplateMesh({ root() }, options); }, RigTemplateMeshFailure::limitExceeded);
}
void licensedTemplateCorpus(const std::string& directory)
{
    for (const auto& name : { "biped", "bird", "fish", "insect", "quadruped", "snake", "spider" }) {
        std::ifstream input(directory + "/rig_" + name + ".xml");
        require(input.good(), "Licensed template input missing");
        std::ostringstream stream;
        stream << input.rdbuf();
        std::string xml = stream.str();
        require(xml.size() <= 16384, "Pinned template input bound");
        xml.push_back('\0');
        rapidxml::xml_document<> document;
        document.parse<0>(xml.data());
        auto* rig = document.first_node("rig");
        require(rig != nullptr, "Template rig root");
        std::vector<RigTemplateBone> bones;
        auto coordinate = [](rapidxml::xml_node<>* element, const char* axis) {
            require(element != nullptr, "Template coordinate element");
            auto* attribute = element->first_attribute(axis);
            require(attribute != nullptr, "Template coordinate attribute");
            char* end = nullptr;
            const float value = std::strtof(attribute->value(), &end);
            require(end != attribute->value() && *end == '\0' && std::isfinite(value), "Source Qt float coordinate");
            return static_cast<double>(value);
        };
        for (auto* bone = rig->first_node("bone"); bone; bone = bone->next_sibling("bone")) {
            auto* identity = bone->first_attribute("name");
            require(identity != nullptr, "Template bone name");
            auto* from = bone->first_node("position");
            auto* to = bone->first_node("endPosition");
            bones.push_back({ std::string(identity->value(), identity->value_size()),
                { coordinate(from, "x"), coordinate(from, "y"), coordinate(from, "z") },
                { coordinate(to, "x"), coordinate(to, "y"), coordinate(to, "z") }, 0 });
        }
        require(!bones.empty(), "Template bones retained");
        RigTemplateMeshOptions options;
        options.selectedBoneName = bones.front().name;
        const auto mesh = buildRigTemplateMesh(bones, options);
        require(mesh.vertices.size() == bones.size() * 296 && mesh.triangles.size() == bones.size() * 588, "Every licensed template retains solids and rings");
        require(std::count_if(mesh.vertices.begin(), mesh.vertices.end(), [](const auto& vertex) { return vertex.role == RigTemplateVertexRole::selectedBone; }) == 8, "Selected corpus bone classification");
        auto minimum = mesh.vertices.front().position, maximum = minimum;
        for (const auto& vertex : mesh.vertices) {
            require(vertex.boneIndex < bones.size(), "Corpus bone index");
            for (size_t axis = 0; axis < 3; ++axis) {
                require(std::isfinite(vertex.position[axis]), "Finite corpus output");
                minimum[axis] = std::min(minimum[axis], vertex.position[axis]);
                maximum[axis] = std::max(maximum[axis], vertex.position[axis]);
            }
        }
        const auto extent = maximum - minimum;
        near(std::max({ extent.x(), extent.y(), extent.z() }), 1.5);
        for (const auto& triangle : mesh.triangles)
            for (auto index : triangle)
                require(index < mesh.vertices.size(), "Corpus triangle index");
        std::cout << name << ": " << bones.size() << " bones, " << mesh.vertices.size() << " vertices, " << mesh.triangles.size() << " triangles\n";
    }
}

}
int main(int argc, char** argv)
{
    try {
        geometryAndRoles();
        exactNamesAndIndependentOwnership();
        validationAndQuotas();
        require(argc == 2, "Pass licensed application/resources directory");
        licensedTemplateCorpus(argv[1]);
        std::cout << "Rig template geometry contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
