#include "ComponentPreviewOutput.h"
#include "Cancellation.h"
#include <cmath>
#include <dust3d/base/retained_payload_budget.h>
#include <dust3d/base/string.h>
#include <limits>
#include <set>

namespace dust3d::native {
namespace {
    void validateComponentPreviewHierarchy(const Snapshot& snapshot)
    {
        using Attributes = std::map<std::string, std::string>;
        std::vector<std::pair<const Attributes*, size_t>> pending { { &snapshot.rootComponent, 0 } };
        std::set<std::string> seen;
        std::set<Uuid> generatedIds;
        while (!pending.empty()) {
            const auto [parent, depth] = pending.back();
            pending.pop_back();
            for (const auto& child : String::split(String::valueOrEmpty(*parent, "children"), ',')) {
                const auto found = snapshot.components.find(child);
                if (found == snapshot.components.end() || !seen.insert(child).second)
                    throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "The component hierarchy contains missing or repeated children.");
                const Uuid generatedId(child);
                if (!generatedId.isNull() && !generatedIds.insert(generatedId).second)
                    throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "The component hierarchy contains ambiguous generated identities.");
                if (depth == 128)
                    throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "The component hierarchy exceeds the preview nesting limit.");
                pending.emplace_back(&found->second, depth + 1);
            }
        }
        if (seen.size() != snapshot.components.size())
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "The component hierarchy contains unreachable components.");
    }

    void requireFinite(double value)
    {
        if (!std::isfinite(value))
            throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "The component preview contains nonfinite data.");
    }
}

Uuid resolveComponentPreviewId(const Snapshot& snapshot, std::string_view authoredId)
{
    const std::string key(authoredId);
    const Uuid id(key);
    if (id.isNull() || snapshot.components.find(key) == snapshot.components.end())
        throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "The exact component identity is unavailable.");
    validateComponentPreviewHierarchy(snapshot);
    return id;
}

size_t componentPreviewOutputLimit(size_t requested)
{
    if (requested > 64 * 1024 * 1024)
        throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "The component preview quota exceeds 64 MiB.");
    return requested ? requested : 16 * 1024 * 1024;
}

std::unique_ptr<Dust3DComponentPreview> copyComponentPreview(const MeshGenerator::ComponentPreview& source,
    std::string_view colorImageId, std::string_view warning, size_t maximumBytes, const Dust3DCancellationToken* cancellation)
{
    checkCancellation(cancellation);
    const bool isCutFace = !source.cutFaceTemplate.empty();
    if (!isCutFace && source.triangles.empty())
        throw DocumentError(DUST3D_STATUS_NO_GEOMETRY, "The component generated no preview geometry.");
    RetainedPayloadBudget budget(componentPreviewOutputLimit(maximumBytes));
    budget.tryAdd(sizeof(Dust3DComponentPreview));
    budget.tryAdd(colorImageId.size());
    budget.tryAdd(warning.size());
    budget.tryAdd(2);
    if (isCutFace) {
        budget.tryAdd(source.cutFaceTemplate.size(), sizeof(Dust3DComponentPreviewPoint));
    } else {
        budget.tryAdd(source.vertices.size(), sizeof(Dust3DComponentPreviewVertex));
        budget.tryAdd(source.triangles.size(), sizeof(Dust3DComponentPreviewTriangle));
    }
    if (budget.isExceeded())
        throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "The component preview exceeds its output quota.");

    auto result = std::make_unique<Dust3DComponentPreview>();
    result->colorImageId = colorImageId;
    result->warning = warning;
    if (isCutFace) {
        result->cutFace.reserve(source.cutFaceTemplate.size());
        for (const auto& point : source.cutFaceTemplate) {
            checkCancellation(cancellation);
            requireFinite(point.x());
            requireFinite(point.y());
            result->cutFace.push_back({ point.x(), point.y() });
        }
        return result;
    }

    const bool hasVertexProperties = !source.vertexProperties.empty();
    const bool hasIndexedUvs = !source.triangleCornerUvs.empty();
    if (source.vertices.size() > std::numeric_limits<uint32_t>::max() || (hasVertexProperties && source.vertexProperties.size() != source.vertices.size()) || (hasIndexedUvs && source.triangleCornerUvs.size() != source.triangles.size()))
        throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "The component preview attributes are not aligned with its geometry.");

    result->vertices.reserve(source.vertices.size());
    for (size_t i = 0; i < source.vertices.size(); ++i) {
        if (i % 256 == 0)
            checkCancellation(cancellation);
        const auto& position = source.vertices[i];
        auto [color, metalness, roughness] = hasVertexProperties
            ? source.vertexProperties[i]
            : std::make_tuple(source.color, source.metalness, source.roughness);
        Dust3DComponentPreviewVertex vertex {
            { position.x(), position.y(), position.z() },
            { color.r(), color.g(), color.b(), color.alpha() }, metalness, roughness
        };
        for (const auto coordinate : vertex.position)
            requireFinite(coordinate);
        for (const auto channel : vertex.color)
            requireFinite(channel);
        requireFinite(metalness);
        requireFinite(roughness);
        result->vertices.push_back(vertex);
    }

    result->triangles.reserve(source.triangles.size());
    for (size_t i = 0; i < source.triangles.size(); ++i) {
        if (i % 256 == 0)
            checkCancellation(cancellation);
        const auto& face = source.triangles[i];
        if (face.size() != 3)
            throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "The component preview contains an invalid triangle.");
        Dust3DComponentPreviewTriangle triangle {};
        for (size_t corner = 0; corner < 3; ++corner) {
            if (face[corner] >= source.vertices.size())
                throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "The component preview contains an invalid vertex index.");
            triangle.indices[corner] = static_cast<uint32_t>(face[corner]);
        }
        const std::array<Vector2, 3>* uvs = nullptr;
        if (hasIndexedUvs) {
            if (source.triangleCornerUvs[i])
                uvs = &*source.triangleCornerUvs[i];
        } else if (!source.triangleUvs.empty()) {
            const auto found = source.triangleUvs.find({ PositionKey(source.vertices[face[0]]),
                PositionKey(source.vertices[face[1]]), PositionKey(source.vertices[face[2]]) });
            if (found != source.triangleUvs.end())
                uvs = &found->second;
        }
        if (uvs) {
            triangle.has_uv = 1;
            for (size_t corner = 0; corner < 3; ++corner) {
                const auto& uv = (*uvs)[corner];
                requireFinite(uv.x());
                requireFinite(uv.y());
                triangle.uv[corner][0] = uv.x();
                triangle.uv[corner][1] = uv.y();
            }
        }
        result->triangles.push_back(triangle);
    }
    checkCancellation(cancellation);
    return result;
}
}

void dust3d_component_preview_destroy(Dust3DComponentPreview* preview) { delete preview; }
const Dust3DComponentPreviewVertex* dust3d_component_preview_vertices(const Dust3DComponentPreview* preview) { return preview ? preview->vertices.data() : nullptr; }
size_t dust3d_component_preview_vertex_count(const Dust3DComponentPreview* preview) { return preview ? preview->vertices.size() : 0; }
const Dust3DComponentPreviewTriangle* dust3d_component_preview_triangles(const Dust3DComponentPreview* preview) { return preview ? preview->triangles.data() : nullptr; }
size_t dust3d_component_preview_triangle_count(const Dust3DComponentPreview* preview) { return preview ? preview->triangles.size() : 0; }
const Dust3DComponentPreviewPoint* dust3d_component_preview_cut_face(const Dust3DComponentPreview* preview) { return preview ? preview->cutFace.data() : nullptr; }
size_t dust3d_component_preview_cut_face_count(const Dust3DComponentPreview* preview) { return preview ? preview->cutFace.size() : 0; }
const char* dust3d_component_preview_color_image_id(const Dust3DComponentPreview* preview) { return preview ? preview->colorImageId.c_str() : ""; }
const char* dust3d_component_preview_warning(const Dust3DComponentPreview* preview) { return preview ? preview->warning.c_str() : ""; }
