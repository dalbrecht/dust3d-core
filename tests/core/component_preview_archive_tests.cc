// SPDX-License-Identifier: MIT
#include "Dust3DBridge.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
}

int main(int argc, char** argv)
{
    try {
        require(argc == 2, "Expected the checked-in Capsule archive path");
        std::ifstream file(argv[1], std::ios::binary);
        require(file.is_open(), "Could not open the Capsule fixture");
        const std::vector<uint8_t> archive((std::istreambuf_iterator<char>(file)), {});
        require(!archive.empty(), "The Capsule fixture is empty");

        constexpr char componentId[] = "{bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb}";
        const Dust3DComponentPreviewID request { componentId, sizeof(componentId) - 1 };
        Dust3DError error {};
        using Batch = std::unique_ptr<Dust3DComponentPreviewBatch, decltype(&dust3d_component_preview_batch_destroy)>;
        const Batch batch(dust3d_component_preview_batch_load(archive.data(), archive.size(), &request, 1, 0, nullptr, &error),
            dust3d_component_preview_batch_destroy);
        require(batch && error.status == DUST3D_STATUS_OK, error.message);
        require(dust3d_component_preview_batch_count(batch.get()) == 1, "The Capsule batch must have one slot");
        const auto* preview = dust3d_component_preview_batch_at(batch.get(), 0);
        require(preview && dust3d_component_preview_cut_face_count(preview) == 0, "The Capsule must produce mesh geometry");
        const auto vertexCount = dust3d_component_preview_vertex_count(preview);
        const auto triangleCount = dust3d_component_preview_triangle_count(preview);
        require(vertexCount > 0 && triangleCount > 0, "The Capsule preview must have vertices and triangles");
        const auto* vertices = dust3d_component_preview_vertices(preview);
        const auto* triangles = dust3d_component_preview_triangles(preview);
        require(vertices && triangles, "The owned Capsule arrays must be readable");
        for (size_t vertex = 0; vertex < vertexCount; ++vertex)
            for (const auto coordinate : vertices[vertex].position)
                require(std::isfinite(coordinate), "The Capsule position must be finite");
        for (size_t triangle = 0; triangle < triangleCount; ++triangle)
            for (const auto index : triangles[triangle].indices)
                require(index < vertexCount, "The Capsule triangle must reference an owned vertex");

        const Dust3DComponentPreviewID duplicate[] { request, request };
        const Batch rejected(dust3d_component_preview_batch_load(archive.data(), archive.size(), duplicate, 2, 0, nullptr, &error),
            dust3d_component_preview_batch_destroy);
        require(!rejected && error.status == DUST3D_STATUS_INVALID_DOCUMENT, "Duplicate requests must fail atomically");
        require(dust3d_component_preview_triangle_count(preview) == triangleCount, "A later request must not invalidate an existing owner");
        return 0;
    } catch (const std::exception& failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
