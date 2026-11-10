// SPDX-License-Identifier: MIT
#include <Dust3DBridge.h>
#include <algorithm>
#include <cstring>
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
}
int main(int argc, char** argv)
{
    try {
        require(argc == 2, "Ordinary licensed fixture directory required");
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
        const size_t firstImage = 0;
        const auto exportPrepared = [&](const size_t* indices, size_t count) {
            return Owner(dust3d_fbx_export_load_with_premultiplied_images(
                             bytes.data(), bytes.size(), &settings, indices, count, &error),
                dust3d_fbx_export_destroy);
        };
        require(!exportPrepared(&firstImage, 1) && error.status == DUST3D_STATUS_INVALID_DOCUMENT,
            "Prepared image flags must be rejected in geometry mode");
        settings.mode = DUST3D_FBX_EXPORT_BAKED;
        Dust3DBakeImage prepared {};
        std::strcpy(prepared.image_id, "{f0000000-0000-4000-8000-000000000002}");
        prepared.width = prepared.height = 1;
        prepared.row_stride = prepared.byte_count = 4;
        uint8_t channels[] { 128, 0, 0, 127 };
        prepared.pixels = channels;
        settings.bake.images = &prepared;
        settings.bake.image_count = 1;
        const size_t pastEnd = 1;
        require(!exportPrepared(nullptr, 1) && error.status == DUST3D_STATUS_INVALID_DOCUMENT,
            "Prepared image index buffer must be present");
        require(!exportPrepared(&pastEnd, 1) && error.status == DUST3D_STATUS_INVALID_DOCUMENT,
            "Prepared image index must be in range");
        require(!exportPrepared(&firstImage, 1) && error.status == DUST3D_STATUS_INVALID_DOCUMENT,
            "Prepared RGB must not exceed alpha");
        channels[0] = 126;
        require(exportPrepared(&firstImage, 1) && error.status == DUST3D_STATUS_OK,
            "Prepared image export must accept valid associated pixels");
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
        std::cout << "Owned static FBX C API contracts passed\n";
        return 0;
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}
