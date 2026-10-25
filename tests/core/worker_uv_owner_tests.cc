// SPDX-License-Identifier: MIT
#include <Dust3DBridge.h>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <native/Engine/WorkerUv.h>
#include <stdexcept>

using namespace dust3d::worker;
namespace {
void require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}
UvAssociation kind(Dust3DUvComponentKind association)
{
    require(association == DUST3D_UV_UNKNOWN || association == DUST3D_UV_COMPONENT || association == DUST3D_UV_AMBIGUOUS, "Native association is supported");
    return static_cast<UvAssociation>(association);
}
uint64_t index(Dust3DUvComponentKind association, size_t value, size_t components)
{
    require(association == DUST3D_UV_COMPONENT ? value < components : value == SIZE_MAX, "Native association/index pair is valid");
    return association == DUST3D_UV_COMPONENT ? static_cast<uint64_t>(value) : noUvComponent;
}
}
int main(int argc, char** argv)
{
    try {
        require(argc == 2, "Expected the licensed Capsule fixture");
        require(std::filesystem::file_size(argv[1]) < 1024 * 1024, "Ordinary licensed input stays small");
        std::ifstream input(argv[1], std::ios::binary);
        require(bool(input), "Fixture opens");
        const std::vector<uint8_t> archive { std::istreambuf_iterator<char>(input), {} };
        Dust3DUvOptions options { sizeof(options), 0, 64, nullptr, 0 };
        Dust3DError error {};
        std::unique_ptr<Dust3DUvMesh, decltype(&dust3d_uv_mesh_destroy)> owner(dust3d_uv_mesh_load(archive.data(), archive.size(), &options, &error), dust3d_uv_mesh_destroy);
        require(owner && error.status == DUST3D_STATUS_OK, "Ordinary Capsule UV owner succeeds");
        const auto count = dust3d_uv_mesh_triangle_count(owner.get());
        const auto components = dust3d_uv_mesh_component_count(owner.get());
        require(count && count <= maximumUvRecords && count * 3 == dust3d_mesh_vertex_count(dust3d_uv_mesh_geometry(owner.get())), "Native UV order aligns with retained geometry triangles");
        UvPayload payload;
        payload.packedTextureSize = dust3d_uv_mesh_texture_size(owner.get());
        for (size_t i = 0; i < components; ++i) {
            const auto* identity = dust3d_uv_mesh_component_id(owner.get(), i);
            require(identity != nullptr, "Native original component key exists");
            payload.components.emplace_back(identity);
        }
        for (size_t i = 0; i < count; ++i) {
            const auto& source = dust3d_uv_mesh_triangles(owner.get())[i];
            UvTriangle target;
            for (size_t corner = 0; corner < 3; ++corner)
                std::copy(source.uv[corner], source.uv[corner] + 2, target.uv[corner].begin());
            target.known = source.known;
            target.association = kind(source.component_kind);
            target.componentIndex = index(source.component_kind, source.component_index, components);
            payload.triangles.push_back(target);
        }
        for (size_t i = 0; i < dust3d_uv_mesh_chart_count(owner.get()); ++i) {
            const auto& source = dust3d_uv_mesh_charts(owner.get())[i];
            UvChart target;
            target.left = source.left;
            target.top = source.top;
            target.width = source.width;
            target.height = source.height;
            std::copy(source.color, source.color + 4, target.color.begin());
            target.flipped = source.flipped;
            target.association = kind(source.component_kind);
            target.componentIndex = index(source.component_kind, source.component_index, components);
            const auto* end = std::find(std::begin(source.image_id), std::end(source.image_id), '\0');
            require(end != std::end(source.image_id), "Native chart image ID terminates within its published buffer");
            target.imageID.assign(source.image_id, end);
            payload.charts.push_back(target);
        }
        for (size_t i = 0; i < dust3d_uv_mesh_gradient_count(owner.get()); ++i) {
            const auto& source = dust3d_uv_mesh_gradients(owner.get())[i];
            UvGradient target;
            target.chartIndex = source.chart_index;
            std::copy(source.large_color, source.large_color + 4, target.largeColor.begin());
            std::copy(source.small_color, source.small_color + 4, target.smallColor.begin());
            target.resolution = source.resolution;
            payload.gradients.push_back(target);
        }
        owner.reset();
        auto bytes = encodeUvPayload(payload, count, {});
        auto decoded = decodeUvPayload(bytes.data(), bytes.size(), count, {});
        require(payload.components == decoded.components && payload.packedTextureSize == decoded.packedTextureSize && !decoded.charts.empty(), "Copied native atlas metadata survives owner destruction");
        require(encodeUvPayload(decoded, count, {}) == bytes, "Complete native metadata survives independent wire decode");
        for (size_t i = 0; i < count; ++i)
            require(std::memcmp(payload.triangles[i].uv.data(), decoded.triangles[i].uv.data(), 6 * sizeof(double)) == 0, "Native corner double bits survive transport without narrowing");
        std::fill(bytes.begin(), bytes.end(), 0);
        payload = {};
        require(decoded.triangles.size() == count && !decoded.components.empty(), "Decoded native metadata is independently owned");
        std::cout << "Ordinary native UV owner transport passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
