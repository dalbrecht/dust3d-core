// SPDX-License-Identifier: MIT
#include <Dust3DBridge.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <native/Engine/WorkerImportedSurfaceCopy.h>
#include <stdexcept>
using namespace dust3d::worker;
namespace {
void require(bool condition, const char* cause)
{
    if (!condition)
        throw std::runtime_error(cause);
}
void healthy(const char* directory, const char* name)
{
    std::ifstream input(std::string(directory) + '/' + name, std::ios::binary);
    require(bool(input), "Existing licensed positive fixture opens");
    const std::vector<uint8_t> archive { std::istreambuf_iterator<char>(input), {} };
    Dust3DError error {};
    Dust3DImportedSurfaceOptions options {};
    options.struct_size = sizeof(options);
    std::unique_ptr<Dust3DImportedSurfaceMesh, decltype(&dust3d_imported_surface_mesh_destroy)> owner(
        dust3d_imported_surface_mesh_load(archive.data(), archive.size(), &options, &error), dust3d_imported_surface_mesh_destroy);
    require(owner != nullptr, error.message);
    auto copy = copyNativeImportedSurface(owner.get(), {});
    require(copy.triangles.size() == dust3d_imported_surface_mesh_triangle_count(owner.get())
            && copy.geometry.vertices.size() == copy.triangles.size() * 3,
        "One native owner's geometry and source records correspond");
    const auto* records = dust3d_imported_surface_mesh_triangles(owner.get());
    for (size_t index = 0; index < copy.triangles.size(); ++index) {
        const auto& actual = copy.triangles[index];
        const auto& expected = records[index];
        require(std::memcmp(actual.uv.data(), expected.uv, sizeof(expected.uv)) == 0
                && std::memcmp(actual.colors.data(), expected.color, sizeof(expected.color)) == 0
                && actual.uvKnown == expected.uv_known && actual.colorEncoding == uint32_t(expected.color_encoding)
                && actual.modelIndex == expected.model_index && actual.componentIndex == expected.component_index,
            "All authored scalar bits, flags and catalog references survive");
    }
    for (size_t index = 0; index < copy.models.size(); ++index)
        require(copy.models[index] == dust3d_imported_surface_mesh_model_id(owner.get(), index), "Model catalog spelling survives");
    for (size_t index = 0; index < copy.components.size(); ++index)
        require(copy.components[index].id == dust3d_imported_surface_mesh_component_id(owner.get(), index)
                && copy.components[index].kind == uint32_t(dust3d_imported_surface_mesh_component_kind(owner.get(), index)),
            "Component spelling and membership survive");
    for (int field = 0; field < 3; ++field) {
        ImportedSurfaceLimits limits;
        if (field == 0)
            limits.maximumWireBytes = 1;
        if (field == 1)
            limits.maximumDecodedBytes = 1;
        if (field == 2)
            limits.surface.maximumDecodedBytes = 1;
        bool rejected = false;
        try {
            (void)copyNativeImportedSurface(owner.get(), limits);
        } catch (const ImportedSurfaceError& cause) {
            rejected = cause.code() == ImportedSurfaceErrorCode::limitExceeded && cause.what()[0];
        }
        require(rejected, "Copy-only quotas reject using the same healthy owner");
    }
    const auto bytes = encodeImportedSurfacePayload(copy, {});
    owner.reset();
    require(encodeImportedSurfacePayload(copy, {}) == bytes
            && encodeImportedSurfacePayload(decodeImportedSurfacePayload(bytes.data(), bytes.size(), {}), {}) == bytes,
        "All copied storage outlives the native owner and wire round trips");
}
}
int main(int argc, char** argv)
{
    try {
        require(argc == 2, "Existing positive fixture directory required");
        healthy(argv[1], "ImportedSurface.ds3");
        healthy(argv[1], "Capsule.ds3");
        bool rejected = false;
        try {
            (void)copyNativeImportedSurface(nullptr, {});
        } catch (const ImportedSurfaceError& cause) {
            rejected = cause.code() == ImportedSurfaceErrorCode::invalidInput;
        }
        require(rejected, "Null owner rejects before native getters");
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
    return 0;
}
