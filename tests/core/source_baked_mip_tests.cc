// SPDX-License-Identifier: MIT
#include <Dust3DBridge.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
using Owner = std::unique_ptr<Dust3DSourceBakedMesh, decltype(&dust3d_source_baked_mesh_destroy)>;
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
std::vector<uint8_t> read(const char* directory, const char* name)
{
    std::ifstream file(std::string(directory) + '/' + name, std::ios::binary);
    require(bool(file), "Missing licensed source mip fixture");
    return { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
}
Dust3DSourceBakeMipOptions options()
{
    Dust3DSourceBakeMipOptions result {};
    result.struct_size = sizeof(result);
    result.source.struct_size = sizeof(result.source);
    result.source.legacy.struct_size = sizeof(result.source.legacy);
    result.source.legacy.atlas_texture_size = 512;
    result.source.legacy.color_size = result.source.legacy.material_size = result.source.legacy.normal_size = 512;
    result.maximum_levels = 2;
    return result;
}
Owner load(const std::vector<uint8_t>& bytes, const Dust3DSourceBakeMipOptions& settings)
{
    Dust3DError error {};
    Owner result(dust3d_source_baked_mesh_load_with_mips(bytes.data(), bytes.size(), &settings, &error), dust3d_source_baked_mesh_destroy);
    require(result && error.status == DUST3D_STATUS_OK, error.message);
    return result;
}
void reject(const std::vector<uint8_t>& bytes, const Dust3DSourceBakeMipOptions* settings, Dust3DStatus expected)
{
    Dust3DError error {};
    Owner result(dust3d_source_baked_mesh_load_with_mips(bytes.data(), bytes.size(), settings, &error), dust3d_source_baked_mesh_destroy);
    require(!result && error.status == expected && error.message[0], "Native mip failure must preserve a typed diagnostic");
}
void verifyLevels(const Dust3DSourceBakedMesh* mesh, bool hasNormal)
{
    const auto cap = dust3d_source_baked_mesh_maximum_sampling_lod(mesh);
    require(cap == 1, "Trusted fixture must retain a real additional mip level");
    for (uint32_t map = 0; map < 5; ++map) {
        const size_t count = dust3d_source_baked_mesh_texture_level_count(mesh, map);
        require(count == (map == DUST3D_TEXTURE_NORMAL && !hasNormal ? 0 : 2), "Every enabled map shares the exact safe level count");
        if (!count)
            continue;
        const auto base = dust3d_source_baked_mesh_texture(mesh, map);
        const auto levelZero = dust3d_source_baked_mesh_texture_level(mesh, map, 0);
        require(base.pixels == levelZero.pixels && base.byte_count == levelZero.byte_count, "Level zero borrows the existing base buffer without duplication");
        const auto mip = dust3d_source_baked_mesh_texture_level(mesh, map, 1);
        require(mip.width == 256 && mip.height == 256 && mip.row_stride == 1024 && mip.byte_count == 256 * 256 * 4 && mip.pixels, "Retained mip dimensions and readable spans are exact");
        require(!dust3d_source_baked_mesh_texture_level(mesh, map, 2).pixels, "Unavailable levels are explicit empty views");
    }
    const auto* geometry = dust3d_source_baked_mesh_geometry(mesh);
    const auto count = dust3d_source_baked_mesh_triangle_count(mesh);
    const auto* records = dust3d_source_baked_mesh_triangles(mesh);
    require(count && dust3d_mesh_vertex_count(geometry) == count * 3, "Native mip owner preserves geometry/triangle alignment");
    for (size_t index = 0; index < count; ++index) {
        require(records[index].uv_known && records[index].policy_known && records[index].tangent.known, "Native mip owner retains mapped policies and tangent provenance");
        for (size_t corner = 0; corner < 3; ++corner)
            for (size_t axis = 0; axis < 2; ++axis)
                require(std::isfinite(records[index].uv[corner][axis]), "Native mip UVs remain finite");
    }
}
void importedAndOwned(const std::vector<uint8_t>& bytes)
{
    Dust3DError error {};
    std::unique_ptr<Dust3DImportedAssets, decltype(&dust3d_imported_assets_destroy)> assets(
        dust3d_imported_assets_load(bytes.data(), bytes.size(), nullptr, &error), dust3d_imported_assets_destroy);
    require(assets != nullptr, error.message);
    const std::string identity(dust3d_imported_assets_material(assets.get(), 0).model_id);
    std::vector<uint8_t> pixels { 128, 128, 128, 128 };
    Dust3DModelBakeImage image { identity.data(), identity.size(), { 1, 1, 4, pixels.size(), pixels.data() } };
    auto settings = options();
    settings.source.model_images = &image;
    settings.source.model_image_count = 1;
    auto owner = load(bytes, settings);
    verifyLevels(owner.get(), false);
    const auto retained = dust3d_source_baked_mesh_texture_level(owner.get(), DUST3D_TEXTURE_COLOR, 1);
    const std::vector<uint8_t> expected(retained.pixels, retained.pixels + retained.byte_count);
    const auto payload = dust3d_source_baked_mesh_payload_bytes(owner.get());
    settings.source.maximum_output_payload_bytes = payload;
    auto exact = load(bytes, settings);
    require(dust3d_source_baked_mesh_payload_bytes(exact.get()) == payload, "Exact native aggregate quota includes all mip levels");
    --settings.source.maximum_output_payload_bytes;
    reject(bytes, &settings, DUST3D_STATUS_LIMIT_EXCEEDED);
    settings.source.maximum_output_payload_bytes = 0;
    settings.source.maximum_raster_samples = 1;
    reject(bytes, &settings, DUST3D_STATUS_LIMIT_EXCEEDED);
    settings.source.maximum_raster_samples = 0;
    Owner legacy(dust3d_source_baked_mesh_load(bytes.data(), bytes.size(), &settings.source, &error), dust3d_source_baked_mesh_destroy);
    require(legacy && dust3d_source_baked_mesh_maximum_sampling_lod(legacy.get()) == 0
            && dust3d_source_baked_mesh_texture_level_count(legacy.get(), DUST3D_TEXTURE_COLOR) == 1,
        "Existing loader retains its base-only contract");
    pixels.assign(4, 0);
    assets.reset();
    exact.reset();
    legacy.reset();
    const auto surviving = dust3d_source_baked_mesh_texture_level(owner.get(), DUST3D_TEXTURE_COLOR, 1);
    require(std::equal(expected.begin(), expected.end(), surviving.pixels), "Mip buffers outlive decoded inputs and unrelated owners");
}
void proceduralAndAdmission(const std::vector<uint8_t>& bytes)
{
    auto settings = options();
    auto owner = load(bytes, settings);
    verifyLevels(owner.get(), false);
    settings.maximum_levels = 1;
    auto base = load(bytes, settings);
    require(dust3d_source_baked_mesh_texture_level_count(base.get(), DUST3D_TEXTURE_COLOR) == 1
            && dust3d_source_baked_mesh_maximum_sampling_lod(base.get()) == 0,
        "Explicit one-level request is base only");
    settings.source.struct_size = 0;
    reject(bytes, &settings, DUST3D_STATUS_INVALID_DOCUMENT);
    settings = options();
    settings.source.legacy.struct_size = 0;
    reject(bytes, &settings, DUST3D_STATUS_INVALID_DOCUMENT);
    settings = options();
    settings.maximum_levels = 15;
    reject(bytes, &settings, DUST3D_STATUS_INVALID_DOCUMENT);
    settings.maximum_levels = 2;
    settings.struct_size = sizeof(settings) - 1;
    reject(bytes, &settings, DUST3D_STATUS_INVALID_DOCUMENT);
    reject(bytes, nullptr, DUST3D_STATUS_INVALID_DOCUMENT);
    settings = options();
    Dust3DError error {};
    std::unique_ptr<Dust3DCancellationToken, decltype(&dust3d_cancellation_token_destroy)> token(
        dust3d_cancellation_token_create(&error), dust3d_cancellation_token_destroy);
    require(token != nullptr, error.message);
    dust3d_cancellation_token_cancel(token.get());
    settings.source.legacy.cancellation = token.get();
    reject(bytes, &settings, DUST3D_STATUS_CANCELLED);
    require(!dust3d_source_baked_mesh_texture_level(nullptr, 0, 0).pixels
            && !dust3d_source_baked_mesh_texture_level(owner.get(), 999, 0).pixels
            && !dust3d_source_baked_mesh_texture_level(owner.get(), 0, SIZE_MAX).pixels
            && dust3d_source_baked_mesh_texture_level_count(nullptr, 0) == 0
            && dust3d_source_baked_mesh_texture_level_count(owner.get(), 999) == 0,
        "Null, absent and out-of-range native mip views are safe");
}
}
int main(int argc, char** argv)
{
    try {
        require(argc == 2, "Fixture path required");
        importedAndOwned(read(argv[1], "ImportedSurface.ds3"));
        proceduralAndAdmission(read(argv[1], "Capsule.ds3"));
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
