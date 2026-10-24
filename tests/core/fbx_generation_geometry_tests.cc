// SPDX-License-Identifier: MIT
#include "FbxGeometrySelection.h"
#include "FbxMeshScene.h"
#include "GenerationPipeline.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

using namespace dust3d::native;
namespace {
void require(bool condition, const char* cause)
{
    if (!condition)
        throw std::runtime_error(cause);
}
std::unique_ptr<dust3d::Snapshot> snapshot(const std::vector<uint8_t>& bytes)
{
    std::string warning;
    return readDocument(bytes.data(), bytes.size(), warning);
}
}
int main(int argc, char** argv)
{
    try {
        require(argc == 2, "Licensed ordinary fixture directory required");
        std::ifstream file(std::string(argv[1]) + "/Capsule.ds3", std::ios::binary);
        require(bool(file), "Capsule fixture missing");
        const std::vector<uint8_t> bytes { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
        std::lock_guard<std::mutex> lock(engineMutex());
        std::unique_ptr<FbxGeometrySelection> geometry;
        GenerationOutputs outputs;
        outputs.fbxGeometry = &geometry;
        auto preview = generateMesh(snapshot(bytes), "", nullptr, nullptr, nullptr, outputs);
        require(geometry && !geometry->object().vertices.empty(), "Original generated geometry not retained");
        require(geometry->object().triangles.size() == 80 && preview->vertices.size() == 240, "Ordinary Capsule source/preview topology changed");
        const auto& indices = geometry->sourceTriangleIndices();
        require(indices.size() == 80, "Source face correspondence incomplete");
        for (size_t index = 0; index < indices.size(); ++index)
            require(indices[index] == index, "Original face order changed");
        bool hasDoublePrecision = false;
        for (const auto& vertex : geometry->object().vertices) {
            for (size_t axis = 0; axis < 3; ++axis)
                hasDoublePrecision |= vertex[axis] != static_cast<double>(static_cast<float>(vertex[axis]));
        }
        require(hasDoublePrecision, "Export geometry reconstructed from float preview");
        const auto encoded = encodeFbxMeshScene(geometry->object());
        require(!encoded.bytes().empty(), "Retained geometry cannot form a static FBX scene");
        const auto retainedBytes = encoded.bytes();
        preview.reset();
        const auto other = generateMesh(snapshot(bytes), "");
        require(other->vertices.size() == 240 && encodeFbxMeshScene(geometry->object()).bytes() == retainedBytes, "Another generation invalidated retained geometry");
        auto* previous = geometry.get();
        outputs.maximumFbxGeometryBytes = 1;
        try {
            generateMesh(snapshot(bytes), "", nullptr, nullptr, nullptr, outputs);
            throw std::runtime_error("Tiny export geometry quota accepted");
        } catch (const DocumentError& cause) {
            require(cause.status() == DUST3D_STATUS_LIMIT_EXCEEDED && geometry.get() == previous, "Rejected export replaced prior owner or lost cause");
        }
        outputs.maximumFbxGeometryBytes = 0;
        auto replacement = generateMesh(snapshot(bytes), "", nullptr, nullptr, nullptr, outputs);
        require(geometry.get() != previous && replacement->vertices.size() == 240, "Successful generation did not replace output owner");
        std::cout << "Original FBX generation geometry contracts passed\n";
        return 0;
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}
