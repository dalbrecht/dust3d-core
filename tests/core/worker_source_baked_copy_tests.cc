// SPDX-License-Identifier: MIT
#include <Dust3DBridge.h>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <native/Engine/WorkerSourceBakedCopy.h>
#include <stdexcept>
using namespace dust3d::worker;
namespace {
void require(bool value, const char* cause)
{
    if (!value)
        throw std::runtime_error(cause);
}
void healthy(const char* directory)
{
    std::ifstream input(std::string(directory) + "/ImportedSurface.ds3", std::ios::binary);
    require(bool(input), "Licensed ordinary imported fixture opens");
    const std::vector<uint8_t> archive { std::istreambuf_iterator<char>(input), {} };
    Dust3DError error {};
    std::unique_ptr<Dust3DImportedAssets, decltype(&dust3d_imported_assets_destroy)> assets(
        dust3d_imported_assets_load(archive.data(), archive.size(), nullptr, &error), dust3d_imported_assets_destroy);
    require(assets != nullptr, error.message);
    const std::string model(dust3d_imported_assets_material(assets.get(), 0).model_id);
    const uint8_t decoded[] { 128, 128, 128, 128 };
    Dust3DModelBakeImage image { model.data(), model.size(), { 1, 1, 4, sizeof(decoded), decoded } };
    Dust3DSourceBakeMipOptions options {};
    options.struct_size = sizeof(options);
    options.source.struct_size = sizeof(options.source);
    options.source.legacy.struct_size = sizeof(options.source.legacy);
    options.source.legacy.atlas_texture_size = 512;
    options.source.legacy.color_size = options.source.legacy.material_size = options.source.legacy.normal_size = 512;
    options.source.model_images = &image;
    options.source.model_image_count = 1;
    options.maximum_levels = 2;
    std::unique_ptr<Dust3DSourceBakedMesh, decltype(&dust3d_source_baked_mesh_destroy)> owner(
        dust3d_source_baked_mesh_load_with_mips(archive.data(), archive.size(), &options, &error), dust3d_source_baked_mesh_destroy);
    require(owner != nullptr, error.message);
    auto copy = copyNativeSourceBakedMesh(owner.get(), {});
    require(copy.geometry.vertices.size() == 12 && copy.surface.triangles.size() == 4 && copy.surface.models == std::vector<std::string> { model }, "Single-owner geometry, records and model catalogs retain correspondence");
    const auto* geometry = dust3d_source_baked_mesh_geometry(owner.get());
    require(copy.geometry.nodeCount == dust3d_mesh_node_count(geometry) && copy.geometry.partCount == dust3d_mesh_part_count(geometry)
            && copy.geometry.warning == dust3d_mesh_warning(geometry),
        "Native source counts and original warning are preserved");
    for (size_t index = 0; index < copy.geometry.vertices.size(); ++index) {
        const auto& actual = copy.geometry.vertices[index];
        const auto& expected = dust3d_mesh_vertices(geometry)[index];
        require(std::memcmp(actual.position.data(), expected.position, sizeof(expected.position)) == 0
                && std::memcmp(actual.normal.data(), expected.normal, sizeof(expected.normal)) == 0
                && std::memcmp(actual.color.data(), expected.color, sizeof(expected.color)) == 0,
            "Every retained vertex attribute bit stays in order");
    }
    require(copy.surface.components.size() == dust3d_source_baked_mesh_component_count(owner.get()), "Component count stays exact");
    for (size_t index = 0; index < copy.surface.components.size(); ++index)
        require(copy.surface.components[index].id == dust3d_source_baked_mesh_component_id(owner.get(), index)
                && copy.surface.components[index].kind == uint32_t(dust3d_source_baked_mesh_component_kind(owner.get(), index)),
            "Exact component keys and membership survive copying");
    for (int quota = 0; quota < 5; ++quota) {
        SourceBakedLimits limits;
        if (quota == 0)
            limits.maximumWireBytes = 1;
        if (quota == 1)
            limits.maximumDecodedBytes = 1;
        if (quota == 2)
            limits.surface.maximumWireBytes = 1;
        if (quota == 3)
            limits.geometry.maximumWireBytes = 1;
        if (quota == 4)
            limits.surface.maximumTextBytes = 1;
        bool rejected = false;
        try {
            (void)copyNativeSourceBakedMesh(owner.get(), limits);
        } catch (const SourceBakedError& cause) {
            rejected = cause.code() == SourceBakedErrorCode::limitExceeded && cause.what()[0];
        }
        require(rejected, "Copy-only quota rejection preserves its original cause without regenerating the fixture");
    }
    bool missing = false;
    try {
        (void)copyNativeSourceBakedMesh(nullptr, {});
    } catch (const SourceBakedError& cause) {
        missing = cause.code() == SourceBakedErrorCode::invalidInput;
    }
    require(missing, "Missing borrowed owner rejects before native accessor calls");
    for (size_t index = 0; index < copy.surface.triangles.size(); ++index) {
        const auto& expected = dust3d_source_baked_mesh_triangles(owner.get())[index];
        const auto& actual = copy.surface.triangles[index];
        require(actual.uvKnown == expected.uv_known && actual.policyKnown == expected.policy_known && actual.tangentKnown == expected.tangent.known
                && actual.alphaMode == uint32_t(expected.alpha_mode) && actual.alphaCutoff == expected.alpha_cutoff && actual.isDoubleSided == expected.is_double_sided
                && actual.modelIndex == (expected.model_index == SIZE_MAX ? noBakedIdentity : expected.model_index) && actual.componentIndex == (expected.component_index == SIZE_MAX ? noBakedIdentity : expected.component_index) && actual.handedness == expected.tangent.handedness,
            "Native packed mapping, source policies and catalog references survive exactly");
        for (size_t corner = 0; corner < 3; ++corner)
            require(std::memcmp(actual.uv[corner].data(), expected.uv[corner], sizeof(expected.uv[corner])) == 0, "All packed UV bits retain triangle order");
        require(std::memcmp(actual.tangent.data(), expected.tangent.direction, sizeof(expected.tangent.direction)) == 0, "Tangent bits remain unchanged");
    }
    for (size_t role = 0; role < copy.textures.maps.size(); ++role) {
        const auto levels = dust3d_source_baked_mesh_texture_level_count(owner.get(), uint32_t(role));
        require(copy.textures.maps[role].size() == levels, "Optional map and base-inclusive levels stay exact");
        for (size_t level = 0; level < levels; ++level) {
            const auto view = dust3d_source_baked_mesh_texture_level(owner.get(), uint32_t(role), level);
            const auto& image = copy.textures.maps[role][level];
            require(image.width == view.width && image.height == view.height && image.pixels.size() == view.byte_count
                    && std::equal(image.pixels.begin(), image.pixels.end(), view.pixels),
                "Every base/mip pixel and dimension is unchanged");
        }
    }
    require(copy.textures.maximumSamplingLod == dust3d_source_baked_mesh_maximum_sampling_lod(owner.get()), "Producer chart-safe cap is retained");
    const auto expected = encodeSourceBakedPayload(copy, {});
    owner.reset();
    assets.reset();
    require(encodeSourceBakedPayload(copy, {}) == expected, "Copied full rendering data survives native owner release");
    require(encodeSourceBakedPayload(decodeSourceBakedPayload(expected.data(), expected.size(), {}), {}) == expected, "Native output makes a complete owned codec round trip");
}
}
int main(int argc, char** argv)
{
    try {
        require(argc == 2, "Known-positive fixture directory only");
        healthy(argv[1]);
        std::cout << "Source-baked native copy passed\n";
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}
