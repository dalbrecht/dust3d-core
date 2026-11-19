// SPDX-License-Identifier: MIT
#include "Cancellation.h"
#include "FbxGeometrySelection.h"
#include "GenerationPipeline.h"
#include <iostream>
#include <stdexcept>
using namespace dust3d;
using namespace dust3d::native;
void require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}
template <class F>
void rejects(F run, Dust3DStatus status)
{
    try {
        run();
    } catch (const DocumentError& error) {
        require(error.status() == status, "Wrong typed failure");
        return;
    }
    throw std::runtime_error("Expected rejection");
}
int main()
{
    try {
        std::lock_guard<std::mutex> lock(engineMutex());
        Object object;
        object.vertices = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 }, { 1, 1, 0 },
            { 1000000, 1000000, 1000000 }, { 1000000.001, 1000000, 1000000 }, { 1000000, 1000000.001, 1000000 } };
        object.triangles = { { 2, 0, 1 }, { 4, 5, 6 }, { 2, 1, 3 } };
        std::vector<size_t> mapping { 99 };
        GenerationOutputs outputs;
        outputs.previewVertexIndices = &mapping;
        Dust3DMesh mesh;
        prepareGeneratedObject(object, nullptr, mesh, outputs);
        require(mapping == std::vector<size_t>({ 2, 0, 1, 2, 1, 3 }), "Original corner identity/filtering lost");
        require(mapping.size() == mesh.vertices.size(), "Draw and mapping counts differ");
        for (size_t i = 0; i < mapping.size(); ++i)
            for (size_t axis = 0; axis < 3; ++axis)
                require(mesh.vertices[i].position[axis] == static_cast<float>(object.vertices[mapping[i]][axis]), "Mapped coordinate differs");
        Dust3DMesh defaultMesh;
        prepareGeneratedObject(object, nullptr, defaultMesh, {});
        require(defaultMesh.vertices.size() == mesh.vertices.size(), "Opt-in mapping changed default triangle admission");
        for (size_t i = 0; i < mesh.vertices.size(); ++i)
            for (size_t axis = 0; axis < 3; ++axis)
                require(defaultMesh.vertices[i].position[axis] == mesh.vertices[i].position[axis], "Opt-in mapping changed geometry");
        const auto previous = mapping;
        auto invalid = object;
        invalid.triangles.push_back({ 0, 1, 999 });
        Dust3DMesh failed;
        rejects([&] { prepareGeneratedObject(invalid, nullptr, failed, outputs); }, DUST3D_STATUS_GENERATION_FAILED);
        require(mapping == previous, "Failure partially replaced mapping");
        Dust3DCancellationToken token;
        token.cancelled.store(true);
        outputs.cancellation = &token;
        Dust3DMesh cancelled;
        rejects([&] { prepareGeneratedObject(object, nullptr, cancelled, outputs); }, DUST3D_STATUS_CANCELLED);
        require(mapping == previous, "Cancellation replaced mapping");
        outputs.cancellation = nullptr;
        outputs.purpose = GenerationPurpose::originalExport;
        std::unique_ptr<FbxGeometrySelection> exportOwner;
        outputs.fbxGeometry = &exportOwner;
        Dust3DMesh exported;
        rejects([&] { prepareGeneratedObject(object, nullptr, exported, outputs); }, DUST3D_STATUS_INVALID_DOCUMENT);
        require(mapping == previous, "Export replaced preview mapping");
        outputs.purpose = GenerationPurpose::preview;
        outputs.fbxGeometry = nullptr;
        Object collapsed = object;
        collapsed.triangles = { { 4, 5, 6 } };
        Dust3DMesh noGeometry;
        rejects([&] { prepareGeneratedObject(collapsed, nullptr, noGeometry, outputs); }, DUST3D_STATUS_GENERATION_FAILED);
        require(mapping == previous, "Collapsed output replaced mapping");
        object.vertices.clear();
        require(mapping == previous, "Mapping borrowed source storage");
        std::cout << "Preview original vertex mapping passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
