// SPDX-License-Identifier: MIT
#include "Cancellation.h"
#include "EngineInternal.h"
#include "FbxGeometrySelection.h"
#include "GenerationPipeline.h"
#include "ObjDocument.h"
#include "SourceProvenance.h"
#include "UvMetadata.h"
#include <iostream>
#include <stdexcept>
using namespace dust3d;
using namespace dust3d::native;
namespace {
void require(bool condition, const char* cause)
{
    if (!condition)
        throw std::runtime_error(cause);
}
template <typename Operation>
void rejects(Operation operation, Dust3DStatus status)
{
    try {
        operation();
    } catch (const DocumentError& cause) {
        require(cause.status() == status, "Preparation lost typed cause");
        return;
    }
    throw std::runtime_error("Invalid output preparation accepted");
}
Object smallTriangle()
{
    Object object;
    object.vertices = { { 1000000, 1000000, 1000000 }, { 1000000.001, 1000000, 1000000 }, { 1000000, 1000000.001, 1000000 } };
    object.triangles = object.triangleAndQuads = { { 0, 1, 2 } };
    return object;
}
}
int main()
{
    try {
        std::lock_guard<std::mutex> lock(engineMutex());
        const auto object = smallTriangle();
        require(Vector3::crossProduct(object.vertices[1] - object.vertices[0], object.vertices[2] - object.vertices[0]).lengthSquared() > 0,
            "Controlled double triangle is degenerate");
        Dust3DMesh preview;
        rejects([&] { prepareGeneratedObject(object, nullptr, preview, {}); }, DUST3D_STATUS_GENERATION_FAILED);
        require(preview.vertices.empty(), "Collapsed preview gained render geometry");
        std::unique_ptr<OwnedObjDocument> obj;
        std::unique_ptr<FbxGeometrySelection> fbx;
        GenerationOutputs outputs;
        outputs.purpose = GenerationPurpose::originalExport;
        outputs.objDocument = &obj;
        outputs.fbxGeometry = &fbx;
        Dust3DMesh metadata;
        metadata.nodeCount = 3;
        metadata.warning = "source note";
        prepareGeneratedObject(object, nullptr, metadata, outputs);
        require(obj && fbx && metadata.vertices.empty() && metadata.vertices.capacity() == 0, "Original export depended on float preview storage/validity");
        require(metadata.nodeCount == 3 && metadata.warning == "source note", "Export preparation changed source metadata");
        require(fbx->object().vertices[1].x() == object.vertices[1].x()
                && fbx->object().vertices[1].x() != static_cast<double>(static_cast<float>(object.vertices[1].x())),
            "Original export lost sub-float coordinate distinctions");
        require(obj->bytes().size() > 30 && fbx->object().triangles == object.triangles, "Original export omitted source faces");
        GenerationOutputs missing;
        missing.purpose = GenerationPurpose::originalExport;
        Dust3DMesh empty;
        rejects([&] { prepareGeneratedObject(object, nullptr, empty, missing); }, DUST3D_STATUS_INVALID_DOCUMENT);
        Snapshot source;
        SourceProvenance provenance(source, 1024 * 1024);
        outputs.sources = &provenance;
        rejects([&] { prepareGeneratedObject(object, nullptr, empty, outputs); }, DUST3D_STATUS_INVALID_DOCUMENT);
        outputs.sources = nullptr;
        UvMetadata uvs(source, 1024 * 1024);
        outputs.uvs = &uvs;
        outputs.useFbxAtlasUvs = true;
        rejects([&] { prepareGeneratedObject(object, nullptr, empty, outputs); }, DUST3D_STATUS_GENERATION_FAILED);
        outputs.uvs = nullptr;
        outputs.useFbxAtlasUvs = false;
        outputs.purpose = static_cast<GenerationPurpose>(17);
        rejects([&] { prepareGeneratedObject(object, nullptr, empty, outputs); }, DUST3D_STATUS_INVALID_DOCUMENT);
        outputs.purpose = GenerationPurpose::originalExport;
        const auto* previousObj = obj.get();
        const auto* previousFbx = fbx.get();
        outputs.maximumFbxGeometryBytes = planFbxGeometrySelection(1, 1, 1, false, false).stagingBytes;
        rejects([&] { prepareGeneratedObject(object, nullptr, empty, outputs); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        require(obj.get() == previousObj && fbx.get() == previousFbx, "Failed original export partially published owners");
        outputs.maximumFbxGeometryBytes = 0;
        Dust3DCancellationToken cancellation;
        cancellation.cancelled.store(true);
        outputs.cancellation = &cancellation;
        rejects([&] { prepareGeneratedObject(object, nullptr, empty, outputs); }, DUST3D_STATUS_CANCELLED);
        require(obj.get() == previousObj && fbx.get() == previousFbx, "Cancellation partially replaced original outputs");
        outputs.cancellation = nullptr;
        auto ordinary = smallTriangle();
        ordinary.vertices = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } };
        Dust3DMesh renderable;
        prepareGeneratedObject(ordinary, nullptr, renderable, {});
        require(renderable.vertices.size() == 3, "Existing preview preparation regressed");
        rejects([&] { prepareGeneratedObject(ordinary, nullptr, renderable, {}); }, DUST3D_STATUS_INVALID_DOCUMENT);
        auto polygonsOnly = ordinary;
        polygonsOnly.triangles.clear();
        std::unique_ptr<OwnedObjDocument> polygons;
        GenerationOutputs polygonOutputs;
        polygonOutputs.purpose = GenerationPurpose::originalExport;
        polygonOutputs.objDocument = &polygons;
        Dust3DMesh polygonMetadata;
        prepareGeneratedObject(polygonsOnly, nullptr, polygonMetadata, polygonOutputs);
        require(polygons && polygonMetadata.vertices.capacity() == 0, "Original polygon export required preview triangles");
        std::cout << "Original exports are independent of float preview validity\n";
        return 0;
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}
