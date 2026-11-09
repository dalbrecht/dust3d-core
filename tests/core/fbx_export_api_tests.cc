// SPDX-License-Identifier: MIT
#include <Dust3DBridge.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <dust3d/base/ds3_file.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <vector>
namespace {
void require(bool condition, const char* cause)
{
    if (!condition)
        throw std::runtime_error(cause);
}
using Owner = std::unique_ptr<Dust3DFbxExport, decltype(&dust3d_fbx_export_destroy)>;
Dust3DFbxExportOptions options()
{
    Dust3DFbxExportOptions result {};
    result.struct_size = sizeof(result);
    result.bake.struct_size = sizeof(result.bake);
    result.bake.atlas_texture_size = result.bake.color_size = result.bake.normal_size = result.bake.material_size = 64;
    return result;
}
void texturedExportOmitsPreviewWarnings(const std::vector<uint8_t>& bytes)
{
    std::vector<uint8_t> model;
    dust3d::Ds3FileReader(bytes.data(), bytes.size()).loadItem("model.xml", &model);
    const std::string original(model.begin(), model.end());
    const auto component = original.find("<component ");
    require(component != std::string::npos, "Fixture must contain a component");
    const char imageId[] = "{eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee}";
    const uint8_t pixel[] { 255, 0, 0, 255 };
    Dust3DBakeImage image {};
    std::memcpy(image.image_id, imageId, sizeof(imageId));
    image.width = image.height = 1;
    image.row_stride = image.byte_count = sizeof(pixel);
    image.pixels = pixel;
    for (const bool assigned : { false, true }) {
        auto xml = original;
        if (assigned)
            xml.insert(component + std::strlen("<component "), std::string("colorImageId=\"") + imageId + "\" ");
        dust3d::Ds3FileWriter writer;
        require(writer.add("model.xml", "model", xml.data(), xml.size()), "Fixture model write failed");
        require(writer.add(std::string("images/") + imageId + ".png", "asset", pixel, sizeof(pixel)), "Fixture image write failed");
        std::vector<uint8_t> archive;
        writer.save(archive);
        auto settings = options();
        Dust3DError error {};
        Owner geometry(dust3d_fbx_export_load(archive.data(), archive.size(), &settings, &error), dust3d_fbx_export_destroy);
        require(geometry && std::strstr(dust3d_fbx_export_warning(geometry.get()), "Untextured preview:") != nullptr,
            "Geometry-only export must retain the existing material warning");
        settings.mode = DUST3D_FBX_EXPORT_BAKED;
        settings.bake.images = &image;
        settings.bake.image_count = 1;
        Owner textured(dust3d_fbx_export_load(archive.data(), archive.size(), &settings, &error), dust3d_fbx_export_destroy);
        require(bool(textured), "Textured export fixture must bake successfully");
        require(std::strcmp(dust3d_fbx_export_warning(textured.get()), "") == 0,
            "Successful textured FBX must omit the untextured preview warning");
        if (assigned) {
            settings.bake.images = nullptr;
            settings.bake.image_count = 0;
            Owner missing(dust3d_fbx_export_load(archive.data(), archive.size(), &settings, &error), dust3d_fbx_export_destroy);
            require(!missing && error.status != DUST3D_STATUS_OK && *error.message,
                "Missing decoded image pixels must still fail with the bake cause");
        }
    }
}

void mirroredTexturedCapsuleExportsCompleteAtlas(const std::string& fixtureDirectory)
{
    std::ifstream file(fixtureDirectory + "/MirroredTexturedCapsule.ds3", std::ios::binary);
    require(bool(file), "Mirrored textured Capsule fixture missing");
    const std::vector<uint8_t> bytes { std::istreambuf_iterator<char>(file), {} };
    constexpr size_t side = 32;
    constexpr char imageId[] = "{12345678-1234-4234-8234-123456789abc}";
    Dust3DUvImage uvImage {};
    std::memcpy(uvImage.image_id, imageId, sizeof(imageId));
    uvImage.width = uvImage.height = side;
    Dust3DUvOptions uvOptions {};
    uvOptions.struct_size = sizeof(uvOptions);
    uvOptions.images = &uvImage;
    uvOptions.image_count = 1;
    Dust3DError error {};
    const std::unique_ptr<Dust3DUvMesh, decltype(&dust3d_uv_mesh_destroy)> uv(
        dust3d_uv_mesh_load(bytes.data(), bytes.size(), &uvOptions, &error), dust3d_uv_mesh_destroy);
    require(uv && error.status == DUST3D_STATUS_OK, "Mirrored Capsule UV generation failed");
    const auto* triangles = dust3d_uv_mesh_triangles(uv.get());
    const auto count = dust3d_uv_mesh_triangle_count(uv.get());
    require(triangles && count > 0, "Mirrored Capsule has no UV triangles");
    for (size_t i = 0; i < count; ++i) {
        const auto& triangle = triangles[i];
        require(triangle.known == 1, "Mirrored Capsule lost a packed UV triangle");
        const auto& a = triangle.uv[0];
        const auto& b = triangle.uv[1];
        const auto& c = triangle.uv[2];
        const auto area = (b[0] - a[0]) * (c[1] - a[1]) - (c[0] - a[0]) * (b[1] - a[1]);
        require(std::isfinite(area) && std::abs(area) > 1e-14, "Mirrored Capsule has a collapsed UV triangle");
    }

    constexpr std::array<std::array<uint8_t, 4>, 4> quadrants { { { 240, 20, 40, 255 }, { 25, 210, 50, 255 }, { 30, 45, 230, 255 }, { 240, 210, 15, 255 } } };
    std::array<uint8_t, side * side * 4> pixels {};
    for (size_t y = 0; y < side; ++y) {
        for (size_t x = 0; x < side; ++x) {
            const auto& color = quadrants[(y >= side / 2 ? 2 : 0) + (x >= side / 2 ? 1 : 0)];
            std::copy(color.begin(), color.end(), pixels.data() + (y * side + x) * 4);
        }
    }
    Dust3DBakeImage image {};
    std::memcpy(image.image_id, imageId, sizeof(imageId));
    image.width = image.height = side;
    image.row_stride = side * 4;
    image.byte_count = pixels.size();
    image.pixels = pixels.data();
    auto settings = options();
    settings.mode = DUST3D_FBX_EXPORT_BAKED;
    settings.bake.images = &image;
    settings.bake.image_count = 1;
    const Owner baked(dust3d_fbx_export_load(bytes.data(), bytes.size(), &settings, &error), dust3d_fbx_export_destroy);
    require(baked && error.status == DUST3D_STATUS_OK, "Mirrored Capsule textured FBX export failed");
    const auto* buffer = dust3d_fbx_export_bytes(baked.get());
    const auto size = dust3d_fbx_export_byte_count(baked.get());
    require(buffer && size > 100 && std::memcmp(buffer, "Kaydara FBX Binary", 18) == 0,
        "Mirrored Capsule produced an invalid FBX container");
}
}
int main(int argc, char** argv)
{
    try {
        require(argc == 2, "Ordinary licensed fixture directory required");
        mirroredTexturedCapsuleExportsCompleteAtlas(argv[1]);
        std::ifstream file(std::string(argv[1]) + "/Capsule.ds3", std::ios::binary);
        require(bool(file), "Capsule fixture missing");
        const std::vector<uint8_t> bytes { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
        Dust3DError error {};
        require(!dust3d_fbx_export_bytes(nullptr) && dust3d_fbx_export_byte_count(nullptr) == 0
                && dust3d_fbx_export_payload_bytes(nullptr) == 0 && std::strcmp(dust3d_fbx_export_warning(nullptr), "") == 0,
            "Null FBX owner accessors are not safe");
        dust3d_fbx_export_destroy(nullptr);
        require(!dust3d_fbx_export_load(bytes.data(), bytes.size(), nullptr, &error)
                && error.status == DUST3D_STATUS_INVALID_DOCUMENT && *error.message,
            "Explicit export options required");
        auto settings = options();
        auto rejected = [&](Dust3DStatus status) {
            Owner owner(dust3d_fbx_export_load(bytes.data(), bytes.size(), &settings, &error), dust3d_fbx_export_destroy);
            require(!owner && error.status == status && *error.message, "Export rejection lost typed cause");
        };
        settings.struct_size--;
        rejected(DUST3D_STATUS_INVALID_DOCUMENT);
        settings = options();
        settings.mode = 2;
        rejected(DUST3D_STATUS_INVALID_DOCUMENT);
        settings = options();
        settings.maximum_geometry_payload_bytes = 1;
        rejected(DUST3D_STATUS_LIMIT_EXCEEDED);
        settings = options();
        settings.maximum_output_payload_bytes = 1;
        rejected(DUST3D_STATUS_LIMIT_EXCEEDED);
        settings.maximum_output_payload_bytes = 64;
        rejected(DUST3D_STATUS_LIMIT_EXCEEDED);
        settings = options();
        settings.maximum_png_payload_bytes = 67108865;
        rejected(DUST3D_STATUS_LIMIT_EXCEEDED);
        settings = options();
        settings.mode = DUST3D_FBX_EXPORT_BAKED;
        settings.bake.struct_size--;
        rejected(DUST3D_STATUS_INVALID_DOCUMENT);
        settings = options();
        settings.mode = DUST3D_FBX_EXPORT_BAKED;
        settings.bake.image_count = 1;
        rejected(DUST3D_STATUS_INVALID_DOCUMENT);
        settings = options();
        Owner geometry(dust3d_fbx_export_load(bytes.data(), bytes.size(), &settings, &error), dust3d_fbx_export_destroy);
        require(geometry && error.status == DUST3D_STATUS_OK, "Ordinary geometry FBX export failed");
        const auto count = dust3d_fbx_export_byte_count(geometry.get());
        const auto* buffer = dust3d_fbx_export_bytes(geometry.get());
        require(buffer && count > 100 && std::memcmp(buffer, "Kaydara FBX Binary", 18) == 0, "Owned FBX container invalid");
        require(dust3d_fbx_export_warning(geometry.get()) && std::strcmp(dust3d_fbx_export_warning(geometry.get()), "") == 0,
            "Ordinary Capsule gained an unexpected parser warning");
        require(dust3d_fbx_export_payload_bytes(geometry.get()) > count, "Retained output root/capacity not accounted");
        settings.maximum_output_payload_bytes = dust3d_fbx_export_payload_bytes(geometry.get());
        Owner exact(dust3d_fbx_export_load(bytes.data(), bytes.size(), &settings, &error), dust3d_fbx_export_destroy);
        require(exact && dust3d_fbx_export_payload_bytes(exact.get()) <= settings.maximum_output_payload_bytes,
            "Exact retained output quota rejected or undercounted");
        --settings.maximum_output_payload_bytes;
        rejected(DUST3D_STATUS_LIMIT_EXCEEDED);
        settings = options();
        const std::vector<uint8_t> copied(buffer, buffer + count);
        settings.mode = DUST3D_FBX_EXPORT_BAKED;
        Owner baked(dust3d_fbx_export_load(bytes.data(), bytes.size(), &settings, &error), dust3d_fbx_export_destroy);
        require(baked && error.status == DUST3D_STATUS_OK && dust3d_fbx_export_byte_count(baked.get()) > count, "Baked atlas FBX export failed");
        require(std::equal(copied.begin(), copied.end(), dust3d_fbx_export_bytes(geometry.get())), "Second export invalidated earlier owner");
        geometry.reset();
        require(copied.size() == count && dust3d_fbx_export_byte_count(baked.get()) > count, "Independent copied/second owner lifetimes lost");
        std::ifstream crossingFile(std::string(argv[1]) + "/RoundedCrossing.ds3", std::ios::binary);
        require(bool(crossingFile), "RoundedCrossing fixture missing");
        const std::vector<uint8_t> crossingBytes { std::istreambuf_iterator<char>(crossingFile), std::istreambuf_iterator<char>() };
        Owner crossing(dust3d_fbx_export_load(crossingBytes.data(), crossingBytes.size(), &settings, &error), dust3d_fbx_export_destroy);
        require(crossing && error.status == DUST3D_STATUS_OK && dust3d_fbx_export_byte_count(crossing.get()) > 100,
            "Baked FBX must retain packed UV coverage for RoundedCrossing Boolean faces");
        settings = options();
        const uint8_t invalid[] { 1, 2, 3 };
        Owner malformed(dust3d_fbx_export_load(invalid, sizeof(invalid), &settings, &error), dust3d_fbx_export_destroy);
        require(!malformed && error.status == DUST3D_STATUS_INVALID_DOCUMENT && *error.message, "Original archive failure lost");
        std::unique_ptr<Dust3DCancellationToken, decltype(&dust3d_cancellation_token_destroy)> token(
            dust3d_cancellation_token_create(&error), dust3d_cancellation_token_destroy);
        require(bool(token), "Cancellation token missing");
        settings.cancellation = token.get();
        settings.mode = DUST3D_FBX_EXPORT_BAKED;
        settings.bake.cancellation = token.get();
        settings.cancellation = nullptr;
        rejected(DUST3D_STATUS_INVALID_DOCUMENT);
        settings.cancellation = token.get();
        Owner cooperative(dust3d_fbx_export_load(bytes.data(), bytes.size(), &settings, &error), dust3d_fbx_export_destroy);
        require(cooperative && error.status == DUST3D_STATUS_OK, "Matching uncancelled export/bake token rejected");
        settings = options();
        settings.cancellation = token.get();
        dust3d_cancellation_token_cancel(token.get());
        rejected(DUST3D_STATUS_CANCELLED);
        require(!dust3d_fbx_export_load(bytes.data(), bytes.size(), nullptr, nullptr), "Null diagnostic boundary threw or accepted options");
        texturedExportOmitsPreviewWarnings(bytes);
        std::cout << "Owned static FBX C API contracts passed\n";
        return 0;
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}
