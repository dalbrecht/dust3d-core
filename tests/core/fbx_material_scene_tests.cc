// SPDX-License-Identifier: MIT
#include "EngineInternal.h"
#include "FbxMaterialImagePlan.h"
#include "FbxMeshScene.h"
#include <algorithm>
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
        require(cause.status() == status, "Material rejection lost typed cause");
        return;
    }
    throw std::runtime_error("Invalid material admitted");
}
std::array<std::vector<uint8_t>, 6> fixtureMaps()
{
    // Authored2x2 RGBA fixtures; distinct first pixels detect channel swaps.
    return {
        std::vector<uint8_t> { 137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68, 82, 0, 0, 0, 2, 0, 0, 0, 2, 8, 6, 0, 0, 0, 114, 182, 13, 36, 0, 0, 0, 26, 73, 68, 65, 84, 120, 156, 99, 16, 209, 176, 249, 127, 162, 34, 160, 129, 193, 45, 42, 239, 255, 180, 85, 251, 28, 0, 63, 219, 7, 211, 182, 27, 118, 105, 0, 0, 0, 0, 73, 69, 78, 68, 174, 66, 96, 130 },
        std::vector<uint8_t> { 137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68, 82, 0, 0, 0, 2, 0, 0, 0, 2, 8, 6, 0, 0, 0, 114, 182, 13, 36, 0, 0, 0, 26, 73, 68, 65, 84, 120, 156, 99, 48, 178, 113, 251, 191, 165, 34, 160, 129, 193, 45, 42, 239, 255, 180, 85, 251, 28, 0, 66, 171, 7, 251, 146, 55, 197, 2, 0, 0, 0, 0, 73, 69, 78, 68, 174, 66, 96, 130 },
        std::vector<uint8_t> { 137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68, 82, 0, 0, 0, 2, 0, 0, 0, 2, 8, 6, 0, 0, 0, 114, 182, 13, 36, 0, 0, 0, 26, 73, 68, 65, 84, 120, 156, 99, 8, 8, 8, 248, 191, 160, 34, 160, 129, 193, 45, 42, 239, 255, 180, 85, 251, 28, 0, 69, 123, 8, 35, 174, 247, 237, 157, 0, 0, 0, 0, 73, 69, 78, 68, 174, 66, 96, 130 },
        std::vector<uint8_t> { 137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68, 82, 0, 0, 0, 2, 0, 0, 0, 2, 8, 6, 0, 0, 0, 114, 182, 13, 36, 0, 0, 0, 26, 73, 68, 65, 84, 120, 156, 99, 200, 75, 137, 250, 223, 83, 17, 208, 192, 224, 22, 149, 247, 127, 218, 170, 125, 14, 0, 72, 75, 8, 75, 210, 244, 35, 116, 0, 0, 0, 0, 73, 69, 78, 68, 174, 66, 96, 130 },
        std::vector<uint8_t> { 137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68, 82, 0, 0, 0, 2, 0, 0, 0, 2, 8, 6, 0, 0, 0, 114, 182, 13, 36, 0, 0, 0, 26, 73, 68, 65, 84, 120, 156, 99, 232, 169, 72, 249, 95, 81, 17, 208, 192, 224, 22, 149, 247, 127, 218, 170, 125, 14, 0, 75, 27, 8, 115, 204, 146, 130, 178, 0, 0, 0, 0, 73, 69, 78, 68, 174, 66, 96, 130 },
        std::vector<uint8_t> { 137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68, 82, 0, 0, 0, 2, 0, 0, 0, 2, 8, 6, 0, 0, 0, 114, 182, 13, 36, 0, 0, 0, 26, 73, 68, 65, 84, 120, 156, 99, 88, 213, 147, 247, 63, 165, 34, 160, 129, 193, 45, 42, 239, 255, 180, 85, 251, 28, 0, 77, 235, 8, 155, 172, 3, 237, 133, 0, 0, 0, 0, 73, 69, 78, 68, 174, 66, 96, 130 }
    };
}
Object mesh()
{
    Object source;
    source.vertices = { { 0, 0, 0 }, { 2, 0, 0 }, { 0, 3, 0 } };
    source.triangles = { { 0, 1, 2 } };
    source.setTriangleVertexNormals({ { { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 } } });
    source.setTriangleVertexUvs({ { { 0, 0 }, { 1, 0 }, { 0, 1 } } });
    return source;
}
}
int main(int argc, char** argv)
{
    try {
        const auto emptyPlan = planFbxMaterialImages({}, false);
        require(emptyPlan.imageCount == 0 && emptyPlan.encodedImageBytes == 0, "Empty material image admission changed");
        const auto sixPlan = planFbxMaterialImages({ 83, 83, 83, 83, 83, 83 }, true);
        require(sixPlan.imageCount == 6 && sixPlan.encodedImageBytes == 498, "Six-channel image admission changed");
        const auto exactPlan = planFbxMaterialImages({ 64 * 1024 * 1024, 0, 0, 0, 0, 0 }, true);
        require(exactPlan.imageCount == 1 && exactPlan.encodedImageBytes == 64 * 1024 * 1024, "Material image byte ceiling changed");
        rejects([] { planFbxMaterialImages({ 64 * 1024 * 1024, 8, 0, 0, 0, 0 }, true); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        rejects([] { planFbxMaterialImages({ std::numeric_limits<size_t>::max(), 0, 0, 0, 0, 0 }, true); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        rejects([] { planFbxMaterialImages({ 3, 0, 0, 0, 0, 0 }, true); }, DUST3D_STATUS_INVALID_DOCUMENT);
        rejects([] { planFbxMaterialImages({ 8, 0, 0, 0, 0, 0 }, false); }, DUST3D_STATUS_INVALID_DOCUMENT);
        auto source = mesh();
        FbxSceneMaterial material;
        material.baseColor = { 0.25, 0.5, 0.75, 0.625 };
        material.metallic = 0.375;
        material.roughness = 0.875;
        const auto baseline = encodeFbxTexturedMeshScene(source, material);
        require(baseline.bytes() == encodeFbxTexturedMeshScene(source, material).bytes(), "Material scene nondeterministic");
        auto bad = material;
        bad.baseColor[0] = -0.1;
        rejects([&] { encodeFbxTexturedMeshScene(source, bad); }, DUST3D_STATUS_INVALID_DOCUMENT);
        bad = material;
        bad.baseColor[3] = 1.1;
        rejects([&] { encodeFbxTexturedMeshScene(source, bad); }, DUST3D_STATUS_INVALID_DOCUMENT);
        bad = material;
        bad.metallic = std::numeric_limits<double>::quiet_NaN();
        rejects([&] { encodeFbxTexturedMeshScene(source, bad); }, DUST3D_STATUS_INVALID_DOCUMENT);
        bad = material;
        bad.metallic = 1.1;
        rejects([&] { encodeFbxTexturedMeshScene(source, bad); }, DUST3D_STATUS_INVALID_DOCUMENT);
        bad = material;
        bad.roughness = std::numeric_limits<double>::infinity();
        rejects([&] { encodeFbxTexturedMeshScene(source, bad); }, DUST3D_STATUS_INVALID_DOCUMENT);
        bad = material;
        bad.roughness = -1;
        rejects([&] { encodeFbxTexturedMeshScene(source, bad); }, DUST3D_STATUS_INVALID_DOCUMENT);
        bad = material;
        bad.pngMaps[0] = { 1, 2, 3 };
        rejects([&] { encodeFbxTexturedMeshScene(source, bad); }, DUST3D_STATUS_INVALID_DOCUMENT);
        rejects([&] { encodeFbxTexturedMeshScene(source, material, { 128, 65536 }); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        rejects([&] { encodeFbxTexturedMeshScene(source, material, { 0, 0 }); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        require(material.baseColor[3] == 0.625 && source.vertices.size() == 3, "Material export mutated input");
        material.pngMaps = fixtureMaps();
        Object noUvs;
        noUvs.vertices = source.vertices;
        noUvs.triangles = source.triangles;
        rejects([&] { encodeFbxTexturedMeshScene(noUvs, material); }, DUST3D_STATUS_INVALID_DOCUMENT);
        const auto output = encodeFbxTexturedMeshScene(source, material);
        for (const auto& png : material.pngMaps)
            require(std::search(output.bytes().begin(), output.bytes().end(), png.begin(), png.end()) != output.bytes().end(), "Embedded PNG bytes changed or channel was lost");
        const auto moved = [&] { auto ownedMaterial = material; return encodeFbxTexturedMeshScene(source, ownedMaterial); }();
        require(moved.bytes() == output.bytes(), "Material output borrowed its input owner");
        bad = material;
        bad.pngMaps[1][0] = 0;
        rejects([&] { encodeFbxTexturedMeshScene(source, bad); }, DUST3D_STATUS_INVALID_DOCUMENT);
        FbxPropertyBlock expectedName;
        expectedName.appendString(std::string("Dust3D_color\0\1Texture", 21));
        auto occurrence = output.bytes().begin();
        size_t typedNameCount = 0;
        while ((occurrence = std::search(occurrence, output.bytes().end(), expectedName.bytes().begin(), expectedName.bytes().end())) != output.bytes().end()) {
            ++typedNameCount;
            ++occurrence;
        }
        require(typedNameCount == 2, "Typed texture object/TextureName binary name was truncated");
        require(baseline.bytes() != output.bytes(), "Embedded images absent from scene output");
        for (size_t channel = 0; channel < 6; ++channel) {
            auto oneMap = FbxSceneMaterial();
            oneMap.pngMaps[channel] = material.pngMaps[channel];
            require(!encodeFbxTexturedMeshScene(source, oneMap).bytes().empty(), "Optional individual map channel rejected");
        }
        if (argc == 2) {
            std::ofstream file(argv[1], std::ios::binary | std::ios::trunc);
            file.write(reinterpret_cast<const char*>(output.bytes().data()), static_cast<std::streamsize>(output.bytes().size()));
            file.close();
            require(bool(file), "Could not write explicit material consumer fixture");
        }
        std::cout << "FBX material scene contracts passed\n";
        return 0;
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}
