// SPDX-License-Identifier: MIT
#include "Cancellation.h"
#include "GeneratedRigInput.h"
#include "GenerationPipeline.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
using namespace dust3d;
using namespace dust3d::native;
void require(bool v, const char* m)
{
    if (!v)
        throw std::runtime_error(m);
}
template <class F>
void rejects(F run, Dust3DStatus expected)
{
    try {
        run();
    } catch (const DocumentError& e) {
        require(e.status() == expected && e.what()[0], "Wrong retained generation failure");
        return;
    }
    throw std::runtime_error("Expected retained generation rejection");
}
int main(int argc, char** argv)
{
    try {
        require(argc == 2, "Expected Capsule fixture path");
        std::ifstream file(argv[1], std::ios::binary);
        require(file.good(), "Cannot read Capsule");
        const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), {});
        std::lock_guard<std::mutex> lock(engineMutex());
        std::unique_ptr<GeneratedRigInput> owner;
        GenerationOutputs outputs;
        outputs.purpose = GenerationPurpose::rigInputOnly;
        outputs.rigInput = &owner;
        std::string warning;
        auto snapshot = readDocument(bytes.data(), bytes.size(), warning); // procedural route; no imported context
        auto metadata = generateMesh(std::move(snapshot), warning, nullptr, nullptr, nullptr, outputs);
        require(owner && !owner->object().vertices.empty() && !owner->object().triangles.empty(), "Full generated Object lost");
        require(owner->snapshot().nodes.size() >= 3 && metadata->nodeCount == 3 && metadata->partCount == 1, "Processed snapshot/source metadata lost");
        require(metadata->vertices.empty() && metadata->vertices.capacity() == 0, "Retained input allocated preview draw buffer");
        const auto* previous = owner.get();
        Dust3DCancellationToken token;
        token.cancelled.store(true);
        outputs.cancellation = &token;
        rejects([&] { generateMesh(readDocument(bytes.data(), bytes.size(), warning), warning, nullptr, nullptr, nullptr, outputs); }, DUST3D_STATUS_CANCELLED);
        require(owner.get() == previous, "Cancelled generation replaced old input owner");
        outputs.cancellation = nullptr;
        std::vector<size_t> indices { 99 };
        outputs.previewVertexIndices = &indices;
        rejects([&] { generateMesh(readDocument(bytes.data(), bytes.size(), warning), warning, nullptr, nullptr, nullptr, outputs); }, DUST3D_STATUS_INVALID_DOCUMENT);
        require(owner.get() == previous && indices == std::vector<size_t> { 99 }, "Invalid hooks replaced outputs");
        outputs.previewVertexIndices = nullptr;
        outputs.rigInput = nullptr;
        rejects([&] { generateMesh(readDocument(bytes.data(), bytes.size(), warning), warning, nullptr, nullptr, nullptr, outputs); }, DUST3D_STATUS_INVALID_DOCUMENT);
        std::cout << "Retained rig generation contracts passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
