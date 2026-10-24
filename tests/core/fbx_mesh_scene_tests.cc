// SPDX-License-Identifier: MIT
#include "EngineInternal.h"
#include "FbxMeshScene.h"
#include "FbxMeshScenePlan.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace dust3d;
using namespace dust3d::native;
namespace {
void require(bool value, const char* cause)
{
    if (!value)
        throw std::runtime_error(cause);
}
template <typename Operation>
void rejects(Operation operation, Dust3DStatus status)
{
    try {
        operation();
    } catch (const DocumentError& cause) {
        require(cause.status() == status, "Mesh scene rejection lost typed cause");
        return;
    }
    throw std::runtime_error("Invalid mesh scene admitted");
}
Object fixture()
{
    Object source;
    source.vertices = { { 0, 0, 0 }, { 2, 0, 0 }, { 0, 3, 0 }, { 0, 0, 4 } };
    source.triangles = { { 0, 1, 2 }, { 0, 3, 1 } };
    source.setTriangleVertexNormals({ { { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 } }, { { 0, 1, 0 }, { 0, 1, 0 }, { 0, 1, 0 } } });
    source.setTriangleVertexUvs({ { { 0, 0 }, { 1, 0 }, { 0, 1 } }, { { 0.25, 0.25 }, { 0.5, 0.5 }, { 0.75, 0.75 } } });
    return source;
}
}
int main(int argc, char** argv)
{
    try {
        const auto plan = planFbxMeshSceneGeometry(4, 2, true, true);
        require(plan.positionValueCount == 12 && plan.polygonIndexCount == 6
                && plan.normalValueCount == 18 && plan.textureCoordinateValueCount == 12 && plan.stagingBytes == 360,
            "FBX logical staging plan changed");
        const auto plainPlan = planFbxMeshSceneGeometry(1000000, 5000000, false, false);
        require(plainPlan.stagingBytes == 84000000 && plainPlan.normalValueCount == 0 && plainPlan.textureCoordinateValueCount == 0,
            "FBX count ceiling admission changed");
        require(planFbxMeshSceneGeometry(4, 2, true, false).stagingBytes == 264
                && planFbxMeshSceneGeometry(4, 2, false, true).stagingBytes == 216,
            "FBX independent corner-stream admission changed");
        rejects([] { planFbxMeshSceneGeometry(0, 2, false, false); }, DUST3D_STATUS_INVALID_DOCUMENT);
        rejects([] { planFbxMeshSceneGeometry(4, 0, false, false); }, DUST3D_STATUS_INVALID_DOCUMENT);
        rejects([] { planFbxMeshSceneGeometry(1000001, 2, false, false); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        rejects([] { planFbxMeshSceneGeometry(4, 5000001, false, false); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        rejects([] { planFbxMeshSceneGeometry(std::numeric_limits<size_t>::max(), 2, false, false); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        rejects([] { planFbxMeshSceneGeometry(4, std::numeric_limits<size_t>::max(), false, false); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        rejects([] { planFbxMeshSceneGeometry(1000000, 5000000, true, true); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        auto source = fixture();
        const auto first = encodeFbxMeshScene(source);
        const auto second = encodeFbxMeshScene(source);
        FbxPropertyBlock expectedIndices;
        expectedIndices.appendInt32Array({ 0, 1, -3, 0, 3, -2 });
        require(std::search(first.bytes().begin(), first.bytes().end(), expectedIndices.bytes().begin(), expectedIndices.bytes().end()) != first.bytes().end(), "Original shared vertex indices or polygon terminators changed");
        rejects([&] { encodeFbxMeshScene(source, { 256 * 1024 * 1024 + 1, 65536 }); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        rejects([&] { encodeFbxMeshScene(source, { 0, 0 }); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        rejects([&] { encodeFbxMeshScene(source, { 0, 65537 }); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        require(first.bytes() == second.bytes(), "Scene encoding is nondeterministic");
        require(source.vertices.size() == 4 && source.triangles[1][1] == 3 && source.triangleVertexUvs()->at(0)[0].y() == 0, "Scene encoding mutated source topology/UV");
        auto owned = [&] { auto temporary = fixture(); return encodeFbxMeshScene(temporary); }();
        require(owned.bytes() == first.bytes(), "Scene output borrowed mesh source");
        rejects([&] { encodeFbxMeshScene(source, { 256, 65536 }); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        rejects([&] { encodeFbxMeshScene(source, { 0, 1 }); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        Object empty;
        rejects([&] { encodeFbxMeshScene(empty, { sizeof(OwnedFbxDocument), 65536 }); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        rejects([&] { encodeFbxMeshScene(empty); }, DUST3D_STATUS_INVALID_DOCUMENT);
        auto bad = fixture();
        bad.triangles[0][1] = 4;
        rejects([&] { encodeFbxMeshScene(bad); }, DUST3D_STATUS_INVALID_DOCUMENT);
        bad = fixture();
        bad.triangles[0].pop_back();
        rejects([&] { encodeFbxMeshScene(bad); }, DUST3D_STATUS_INVALID_DOCUMENT);
        bad = fixture();
        bad.vertices[0][0] = std::numeric_limits<double>::infinity();
        rejects([&] { encodeFbxMeshScene(bad); }, DUST3D_STATUS_INVALID_DOCUMENT);
        bad = fixture();
        bad.vertices[0][0] = 4000001;
        rejects([&] { encodeFbxMeshScene(bad); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        bad = fixture();
        bad.vertices[0][0] = -4000000;
        require(!encodeFbxMeshScene(bad).bytes().empty(), "Inclusive coordinate boundary rejected");
        bad = fixture();
        bad.setTriangleVertexNormals({ { { 0, 0, 1 } }, { { 0, 1, 0 }, { 0, 1, 0 }, { 0, 1, 0 } } });
        rejects([&] { encodeFbxMeshScene(bad); }, DUST3D_STATUS_INVALID_DOCUMENT);
        bad = fixture();
        bad.setTriangleVertexUvs({});
        rejects([&] { encodeFbxMeshScene(bad); }, DUST3D_STATUS_INVALID_DOCUMENT);
        bad = fixture();
        bad.setTriangleVertexNormals({});
        rejects([&] { encodeFbxMeshScene(bad); }, DUST3D_STATUS_INVALID_DOCUMENT);
        bad = fixture();
        bad.setTriangleVertexUvs({ { { 0, 0 } }, { { 0, 0 }, { 1, 0 }, { 0, 1 } } });
        rejects([&] { encodeFbxMeshScene(bad); }, DUST3D_STATUS_INVALID_DOCUMENT);
        bad = fixture();
        auto normals = *bad.triangleVertexNormals();
        normals[0][0][0] = std::numeric_limits<double>::quiet_NaN();
        bad.setTriangleVertexNormals(normals);
        rejects([&] { encodeFbxMeshScene(bad); }, DUST3D_STATUS_INVALID_DOCUMENT);
        bad = fixture();
        auto uvs = *bad.triangleVertexUvs();
        uvs[0][0][1] = std::numeric_limits<double>::infinity();
        bad.setTriangleVertexUvs(uvs);
        rejects([&] { encodeFbxMeshScene(bad); }, DUST3D_STATUS_INVALID_DOCUMENT);
        bad = fixture();
        normals = *bad.triangleVertexNormals();
        normals[0][0][0] = 1000001;
        bad.setTriangleVertexNormals(normals);
        rejects([&] { encodeFbxMeshScene(bad); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        bad = fixture();
        uvs = *bad.triangleVertexUvs();
        uvs[0][0][1] = -1000001;
        bad.setTriangleVertexUvs(uvs);
        rejects([&] { encodeFbxMeshScene(bad); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        Object plain;
        plain.vertices = source.vertices;
        plain.triangles = source.triangles;
        require(!encodeFbxMeshScene(plain).bytes().empty(), "Absent optional corner streams rejected");
        if (argc == 2) {
            std::ofstream output(argv[1], std::ios::binary | std::ios::trunc);
            output.write(reinterpret_cast<const char*>(first.bytes().data()), static_cast<std::streamsize>(first.bytes().size()));
            output.close();
            require(bool(output), "Could not write explicit consumer fixture");
        }
        std::cout << "FBX mesh scene contracts passed\n";
        return 0;
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}
