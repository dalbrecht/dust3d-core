#include "EngineInternal.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>

namespace dust3d {
namespace native {

    std::unique_ptr<Dust3DIndexedMesh> buildIndexedMesh(const Dust3DMesh& source, size_t maximumBytes)
    {
        constexpr size_t defaultMaximumPayloadBytes = 256 * 1024 * 1024;
        const size_t payloadBudgetBytes = maximumBytes == 0 ? defaultMaximumPayloadBytes : maximumBytes;
        const size_t expandedVertexCount = source.vertices.size();
        if (expandedVertexCount == 0 || expandedVertexCount % 3 != 0)
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "The source mesh must contain complete nonempty triangles.");
        if (expandedVertexCount > std::numeric_limits<uint32_t>::max())
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "The source mesh exceeds UInt32 index limits.");
        if (expandedVertexCount > payloadBudgetBytes / sizeof(uint32_t))
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "The indexed triangle buffer exceeds its payload budget.");
        const size_t indexPayloadBytes = expandedVertexCount * sizeof(uint32_t);
        const size_t maximumVertexCount = (payloadBudgetBytes - indexPayloadBytes) / sizeof(Dust3DVertex);
        auto indexed = std::make_unique<Dust3DIndexedMesh>();
        indexed->nodeCount = source.nodeCount;
        indexed->partCount = source.partCount;
        indexed->warning = source.warning;
        indexed->indices.reserve(expandedVertexCount);
        indexed->vertices.reserve(std::min({ expandedVertexCount, maximumVertexCount, size_t { 4096 } }));
        std::map<std::array<float, 10>, uint32_t> vertexIndicesByAttributes;
        for (const auto& vertex : source.vertices) {
            std::array<float, 10> attributes;
            std::copy_n(vertex.position, 3, attributes.begin());
            std::copy_n(vertex.normal, 3, attributes.begin() + 3);
            std::copy_n(vertex.color, 4, attributes.begin() + 6);
            for (const auto value : attributes) {
                if (!std::isfinite(value))
                    throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "The source mesh contains nonfinite vertex attributes.");
            }
            auto foundVertexIndex = vertexIndicesByAttributes.find(attributes);
            if (foundVertexIndex != vertexIndicesByAttributes.end()) {
                indexed->indices.push_back(foundVertexIndex->second);
                continue;
            }
            if (indexed->vertices.size() >= maximumVertexCount)
                throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "The indexed vertex buffer exceeds its payload budget.");
            // Each unique vertex precedes a source element; the checked source count
            // therefore bounds this conversion even when no attributes are shared.
            const auto vertexIndex = static_cast<uint32_t>(indexed->vertices.size());
            indexed->vertices.push_back(vertex);
            vertexIndicesByAttributes.emplace(attributes, vertexIndex);
            indexed->indices.push_back(vertexIndex);
        }
        return indexed;
    }
}
}

Dust3DIndexedMesh* dust3d_mesh_create_indexed(const Dust3DMesh* mesh, size_t maximum_bytes, Dust3DError* error)
{
    dust3d::native::setError(error, DUST3D_STATUS_OK, "");
    try {
        if (!mesh)
            throw dust3d::native::DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "An indexed copy requires a source mesh owner.");
        return dust3d::native::buildIndexedMesh(*mesh, maximum_bytes).release();
    } catch (const dust3d::native::DocumentError& exception) {
        dust3d::native::setError(error, exception.status(), exception.what());
    } catch (const std::bad_alloc&) {
        dust3d::native::setError(error, DUST3D_STATUS_LIMIT_EXCEEDED, "Not enough memory to create indexed mesh buffers.");
    } catch (const std::exception& exception) {
        dust3d::native::setError(error, DUST3D_STATUS_GENERATION_FAILED, exception.what());
    } catch (...) {
        dust3d::native::setError(error, DUST3D_STATUS_GENERATION_FAILED, "An unknown error occurred while indexing mesh buffers.");
    }
    return nullptr;
}

void dust3d_indexed_mesh_destroy(Dust3DIndexedMesh* mesh) { delete mesh; }
const Dust3DVertex* dust3d_indexed_mesh_vertices(const Dust3DIndexedMesh* mesh) { return mesh ? mesh->vertices.data() : nullptr; }
size_t dust3d_indexed_mesh_vertex_count(const Dust3DIndexedMesh* mesh) { return mesh ? mesh->vertices.size() : 0; }
const uint32_t* dust3d_indexed_mesh_indices(const Dust3DIndexedMesh* mesh) { return mesh ? mesh->indices.data() : nullptr; }
size_t dust3d_indexed_mesh_index_count(const Dust3DIndexedMesh* mesh) { return mesh ? mesh->indices.size() : 0; }
size_t dust3d_indexed_mesh_node_count(const Dust3DIndexedMesh* mesh) { return mesh ? mesh->nodeCount : 0; }
size_t dust3d_indexed_mesh_part_count(const Dust3DIndexedMesh* mesh) { return mesh ? mesh->partCount : 0; }
const char* dust3d_indexed_mesh_warning(const Dust3DIndexedMesh* mesh) { return mesh ? mesh->warning.c_str() : ""; }
