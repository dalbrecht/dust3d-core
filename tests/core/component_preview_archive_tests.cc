// SPDX-License-Identifier: MIT
#include "Dust3DBridge.h"
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

std::vector<uint8_t> readArchive(const char* path)
{
    std::ifstream file(path, std::ios::binary);
    require(file.is_open(), "Could not open the archive fixture");
    const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), {});
    require(!bytes.empty(), "The archive fixture is empty");
    return bytes;
}

std::string disabledCapsuleArchive(const std::vector<uint8_t>& capsule, const std::string& groupId)
{
    const std::string archive(capsule.begin(), capsule.end());
    const auto canvas = archive.find("<canvas");
    require(canvas != std::string::npos, "Capsule model XML is missing");
    auto xml = archive.substr(canvas);
    const std::string enabled = "disabled=\"false\"";
    const auto disabled = xml.find(enabled);
    require(disabled != std::string::npos, "Capsule disabled attribute is missing");
    xml.replace(disabled, enabled.size(), "disabled=\"true\"");
    const std::string opening = "<components>";
    const auto start = xml.find(opening);
    const auto end = xml.find("</components>");
    require(start != std::string::npos && end != std::string::npos && start < end, "Capsule components are missing");
    xml.insert(end, "</component>");
    xml.insert(start + opening.size(), "<component id='" + groupId + "'>");
    const std::string prefix = "DUST3D 1.0 xml ";
    const auto header = "<ds3><model name='model.xml' offset='0' size='" + std::to_string(xml.size()) + "'/></ds3>";
    std::ostringstream output;
    output << prefix << std::setfill('0') << std::setw(10) << prefix.size() + 12 + header.size() << "\r\n"
           << header << xml;
    return output.str();
}
}

int main(int argc, char** argv)
{
    try {
        require(argc == 3, "Expected the checked-in Capsule and ImportedSurface archive paths");
        const auto archive = readArchive(argv[1]);

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

        const std::string groupId = "dddddddd-dddd-4ddd-8ddd-dddddddddddd";
        const auto disabledArchive = disabledCapsuleArchive(archive, groupId);
        const Dust3DComponentPreviewID disabledRequests[] { { groupId.data(), groupId.size() }, request };
        const Batch disabled(dust3d_component_preview_batch_load(
                                 reinterpret_cast<const uint8_t*>(disabledArchive.data()), disabledArchive.size(), disabledRequests, 2, 0, nullptr, &error),
            dust3d_component_preview_batch_destroy);
        require(disabled && error.status == DUST3D_STATUS_OK, error.message);
        require(dust3d_component_preview_batch_count(disabled.get()) == 2, "Disabled preview batch lost a requested slot");
        for (size_t slot = 0; slot < 2; ++slot)
            require(dust3d_component_preview_triangle_count(dust3d_component_preview_batch_at(disabled.get(), slot)) > 0,
                "The disabled leaf and its bare-ID parent must both produce previews");

        const auto importedArchive = readArchive(argv[2]);
        constexpr char importedId[] = "{00000200-0000-4000-8000-000000000200}";
        const Dust3DComponentPreviewID importedRequest { importedId, sizeof(importedId) - 1 };
        const Batch imported(dust3d_component_preview_batch_load(importedArchive.data(), importedArchive.size(), &importedRequest, 1, 0, nullptr, &error),
            dust3d_component_preview_batch_destroy);
        require(imported && error.status == DUST3D_STATUS_OK, error.message);
        const auto* importedPreview = dust3d_component_preview_batch_at(imported.get(), 0);
        const auto importedTriangleCount = dust3d_component_preview_triangle_count(importedPreview);
        const auto* importedTriangles = dust3d_component_preview_triangles(importedPreview);
        require(importedTriangles && importedTriangleCount > 0, "The imported model must produce a mesh thumbnail");
        for (size_t index = 0; index < importedTriangleCount; ++index) {
            require(importedTriangles[index].has_uv == 1, "The imported model must preserve its indexed corner UVs");
            for (const auto& uv : importedTriangles[index].uv)
                require(std::isfinite(uv[0]) && std::isfinite(uv[1]), "The imported corner UVs must be finite");
        }
        return 0;
    } catch (const std::exception& failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
