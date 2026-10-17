#include "EngineInternal.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <dust3d/mesh/mesh_generator.h>
#include <mutex>

namespace {
void setError(Dust3DError* error, Dust3DStatus status, const char* message)
{
    if (!error)
        return;
    error->status = status;
    std::snprintf(error->message, sizeof(error->message), "%s", message);
}

std::unique_ptr<Dust3DMesh> generateMesh(std::unique_ptr<dust3d::Snapshot> snapshot, const std::string& warning)
{
    auto mesh = std::make_unique<Dust3DMesh>();
    mesh->nodeCount = snapshot->nodes.size();
    mesh->partCount = snapshot->parts.size();
    mesh->warning = warning;
    // MeshGenerator owns the raw Snapshot constructor argument.
    dust3d::MeshGenerator generator(snapshot.release());
    generator.generate();
    std::unique_ptr<dust3d::Object> object(generator.takeObject());
    if (!generator.isSuccessful() || !object)
        throw dust3d::native::DocumentError(DUST3D_STATUS_GENERATION_FAILED, "Dust3D could not generate a complete mesh from this document.");
    if (object->triangles.empty())
        throw dust3d::native::DocumentError(DUST3D_STATUS_NO_GEOMETRY, "Generation completed with no mesh-producing geometry.");
    if (object->triangles.size() > 5000000)
        throw dust3d::native::DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "The generated mesh exceeds the five million triangle limit.");
    mesh->vertices.reserve(object->triangles.size() * 3);
    const auto* normals = object->triangleVertexNormals();
    for (size_t triangleIndex = 0; triangleIndex < object->triangles.size(); ++triangleIndex) {
        const auto& triangle = object->triangles[triangleIndex];
        if (triangle.size() != 3)
            throw dust3d::native::DocumentError(DUST3D_STATUS_GENERATION_FAILED, "The engine returned an invalid triangle.");
        std::array<Dust3DVertex, 3> exportedTriangle {};
        for (size_t corner = 0; corner < 3; ++corner) {
            const auto vertexIndex = triangle[corner];
            if (vertexIndex >= object->vertices.size())
                throw dust3d::native::DocumentError(DUST3D_STATUS_GENERATION_FAILED, "The engine returned an invalid vertex index.");
            const auto& position = object->vertices[vertexIndex];
            auto normal = normals && triangleIndex < normals->size() && (*normals)[triangleIndex].size() == 3
                ? (*normals)[triangleIndex][corner]
                : dust3d::Vector3(0, 1, 0);
            if (normal.length() < 0.000001)
                normal = dust3d::Vector3(0, 1, 0);
            normal.normalize();
            const auto color = vertexIndex < object->vertexColors.size() ? object->vertexColors[vertexIndex] : dust3d::Color(0.65, 0.7, 0.8);
            Dust3DVertex vertex {};
            for (size_t axis = 0; axis < 3; ++axis) {
                if (!std::isfinite(position[axis]) || !std::isfinite(normal[axis]))
                    throw dust3d::native::DocumentError(DUST3D_STATUS_GENERATION_FAILED, "The engine produced nonfinite geometry.");
                vertex.position[axis] = static_cast<float>(position[axis]);
                vertex.normal[axis] = static_cast<float>(normal[axis]);
                if (!std::isfinite(vertex.position[axis]))
                    throw dust3d::native::DocumentError(DUST3D_STATUS_GENERATION_FAILED, "The geometry exceeds the renderer's coordinate range.");
            }
            vertex.color[0] = static_cast<float>(color.r());
            vertex.color[1] = static_cast<float>(color.g());
            vertex.color[2] = static_cast<float>(color.b());
            vertex.color[3] = 1.0f; // Opaque preview; material transparency is a later feature.
            exportedTriangle[corner] = vertex;
        }
        auto exportedPosition = [](const Dust3DVertex& vertex) {
            return dust3d::Vector3(vertex.position[0], vertex.position[1], vertex.position[2]);
        };
        const auto first = exportedPosition(exportedTriangle[0]);
        const auto second = exportedPosition(exportedTriangle[1]);
        const auto third = exportedPosition(exportedTriangle[2]);
        // Double-precision surface slivers can collapse only when converted to the
        // ABI's float coordinates. Validate first so invalid geometry is never hidden.
        if (dust3d::Vector3::crossProduct(second - first, third - first).lengthSquared() == 0)
            continue;
        mesh->vertices.insert(mesh->vertices.end(), exportedTriangle.begin(), exportedTriangle.end());
    }
    if (mesh->vertices.empty())
        throw dust3d::native::DocumentError(DUST3D_STATUS_GENERATION_FAILED, "All generated triangles collapse at the exported coordinate precision.");
    return mesh;
}
}

Dust3DMesh* dust3d_mesh_load(const uint8_t* bytes, size_t size, Dust3DError* error)
{
    setError(error, DUST3D_STATUS_OK, "");
    try {
        static std::mutex engineMutex;
        std::lock_guard<std::mutex> lock(engineMutex);
        std::string warning;
        auto snapshot = dust3d::native::readDocument(bytes, size, warning);
        return generateMesh(std::move(snapshot), warning).release();
    } catch (const dust3d::native::DocumentError& exception) {
        setError(error, exception.status(), exception.what());
    } catch (const std::bad_alloc&) {
        setError(error, DUST3D_STATUS_LIMIT_EXCEEDED, "Not enough memory to open this document.");
    } catch (const std::exception& exception) {
        setError(error, DUST3D_STATUS_INVALID_DOCUMENT, exception.what());
    } catch (...) {
        setError(error, DUST3D_STATUS_GENERATION_FAILED, "An unknown error occurred in the modeling engine.");
    }
    return nullptr;
}

void dust3d_mesh_destroy(Dust3DMesh* mesh) { delete mesh; }
const Dust3DVertex* dust3d_mesh_vertices(const Dust3DMesh* mesh) { return mesh ? mesh->vertices.data() : nullptr; }
size_t dust3d_mesh_vertex_count(const Dust3DMesh* mesh) { return mesh ? mesh->vertices.size() : 0; }
size_t dust3d_mesh_node_count(const Dust3DMesh* mesh) { return mesh ? mesh->nodeCount : 0; }
size_t dust3d_mesh_part_count(const Dust3DMesh* mesh) { return mesh ? mesh->partCount : 0; }
const char* dust3d_mesh_warning(const Dust3DMesh* mesh) { return mesh ? mesh->warning.c_str() : ""; }
