// SPDX-License-Identifier: MIT
#include <chrono>
#include <cstring>
#include <fstream>
#include <future>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <native/Engine/Cancellation.h>
#include <native/Engine/GenerationPipeline.h>
#include <stdexcept>

namespace {
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
void nullBakedOwners()
{
    require(!dust3d_baked_mesh_uvs(nullptr) && !dust3d_baked_mesh_tangents(nullptr), "Null baked owners must expose no borrowed storage");
    require(dust3d_baked_mesh_tangent_count(nullptr) == 0 && dust3d_baked_mesh_payload_bytes(nullptr) == 0, "Null baked owner counts must be zero");
    const auto view = dust3d_baked_mesh_texture(nullptr, DUST3D_TEXTURE_COLOR);
    require(!view.pixels && view.width == 0 && view.height == 0 && view.row_stride == 0 && view.byte_count == 0, "Null baked texture views must be fully zero initialized");
    dust3d_baked_mesh_destroy(nullptr);
    Dust3DError error {};
    require(!dust3d_baked_mesh_load(nullptr, 0, nullptr, &error) && error.status == DUST3D_STATUS_INVALID_DOCUMENT && *error.message, "Missing bake options must fail with a typed cause and no owner");
}
void invalidBakeInputs()
{
    Dust3DError error {};
    Dust3DBakeOptions options {};
    auto rejected = [&](Dust3DStatus status, const char* expected) {
        require(!dust3d_baked_mesh_load(nullptr, 0, &options, &error) && error.status == status && std::strstr(error.message, expected), "Invalid bake input must preserve its specific boundary cause");
    };
    rejected(DUST3D_STATUS_INVALID_DOCUMENT, "sized options");
    options.struct_size = sizeof(options);
    options.maximum_bake_payload_bytes = size_t(268435457);
    rejected(DUST3D_STATUS_LIMIT_EXCEEDED, "budgets");
    options.maximum_bake_payload_bytes = 0;
    options.color_size = 8193;
    rejected(DUST3D_STATUS_INVALID_DOCUMENT, "dimensions");
    options.color_size = 0;
    options.image_count = 1;
    rejected(DUST3D_STATUS_INVALID_DOCUMENT, "descriptor buffer");
    Dust3DBakeImage image {};
    options.images = &image;
    std::memset(image.image_id, 'x', sizeof(image.image_id));
    rejected(DUST3D_STATUS_INVALID_DOCUMENT, "terminated");
    std::strcpy(image.image_id, "{eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee}");
    uint8_t pixel[4] {};
    image.pixels = pixel;
    image.width = image.height = 1;
    image.row_stride = image.byte_count = 4;
    image.row_stride = 3;
    rejected(DUST3D_STATUS_INVALID_DOCUMENT, "stride/span");
    image.row_stride = std::numeric_limits<size_t>::max();
    image.height = 2;
    rejected(DUST3D_STATUS_INVALID_DOCUMENT, "stride/span");
    options.images = nullptr;
    options.image_count = 0;
    auto* token = dust3d_cancellation_token_create(&error);
    require(token != nullptr, "Cancelled load contract requires an owning token");
    dust3d_cancellation_token_cancel(token);
    options.cancellation = token;
    rejected(DUST3D_STATUS_CANCELLED, "cancelled");
    dust3d_cancellation_token_destroy(token);
}
void ownedCapsule(const std::string& directory)
{
    std::ifstream file(directory + "/Capsule.ds3", std::ios::binary);
    require(file.good(), "Baked owner fixture must exist");
    const std::vector<uint8_t> bytes { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
    Dust3DBakeOptions options {};
    options.struct_size = sizeof(options);
    options.color_size = options.normal_size = options.material_size = 8;
    Dust3DError error {};
    using Owner = std::unique_ptr<Dust3DBakedMesh, decltype(&dust3d_baked_mesh_destroy)>;
    Owner owner(dust3d_baked_mesh_load(bytes.data(), bytes.size(), &options, &error), dust3d_baked_mesh_destroy);
    if (!owner)
        throw std::runtime_error(error.message);
    const auto* uvs = dust3d_baked_mesh_uvs(owner.get());
    const auto color = dust3d_baked_mesh_texture(owner.get(), DUST3D_TEXTURE_COLOR);
    require(uvs && dust3d_uv_mesh_triangle_count(uvs) == 80 && dust3d_baked_mesh_tangent_count(owner.get()) == 80, "Real capsule geometry, UVs and tangents must align");
    require(color.width == 8 && color.height == 8 && color.row_stride == 32 && color.byte_count == 256 && color.pixels, "Baked capsule must expose actual bounded RGBA8 pixels");
    require(!dust3d_baked_mesh_texture(owner.get(), DUST3D_TEXTURE_NORMAL).pixels, "Plain capsule must not invent normal maps");
    const std::vector<uint8_t> copied(color.pixels, color.pixels + color.byte_count);
    options.maximum_bake_payload_bytes = dust3d_baked_mesh_payload_bytes(owner.get());
    require(options.maximum_bake_payload_bytes == 256 + 80 * sizeof(Dust3DTextureTangent), "Final payload counts maps and aligned C tangent records exactly");
    Owner exact(dust3d_baked_mesh_load(bytes.data(), bytes.size(), &options, &error), dust3d_baked_mesh_destroy);
    require(exact && std::equal(copied.begin(), copied.end(), color.pixels), "Exact-budget later loads must preserve earlier borrowed pixels");
    --options.maximum_bake_payload_bytes;
    require(!dust3d_baked_mesh_load(bytes.data(), bytes.size(), &options, &error) && error.status == DUST3D_STATUS_LIMIT_EXCEEDED, "One-byte insufficient bake budgets must not publish partial owners");
}
void cancellationWithoutEngineLock()
{
    Dust3DError error {};
    std::unique_ptr<Dust3DCancellationToken, decltype(&dust3d_cancellation_token_destroy)> token(dust3d_cancellation_token_create(&error), dust3d_cancellation_token_destroy);
    require(token && error.status == DUST3D_STATUS_OK, "Cancellation token allocation must succeed");
    dust3d::native::checkCancellation(nullptr);
    dust3d::native::checkCancellation(token.get());
    std::unique_lock<std::mutex> engineLock(dust3d::native::engineMutex());
    auto worker = std::async(std::launch::async, [&] {
        dust3d_cancellation_token_cancel(token.get());
        return dust3d_cancellation_token_is_cancelled(token.get());
    });
    const auto state = worker.wait_for(std::chrono::seconds(2));
    // Always release the lock and join before asserting, including a regression
    // where cancellation incorrectly waits on generation's mutex.
    engineLock.unlock();
    const auto cancelled = worker.get();
    require(state == std::future_status::ready && cancelled, "Cancellation/query must complete while the engine mutex remains held");
    bool rejected = false;
    try {
        dust3d::native::checkCancellation(token.get());
    } catch (const dust3d::native::DocumentError& cause) {
        rejected = cause.status() == DUST3D_STATUS_CANCELLED && std::strcmp(cause.what(), "The native operation was cancelled.") == 0;
    }
    require(rejected, "Cancelled native operations must preserve their typed status and cause");
}
}
int main(int argc, char** argv)
{
    try {
        cancellationWithoutEngineLock();
        nullBakedOwners();
        invalidBakeInputs();
        require(argc == 2, "Baked mesh tests require the fixture directory");
        ownedCapsule(argv[1]);
        std::cout << "Baked mesh boundary contracts passed\n";
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}
