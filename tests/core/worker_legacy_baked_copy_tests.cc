// SPDX-License-Identifier: MIT
#include <Dust3DBridge.h>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <native/Engine/WorkerLegacyBakedCopy.h>
#include <stdexcept>
using namespace dust3d::worker;
namespace {
void require(bool condition, const char* cause)
{
    if (!condition)
        throw std::runtime_error(cause);
}
void healthy(const char* directory)
{
    std::ifstream input(std::string(directory) + "/Capsule.ds3", std::ios::binary);
    require(bool(input), "Licensed ordinary Capsule fixture opens");
    const std::vector<uint8_t> archive { std::istreambuf_iterator<char>(input), {} };
    Dust3DBakeOptions options {};
    options.struct_size = sizeof(options);
    options.color_size = options.normal_size = options.material_size = 8;
    Dust3DError error {};
    std::unique_ptr<Dust3DBakedMesh, decltype(&dust3d_baked_mesh_destroy)> owner(
        dust3d_baked_mesh_load(archive.data(), archive.size(), &options, &error), dust3d_baked_mesh_destroy);
    require(owner != nullptr, error.message);
    auto copy = copyNativeLegacyBakedMesh(owner.get(), {});
    const auto* uv = dust3d_baked_mesh_uvs(owner.get());
    const auto* geometry = dust3d_uv_mesh_geometry(uv);
    require(copy.geometry.vertices.size() == dust3d_mesh_vertex_count(geometry) && copy.tangents.size() == dust3d_baked_mesh_tangent_count(owner.get())
            && copy.uv.triangles.size() == copy.tangents.size(),
        "Legacy geometry, UV and tangent counts remain coherent");
    for (size_t index = 0; index < copy.geometry.vertices.size(); ++index) {
        const auto& actual = copy.geometry.vertices[index];
        const auto& expected = dust3d_mesh_vertices(geometry)[index];
        require(std::memcmp(actual.position.data(), expected.position, sizeof(expected.position)) == 0
                && std::memcmp(actual.normal.data(), expected.normal, sizeof(expected.normal)) == 0
                && std::memcmp(actual.color.data(), expected.color, sizeof(expected.color)) == 0,
            "All retained vertex bits remain in order");
    }
    for (size_t index = 0; index < copy.tangents.size(); ++index) {
        const auto& tangent = dust3d_baked_mesh_tangents(owner.get())[index];
        require(std::memcmp(copy.tangents[index].direction.data(), tangent.direction, sizeof(tangent.direction)) == 0
                && copy.tangents[index].handedness == tangent.handedness && copy.tangents[index].known == tangent.known,
            "Every original tangent scalar and mapping flag survives");
        const auto& triangle = dust3d_uv_mesh_triangles(uv)[index];
        for (size_t corner = 0; corner < 3; ++corner)
            require(std::memcmp(copy.uv.triangles[index].uv[corner].data(), triangle.uv[corner], sizeof(triangle.uv[corner])) == 0, "UV bits remain aligned with tangents");
    }
    require(copy.uv.charts.size() == dust3d_uv_mesh_chart_count(uv) && copy.uv.gradients.size() == dust3d_uv_mesh_gradient_count(uv)
            && copy.uv.components.size() == dust3d_uv_mesh_component_count(uv),
        "Legacy catalogs, charts and gradients retain actual counts");
    require(copy.geometry.nodeCount == dust3d_mesh_node_count(geometry) && copy.geometry.partCount == dust3d_mesh_part_count(geometry)
            && copy.geometry.warning == dust3d_mesh_warning(geometry) && copy.uv.packedTextureSize == dust3d_uv_mesh_texture_size(uv),
        "Original source counts, warning and atlas size survive");
    for (size_t index = 0; index < copy.uv.components.size(); ++index)
        require(copy.uv.components[index] == dust3d_uv_mesh_component_id(uv, index), "Every component identity remains byte exact");
    for (size_t index = 0; index < copy.uv.charts.size(); ++index) {
        const auto& actual = copy.uv.charts[index];
        const auto& expected = dust3d_uv_mesh_charts(uv)[index];
        require(actual.left == expected.left && actual.top == expected.top && actual.width == expected.width && actual.height == expected.height
                && actual.flipped == expected.flipped && actual.imageID == expected.image_id
                && uint32_t(actual.association) == uint32_t(expected.component_kind)
                && actual.componentIndex == (expected.component_index == SIZE_MAX ? noUvComponent : expected.component_index)
                && std::memcmp(actual.color.data(), expected.color, sizeof(expected.color)) == 0,
            "Every chart bound/color/rotation/image association survives");
    }
    for (size_t index = 0; index < copy.uv.gradients.size(); ++index) {
        const auto& actual = copy.uv.gradients[index];
        const auto& expected = dust3d_uv_mesh_gradients(uv)[index];
        require(actual.chartIndex == expected.chart_index && actual.resolution == expected.resolution
                && std::memcmp(actual.largeColor.data(), expected.large_color, sizeof(expected.large_color)) == 0
                && std::memcmp(actual.smallColor.data(), expected.small_color, sizeof(expected.small_color)) == 0,
            "Every generated gradient scalar survives when present");
    }
    for (size_t role = 0; role < bakedTextureRoleCount; ++role) {
        const auto image = dust3d_baked_mesh_texture(owner.get(), uint32_t(role));
        require(copy.textures.maps[role].size() == (image.pixels ? 1 : 0), "Optional maps remain explicit and base only");
        if (image.pixels)
            require(copy.textures.maps[role][0].width == image.width && copy.textures.maps[role][0].height == image.height
                    && copy.textures.maps[role][0].pixels.size() == image.byte_count && std::equal(copy.textures.maps[role][0].pixels.begin(), copy.textures.maps[role][0].pixels.end(), image.pixels),
                "All base pixel bytes survive");
    }
    for (int field = 0; field < 2; ++field) {
        LegacyBakedLimits limits;
        if (!field)
            limits.maximumWireBytes = 1;
        else
            limits.maximumDecodedBytes = 1;
        bool rejected = false;
        try {
            (void)copyNativeLegacyBakedMesh(owner.get(), limits);
        } catch (const LegacyBakedError& cause) {
            rejected = cause.code() == LegacyBakedErrorCode::limitExceeded;
        }
        require(rejected, "Copy-only aggregate rejection uses the existing healthy owner without regeneration");
    }
    const auto wire = encodeLegacyBakedPayload(copy, {});
    owner.reset();
    require(encodeLegacyBakedPayload(copy, {}) == wire && encodeLegacyBakedPayload(decodeLegacyBakedPayload(wire.data(), wire.size(), {}), {}) == wire,
        "Coherent legacy values own all buffers after native destruction");
    bool missing = false;
    try {
        (void)copyNativeLegacyBakedMesh(nullptr, {});
    } catch (const LegacyBakedError& cause) {
        missing = cause.code() == LegacyBakedErrorCode::invalidInput;
    }
    require(missing, "Missing owner rejects before native accessors");
}
}
int main(int argc, char** argv)
{
    try {
        require(argc == 2, "Known-positive fixture directory only");
        healthy(argv[1]);
        std::cout << "Legacy baked native copy passed\n";
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}
