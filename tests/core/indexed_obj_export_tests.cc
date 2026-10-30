// SPDX-License-Identifier: MIT
#include "EngineInternal.h"
#include <Dust3DBridge.h>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <locale>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool condition, const char* reason)
{
    if (!condition)
        throw std::runtime_error(reason);
}
Dust3DVertex vertex(float x, float y, float z, float nx, float ny, float nz)
{
    return { { x, y, z }, { nx, ny, nz }, { 1, 1, 1, 1 } };
}
void verifyObj(const uint8_t* bytes, size_t count, const std::vector<Dust3DVertex>& original, const std::vector<uint32_t>& indices)
{
    std::istringstream input(std::string(reinterpret_cast<const char*>(bytes), count));
    input.imbue(std::locale::classic());
    std::vector<std::array<float, 3>> positions, normals;
    std::vector<size_t> readIndices;
    std::string line;
    while (std::getline(input, line)) {
        std::istringstream row(line);
        row.imbue(std::locale::classic());
        std::string kind;
        row >> kind;
        if (kind == "v" || kind == "vn") {
            std::array<double, 3> values {};
            require(bool(row >> values[0] >> values[1] >> values[2]), "Independent parser must read vectors");
            require(std::isfinite(values[0]) && std::isfinite(values[1]) && std::isfinite(values[2]), "Parsed values must remain finite");
            (kind == "v" ? positions : normals).push_back({ static_cast<float>(values[0]), static_cast<float>(values[1]), static_cast<float>(values[2]) });
        } else if (kind == "f") {
            for (size_t corner = 0; corner < 3; ++corner) {
                std::string item;
                require(bool(row >> item), "Face must contain three corners");
                const auto separator = item.find("//");
                require(separator != std::string::npos, "Each face corner must reference its normal");
                const auto position = std::stoul(item.substr(0, separator));
                const auto normal = std::stoul(item.substr(separator + 2));
                require(position && position == normal, "Face indices must be one-based and aligned");
                readIndices.push_back(position - 1);
            }
            std::string extra;
            require(!(row >> extra), "Indexed output contains triangles only");
        } else {
            require(kind == "#", "Only geometry records are expected");
        }
    }
    require(positions.size() == original.size() && normals.size() == original.size(), "Vertex and normal counts must match");
    require(readIndices == std::vector<size_t>(indices.begin(), indices.end()), "Triangle winding and index order must match");
    for (size_t i = 0; i < original.size(); ++i)
        for (size_t axis = 0; axis < 3; ++axis)
            require(positions[i][axis] == original[i].position[axis] && normals[i][axis] == original[i].normal[axis], "Position/normal values must round-trip as floats");
}
class DecimalComma : public std::numpunct<char> {
protected:
    char do_decimal_point() const override { return ','; }
};
void indexedObjPreservesGeometryAndOwnership()
{
    auto mesh = std::make_unique<Dust3DIndexedMesh>();
    const float precise = std::nextafter(1.0f, 2.0f);
    mesh->vertices = { vertex(precise, 0, 0, 0, 0, 1), vertex(1, 1, 0, 0, 0, 1),
        vertex(0, 1, 0, 0, 0, 1), vertex(precise, 0, 0, 0, 1, 0), vertex(0, 0, 1, 0, 1, 0),
        vertex(-std::numeric_limits<float>::max(), std::numeric_limits<float>::min(), std::numeric_limits<float>::denorm_min(), -1, 0, 0),
        vertex(std::numeric_limits<float>::max(), -std::numeric_limits<float>::min(), -std::numeric_limits<float>::denorm_min(), 0, -1, 0),
        vertex(-3, -2, -1, 0, 0, -1) };
    mesh->indices = { 0, 1, 2, 3, 2, 4, 5, 6, 7 };
    mesh->warning = "Retained source warning";
    const auto original = mesh->vertices;
    const auto indices = mesh->indices;
    Dust3DError error {};
    Dust3DObjExportOptions options {};
    options.struct_size = sizeof(options);
    using Owner = std::unique_ptr<Dust3DObjExport, decltype(&dust3d_obj_export_destroy)>;
    const auto oldLocale = std::locale();
    std::locale::global(std::locale(oldLocale, new DecimalComma));
    Owner output(dust3d_indexed_mesh_export_obj(mesh.get(), &options, &error), dust3d_obj_export_destroy);
    std::locale::global(oldLocale);
    require(output && error.status == DUST3D_STATUS_OK, "Indexed OBJ should succeed");
    const auto payload = dust3d_obj_export_payload_bytes(output.get());
    options.maximum_output_payload_bytes = payload;
    Owner exact(dust3d_indexed_mesh_export_obj(mesh.get(), &options, &error), dust3d_obj_export_destroy);
    require(bool(exact), "Exact output quota should succeed");
    options.maximum_output_payload_bytes = payload - 1;
    require(!dust3d_indexed_mesh_export_obj(mesh.get(), &options, &error) && error.status == DUST3D_STATUS_LIMIT_EXCEEDED,
        "One-byte-short output quota should reject");
    mesh.reset();
    require(std::strcmp(dust3d_obj_export_warning(output.get()), "Retained source warning") == 0, "Warning must outlive indexed source");
    const auto* bytes = dust3d_obj_export_bytes(output.get());
    const auto count = dust3d_obj_export_byte_count(output.get());
    require(bytes && count, "Output bytes must outlive indexed source");
    verifyObj(bytes, count, original, indices);
}
void capsuleRoundTrip(const char* path)
{
    std::ifstream input(path, std::ios::binary);
    require(bool(input), "Capsule fixture must open");
    const std::vector<uint8_t> bytes { std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
    Dust3DError error {};
    std::unique_ptr<Dust3DMesh, decltype(&dust3d_mesh_destroy)> source(dust3d_mesh_load(bytes.data(), bytes.size(), &error), dust3d_mesh_destroy);
    require(bool(source), "Capsule procedural mesh must load");
    std::unique_ptr<Dust3DIndexedMesh, decltype(&dust3d_indexed_mesh_destroy)> indexed(dust3d_mesh_create_indexed(source.get(), 0, &error), dust3d_indexed_mesh_destroy);
    require(bool(indexed), "Capsule indexed geometry must build");
    const auto vertexCount = dust3d_indexed_mesh_vertex_count(indexed.get());
    const auto indexCount = dust3d_indexed_mesh_index_count(indexed.get());
    const auto* vertices = dust3d_indexed_mesh_vertices(indexed.get());
    const auto* indices = dust3d_indexed_mesh_indices(indexed.get());
    require(vertexCount && indexCount && vertices && indices, "Capsule indexed buffers must be nonempty");
    const std::vector<Dust3DVertex> original(vertices, vertices + vertexCount);
    const std::vector<uint32_t> originalIndices(indices, indices + indexCount);
    Dust3DObjExportOptions options {};
    options.struct_size = sizeof(options);
    std::unique_ptr<Dust3DObjExport, decltype(&dust3d_obj_export_destroy)> output(dust3d_indexed_mesh_export_obj(indexed.get(), &options, &error), dust3d_obj_export_destroy);
    require(bool(output), "Capsule indexed OBJ must encode");
    indexed.reset();
    source.reset();
    const auto* encoded = dust3d_obj_export_bytes(output.get());
    const auto count = dust3d_obj_export_byte_count(output.get());
    require(encoded && count, "Capsule output must own its bytes");
    verifyObj(encoded, count, original, originalIndices);
}
void invalidOwnedInputsReject()
{
    Dust3DError error {};
    Dust3DObjExportOptions options {};
    options.struct_size = sizeof(options);
    require(!dust3d_indexed_mesh_export_obj(nullptr, &options, &error) && error.status == DUST3D_STATUS_INVALID_DOCUMENT, "NULL source must reject");
    Dust3DIndexedMesh mesh;
    mesh.vertices = { vertex(0, 0, 0, 0, 0, 1), vertex(1, 0, 0, 0, 0, 1), vertex(0, 1, 0, 0, 0, 1) };
    require(!dust3d_indexed_mesh_export_obj(&mesh, &options, &error) && error.status == DUST3D_STATUS_INVALID_DOCUMENT, "Empty indices must reject");
    mesh.indices = { 0, 1 };
    require(!dust3d_indexed_mesh_export_obj(&mesh, &options, &error) && error.status == DUST3D_STATUS_INVALID_DOCUMENT, "Partial triangle must reject");
    mesh.indices = { 0, 1, 3 };
    require(!dust3d_indexed_mesh_export_obj(&mesh, &options, &error) && error.status == DUST3D_STATUS_INVALID_DOCUMENT, "Out-of-bounds index must reject");
    mesh.indices = { 0, 1, 2 };
    require(!dust3d_indexed_mesh_export_obj(&mesh, nullptr, &error) && error.status == DUST3D_STATUS_INVALID_DOCUMENT, "NULL options must reject");
    --options.struct_size;
    require(!dust3d_indexed_mesh_export_obj(&mesh, &options, &error) && error.status == DUST3D_STATUS_INVALID_DOCUMENT, "Bad option size must reject");
    options.struct_size = sizeof(options);
    mesh.vertices[0].position[0] = std::numeric_limits<float>::infinity();
    require(!dust3d_indexed_mesh_export_obj(&mesh, &options, &error) && error.status == DUST3D_STATUS_INVALID_DOCUMENT, "Nonfinite position must reject");
    mesh.vertices[0].position[0] = 0;
    std::unique_ptr<Dust3DCancellationToken, decltype(&dust3d_cancellation_token_destroy)> cancellation(dust3d_cancellation_token_create(&error), dust3d_cancellation_token_destroy);
    require(bool(cancellation), "Cancellation token must be created");
    dust3d_cancellation_token_cancel(cancellation.get());
    options.cancellation = cancellation.get();
    require(!dust3d_indexed_mesh_export_obj(&mesh, &options, &error) && error.status == DUST3D_STATUS_CANCELLED, "Pre-cancelled export must not publish");
    options.cancellation = nullptr;
    mesh.vertices[0].normal[0] = std::numeric_limits<float>::quiet_NaN();
    require(!dust3d_indexed_mesh_export_obj(&mesh, &options, &error) && error.status == DUST3D_STATUS_INVALID_DOCUMENT, "Nonfinite normal must reject");
}
}
int main(int argc, char** argv)
{
    try {
        require(argc >= 2, "Expected test scenario");
        const std::string scenario = argv[1];
        if (scenario == "pure") {
            indexedObjPreservesGeometryAndOwnership();
            invalidOwnedInputsReject();
        } else if (scenario == "capsule" && argc == 3) {
            capsuleRoundTrip(argv[2]);
        } else {
            throw std::runtime_error("Unknown test scenario");
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
