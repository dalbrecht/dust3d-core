// SPDX-License-Identifier: MIT
#include "BakedMesh.h"
#include "FbxBakedMaterial.h"
#include "FbxGeometrySelection.h"
#include "FbxMeshScene.h"
#include "GenerationPipeline.h"
#include "UvMetadata.h"
#include <cmath>
#include <fstream>
#include <iomanip>
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
        require(argc == 2 || argc == 3 || (argc == 4 && std::string(argv[3]) == "--default-material"),
            "Licensed ordinary fixture directory required; optional output directory and --default-material");
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
        outputs.useFbxAtlasUvs = true;
        previous = geometry.get();
        GenerationOutputs missingGeometry;
        missingGeometry.useFbxAtlasUvs = true;
        try {
            generateMesh(snapshot(bytes), "", nullptr, nullptr, nullptr, missingGeometry);
            throw std::runtime_error("Atlas export without geometry owner accepted");
        } catch (const DocumentError& cause) {
            require(cause.status() == DUST3D_STATUS_INVALID_DOCUMENT, "Missing geometry output lost typed cause");
        }
        try {
            generateMesh(snapshot(bytes), "", nullptr, nullptr, nullptr, outputs);
            throw std::runtime_error("Atlas export without UV generation accepted");
        } catch (const DocumentError& cause) {
            require(cause.status() == DUST3D_STATUS_INVALID_DOCUMENT && geometry.get() == previous, "Missing UV metadata lost original owner/cause");
        }
        auto original = snapshot(bytes);
        if (argc != 4) {
            for (auto& [identity, attributes] : original->parts) {
                (void)identity;
                attributes["metallic"] = "0.375";
                attributes["roughness"] = "0.625";
                attributes["emissive"] = "0.5";
            }
        }
        UvMetadata metadata(*original, 64 * 1024 * 1024);
        Dust3DBakeOptions settings {};
        settings.struct_size = sizeof(settings);
        settings.atlas_texture_size = settings.color_size = settings.normal_size = settings.material_size = 64;
        BakedMeshData bake(settings);
        bake.validateVisuals(*original);
        outputs.uvs = &metadata;
        outputs.baked = &bake;
        outputs.textureSize = bake.textureSize();
        auto texturedPreview = generateMesh(std::move(original), "", nullptr, nullptr, nullptr, outputs);
        require(texturedPreview->vertices.size() == 240 && geometry->object().triangleVertexUvs(), "Atlas export not generated");
        require(bake.maps[0] && bake.maps[0]->width == 64 && bake.maps[0]->height == 64, "Aligned procedural color bake missing");
        if (argc != 4)
            require(bake.maps[2] && bake.maps[3] && bake.maps[4], "Controlled generated metallic/roughness/emissive maps missing");
        const auto& packed = *geometry->object().triangleVertexUvs();
        const auto& atlas = metadata.atlas();
        require(packed.size() == atlas.triangleUvs.size(), "Atlas source face count lost");
        for (size_t triangle = 0; triangle < packed.size(); ++triangle) {
            for (size_t corner = 0; corner < 3; ++corner) {
                require(packed[triangle][corner].x() == atlas.triangleUvs[triangle][corner].x()
                        && packed[triangle][corner].y() == atlas.triangleUvs[triangle][corner].y(),
                    "Export used local or float UVs instead of original packed atlas doubles");
            }
        }
        FbxBakedImageViews images {};
        for (size_t channel = 0; channel < bake.maps.size(); ++channel)
            images[channel] = bake.maps[channel] ? &*bake.maps[channel] : nullptr;
        const auto material = makeFbxBakedMaterial(images);
        const auto textured = encodeFbxTexturedMeshScene(geometry->object(), material);
        require(textured.bytes().size() > material.pngMaps[0].size(), "Generated baked scene failed to embed material/geometry");
        if (argc >= 3) {
            const std::string directory(argv[2]);
            std::ofstream exported(directory + "/baked.fbx", std::ios::binary | std::ios::trunc);
            exported.exceptions(std::ios::badbit | std::ios::failbit);
            exported.write(reinterpret_cast<const char*>(textured.bytes().data()), static_cast<std::streamsize>(textured.bytes().size()));
            exported.close();
            std::ofstream expected(directory + "/expected.json", std::ios::trunc);
            expected.exceptions(std::ios::badbit | std::ios::failbit);
            expected << std::setprecision(17) << "{\"corners\":[";
            const auto& source = geometry->object();
            const auto* normals = source.triangleVertexNormals();
            require(normals, "Controlled Capsule must retain corner normals");
            for (size_t triangle = 0; triangle < source.triangles.size(); ++triangle) {
                for (size_t corner = 0; corner < 3; ++corner) {
                    if (triangle || corner)
                        expected << ',';
                    const auto& position = source.vertices[source.triangles[triangle][corner]];
                    const auto& normal = (*normals)[triangle][corner];
                    expected << '[' << position.x() << ',' << position.y() << ',' << position.z() << ','
                             << normal.x() << ',' << normal.y() << ',' << normal.z() << ','
                             << packed[triangle][corner].x() << ',' << 1 - packed[triangle][corner].y() << ']';
                }
            }
            expected << "],\"images\":[";
            constexpr std::array<const char*, 6> names { "color", "normal", "metallic", "roughness", "emissive", "ao" };
            bool first = true;
            for (size_t channel = 0; channel < images.size(); ++channel) {
                if (!images[channel])
                    continue;
                if (!first)
                    expected << ',';
                first = false;
                const auto& image = *images[channel];
                expected << "{\"channel\":\"" << names[channel] << "\",\"width\":" << image.width << ",\"height\":" << image.height << ",\"pixels\":[";
                for (size_t index = 0; index < image.pixels.size(); ++index) {
                    if (index)
                        expected << ',';
                    expected << static_cast<unsigned>(image.pixels[index]);
                }
                expected << "]}";
            }
            expected << "]}";
            expected.close();
        }
        std::cout << "Original FBX generation geometry contracts passed\n";
        return 0;
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}
