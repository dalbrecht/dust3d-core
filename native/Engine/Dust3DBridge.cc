#include "BakedMesh.h"
#include "FbxGeometrySelection.h"
#include "GenerationPipeline.h"
#include "ImportedModels.h"
#include "ImportedSurface.h"
#include "SourceBakedMesh.h"
#include "SourceProvenance.h"
#include "UvMetadata.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <dust3d/mesh/mesh_generator.h>
#include <mutex>
#include <numeric>

namespace dust3d {
namespace native {
    void setError(Dust3DError* error, Dust3DStatus status, const char* message) noexcept
    {
        if (!error)
            return;
        error->status = status;
        std::snprintf(error->message, sizeof(error->message), "%s", message);
    }
}
}

namespace dust3d {
namespace native {
    std::mutex& engineMutex()
    {
        static std::mutex mutex;
        return mutex;
    }

    Color sourceCornerColor(const Object& object, size_t triangleIndex, size_t corner)
    {
        const auto vertexIndex = object.triangles.at(triangleIndex).at(corner);
        if (triangleIndex < object.triangleVertexColors.size() && object.triangleVertexColors[triangleIndex])
            return object.triangleVertexColors[triangleIndex]->at(corner);
        return vertexIndex < object.vertexColors.size() ? object.vertexColors[vertexIndex] : Color(0.65, 0.7, 0.8);
    }

    std::unique_ptr<Dust3DMesh> generateMesh(std::unique_ptr<dust3d::Snapshot> snapshot, const std::string& warning, ImportedModels* imports,
        MeshGenerator::GeneratedCacheContext* cache, MeshGenerator::GenerationMetrics* metrics, GenerationOutputs outputs)
    {
        if (outputs.fbxGeometry)
            planFbxGeometrySelection(1, 1, 1, false, false, outputs.maximumFbxGeometryBytes);
        auto* sources = outputs.sources;
        auto* uvs = outputs.uvs;
        auto* baked = outputs.baked;
        auto* surfaces = outputs.surfaces;
        auto* sourceBaked = outputs.sourceBaked;
        auto checkGenerationCancellation = [&] {
            if (baked)
                checkCancellation(baked->cancellation());
            if (sourceBaked)
                checkCancellation(sourceBaked->cancellation());
        };
        const std::map<Uuid, UvAtlasImage> emptyImages;
        const auto& images = outputs.images ? *outputs.images : emptyImages;
        const size_t textureSize = outputs.textureSize;
        auto mesh = std::make_unique<Dust3DMesh>();
        mesh->nodeCount = snapshot->nodes.size();
        mesh->partCount = snapshot->parts.size();
        mesh->warning = warning;
        // MeshGenerator owns the raw Snapshot constructor argument.
        dust3d::MeshGenerator generator(snapshot.get());
        snapshot.release();
        if (imports)
            generator.setImportedModelData(imports->takeGeometry());
        if (cache)
            generator.setGeneratedCacheContext(cache);
        checkGenerationCancellation();
        generator.generate();
        checkGenerationCancellation();
        if (metrics)
            *metrics = generator.generationMetrics();
        std::unique_ptr<dust3d::Object> object(generator.takeObject());
        if (!generator.isSuccessful() || !object)
            throw dust3d::native::DocumentError(DUST3D_STATUS_GENERATION_FAILED, "Dust3D could not generate a complete mesh from this document.");
        if (object->triangles.empty())
            throw dust3d::native::DocumentError(DUST3D_STATUS_NO_GEOMETRY, "Generation completed with no mesh-producing geometry.");
        if (object->triangles.size() > 5000000)
            throw dust3d::native::DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "The generated mesh exceeds the five million triangle limit.");
        if (surfaces)
            surfaces->prepare(*object, mesh->warning);
        std::unique_ptr<Snapshot> generated;
        if (uvs || sourceBaked) {
            generated.reset(generator.takeSnapshot());
            if (!generated)
                throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "The generator returned no UV material snapshot.");
            if (uvs)
                uvs->prepare(*object, *generated, images, textureSize);
        }
        mesh->vertices.reserve(object->triangles.size() * 3);
        const auto* normals = object->triangleVertexNormals();
        for (size_t triangleIndex = 0; triangleIndex < object->triangles.size(); ++triangleIndex) {
            if (triangleIndex % 256 == 0)
                checkGenerationCancellation();
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
                const auto color = sourceCornerColor(*object, triangleIndex, corner);
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
            if (surfaces)
                surfaces->appendTriangle(triangleIndex);
            if (uvs)
                uvs->appendTriangle(triangleIndex);
            if (baked)
                baked->appendTriangle(triangleIndex);
            if (sourceBaked)
                sourceBaked->appendTriangle(triangleIndex);
            if (sources) {
                for (const auto vertexIndex : triangle)
                    sources->appendCorner(*object, object->vertices[vertexIndex], triangleIndex);
            }
            mesh->vertices.insert(mesh->vertices.end(), exportedTriangle.begin(), exportedTriangle.end());
        }
        if (mesh->vertices.empty())
            throw dust3d::native::DocumentError(DUST3D_STATUS_GENERATION_FAILED, "All generated triangles collapse at the exported coordinate precision.");
        if (baked) {
            if (!uvs || !generated)
                throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "Baking requires locally retained UV generation metadata.");
            baked->prepare(*object, *generated, uvs->atlas());
        }
        if (surfaces)
            surfaces->finish(*mesh);
        if (sourceBaked) {
            if (!surfaces || !generated)
                throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "Source baking requires retained surface and generated metadata.");
            sourceBaked->prepare(*object, *generated, *mesh, *surfaces);
        }
        if (outputs.fbxGeometry) {
            planFbxGeometrySelection(object->vertices.size(), object->triangles.size(), object->triangles.size(),
                object->triangleVertexNormals() != nullptr, object->triangleVertexUvs() != nullptr, outputs.maximumFbxGeometryBytes);
            std::vector<size_t> originalFaces(object->triangles.size());
            std::iota(originalFaces.begin(), originalFaces.end(), size_t { 0 });
            auto geometry = std::make_unique<FbxGeometrySelection>(selectFbxGeometry(*object, originalFaces, nullptr, outputs.maximumFbxGeometryBytes));
            checkGenerationCancellation();
            *outputs.fbxGeometry = std::move(geometry);
        }
        return mesh;
    }
}
}

Dust3DMesh* dust3d_mesh_load(const uint8_t* bytes, size_t size, Dust3DError* error)
{
    dust3d::native::setError(error, DUST3D_STATUS_OK, "");
    try {
        std::lock_guard<std::mutex> lock(dust3d::native::engineMutex());
        std::string warning;
        dust3d::native::ImportedModels imports;
        auto snapshot = dust3d::native::readDocument(bytes, size, warning, nullptr, &imports);
        return dust3d::native::generateMesh(std::move(snapshot), warning, &imports).release();
    } catch (const dust3d::native::DocumentError& exception) {
        dust3d::native::setError(error, exception.status(), exception.what());
    } catch (const std::bad_alloc&) {
        dust3d::native::setError(error, DUST3D_STATUS_LIMIT_EXCEEDED, "Not enough memory to open this document.");
    } catch (const std::exception& exception) {
        dust3d::native::setError(error, DUST3D_STATUS_INVALID_DOCUMENT, exception.what());
    } catch (...) {
        dust3d::native::setError(error, DUST3D_STATUS_GENERATION_FAILED, "An unknown error occurred in the modeling engine.");
    }
    return nullptr;
}

void dust3d_mesh_destroy(Dust3DMesh* mesh) { delete mesh; }
const Dust3DVertex* dust3d_mesh_vertices(const Dust3DMesh* mesh) { return mesh ? mesh->vertices.data() : nullptr; }
size_t dust3d_mesh_vertex_count(const Dust3DMesh* mesh) { return mesh ? mesh->vertices.size() : 0; }
size_t dust3d_mesh_node_count(const Dust3DMesh* mesh) { return mesh ? mesh->nodeCount : 0; }
size_t dust3d_mesh_part_count(const Dust3DMesh* mesh) { return mesh ? mesh->partCount : 0; }
const char* dust3d_mesh_warning(const Dust3DMesh* mesh) { return mesh ? mesh->warning.c_str() : ""; }
