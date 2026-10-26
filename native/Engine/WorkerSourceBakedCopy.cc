// SPDX-License-Identifier: MIT
#include "WorkerSourceBakedCopy.h"
#include "WorkerBakedTextureViews.h"
#include "WorkerMeshCopyInspection.h"
#include "WorkerOperation.h"
#include "WorkerSourceBakedValidation.h"
namespace dust3d {
namespace worker {
    namespace {
        [[noreturn]] void reject(SourceBakedErrorCode code, const char* cause) { throw SourceBakedError(code, cause); }
        void charge(uint64_t& total, uint64_t count, uint64_t unit, uint64_t maximum)
        {
            if (total > maximum || count > (maximum - total) / unit)
                reject(SourceBakedErrorCode::limitExceeded, "Native source-baked copy exceeds its aggregate quota.");
            total += count * unit;
        }
        std::string_view key(const char* text)
        {
            if (!text)
                reject(SourceBakedErrorCode::invalidIdentity, "Native source-baked catalog key storage is missing.");
            size_t count = 0;
            while (count <= 38 && text[count])
                ++count;
            if (count > 38)
                reject(SourceBakedErrorCode::invalidIdentity, "Native source-baked catalog key exceeds its readable bound.");
            return { text, count };
        }
        BakedSurfaceTriangle triangle(const Dust3DSourceBakedTriangle& value)
        {
            BakedSurfaceTriangle result;
            for (size_t corner = 0; corner < 3; ++corner)
                for (size_t axis = 0; axis < 2; ++axis)
                    result.uv[corner][axis] = value.uv[corner][axis];
            for (size_t axis = 0; axis < 3; ++axis)
                result.tangent[axis] = value.tangent.direction[axis];
            result.handedness = value.tangent.handedness;
            result.alphaCutoff = value.alpha_cutoff;
            result.modelIndex = value.model_index == SIZE_MAX ? noBakedIdentity : value.model_index;
            result.componentIndex = value.component_index == SIZE_MAX ? noBakedIdentity : value.component_index;
            result.uvKnown = value.uv_known;
            result.policyKnown = value.policy_known;
            result.isDoubleSided = value.is_double_sided;
            result.tangentKnown = value.tangent.known;
            result.alphaMode = static_cast<uint32_t>(value.alpha_mode);
            return result;
        }
        struct GeometryShape {
            uint64_t wire, logical;
        };
        GeometryShape inspectGeometry(const Dust3DMesh* geometry, uint64_t triangles, const MeshPayloadLimits& limits)
        {
            const auto count = dust3d_mesh_vertex_count(geometry);
            if (!dust3d_mesh_vertices(geometry) || !count || count % 3 || count / 3 != triangles)
                reject(SourceBakedErrorCode::countMismatch, "Native source-baked geometry/triangle correspondence is invalid.");
            try {
                const auto shape = detail::inspectNativeMeshCopy(geometry, limits);
                return { shape.wire, shape.logical };
            } catch (const MeshPayloadError& cause) {
                if (cause.code() == MeshPayloadErrorCode::limitExceeded)
                    reject(SourceBakedErrorCode::limitExceeded, cause.what());
                throw;
            }
        }
        detail::BakedTextureViews textureViews(const Dust3DSourceBakedMesh* owner)
        {
            detail::BakedTextureViews views;
            views.maximumSamplingLod = dust3d_source_baked_mesh_maximum_sampling_lod(owner);
            for (size_t role = 0; role < bakedTextureRoleCount; ++role) {
                const auto count = dust3d_source_baked_mesh_texture_level_count(owner, static_cast<uint32_t>(role));
                if (count > maximumTextureLevels)
                    throw BakedTexturesError(BakedTexturesErrorCode::invalidLayout, "Native source-baked texture chain exceeds fourteen levels.");
                views.counts[role] = static_cast<uint32_t>(count);
                if (!count) {
                    const auto absent = dust3d_source_baked_mesh_texture(owner, static_cast<uint32_t>(role));
                    if (absent.pixels || absent.width || absent.height || absent.row_stride || absent.byte_count)
                        throw BakedTexturesError(BakedTexturesErrorCode::invalidImage, "Native source-baked absent texture has nonempty storage.");
                }
                for (size_t level = 0; level < count; ++level) {
                    const auto view = dust3d_source_baked_mesh_texture_level(owner, static_cast<uint32_t>(role), level);
                    if (!view.width || view.width > 8192 || !view.height || view.height > 8192 || view.row_stride != view.width * 4)
                        throw BakedTexturesError(BakedTexturesErrorCode::invalidImage, "Native source-baked texture dimensions or tight row stride are invalid.");
                    views.maps[role][level] = { static_cast<uint32_t>(view.width), static_cast<uint32_t>(view.height), view.byte_count, view.pixels };
                }
            }
            return views;
        }
    }
    SourceBakedPayload copyNativeSourceBakedMesh(const Dust3DSourceBakedMesh* owner, const SourceBakedLimits& limits)
    {
        validateSourceBakedLimits(limits);
        if (!owner)
            reject(SourceBakedErrorCode::invalidInput, "Native source-baked owner is missing.");
        const auto* geometry = dust3d_source_baked_mesh_geometry(owner);
        const auto* records = dust3d_source_baked_mesh_triangles(owner);
        const auto triangles = dust3d_source_baked_mesh_triangle_count(owner), models = dust3d_source_baked_mesh_model_count(owner), components = dust3d_source_baked_mesh_component_count(owner);
        (void)detail::measureBakedSurfaceCounts(triangles, models, components, 0, triangles, limits.surface);
        if (!geometry || !records)
            reject(SourceBakedErrorCode::invalidLayout, "Native source-baked geometry/record storage is missing.");
        const auto payload = dust3d_source_baked_mesh_payload_bytes(owner);
        if (!payload || payload > maximumPayloadBytes)
            reject(SourceBakedErrorCode::invalidLayout, "Native source-baked retained payload is outside supported bounds.");
        uint64_t minimum = 0;
        charge(minimum, dust3d_mesh_vertex_count(geometry), sizeof(Dust3DVertex), payload);
        charge(minimum, triangles, sizeof(Dust3DSourceBakedTriangle), payload);
        charge(minimum, models, sizeof(const char*), payload);
        charge(minimum, components, sizeof(const char*), payload);
        const auto mesh = inspectGeometry(geometry, triangles, limits.geometry);
        uint64_t text = 0;
        std::unordered_set<std::string_view> keys;
        for (size_t index = 0; index < models; ++index) {
            const auto identity = key(dust3d_source_baked_mesh_model_id(owner, index));
            detail::validateBakedCatalogKey(identity, keys);
            charge(text, identity.size(), 1, limits.surface.maximumTextBytes);
        }
        keys.clear();
        for (size_t index = 0; index < components; ++index) {
            const auto identity = key(dust3d_source_baked_mesh_component_id(owner, index));
            detail::validateBakedCatalogKey(identity, keys);
            detail::validateBakedComponentKind(static_cast<uint32_t>(dust3d_source_baked_mesh_component_kind(owner, index)));
            charge(text, identity.size(), 1, limits.surface.maximumTextBytes);
        }
        const auto surface = detail::measureBakedSurfaceCounts(triangles, models, components, text, triangles, limits.surface);
        for (size_t index = 0; index < triangles; ++index)
            detail::validateBakedSurfaceTriangle(triangle(records[index]), models, components);
        const auto views = textureViews(owner);
        const auto maps = detail::inspectBakedTextureViews(views, limits.textures);
        uint64_t wire = sourceBakedPrefixBytes, logical = 0;
        charge(wire, mesh.wire, 1, limits.maximumWireBytes);
        charge(wire, surface.wireBytes, 1, limits.maximumWireBytes);
        charge(wire, maps.wireBytes, 1, limits.maximumWireBytes);
        charge(logical, mesh.logical, 1, limits.maximumDecodedBytes);
        charge(logical, surface.logicalBytes, 1, limits.maximumDecodedBytes);
        charge(logical, maps.logicalBytes, 1, limits.maximumDecodedBytes);
        SourceBakedPayload result;
        result.geometry = copyNativeMesh(geometry, limits.geometry);
        result.surface.triangles.reserve(triangles);
        result.surface.models.reserve(models);
        result.surface.components.reserve(components);
        for (size_t index = 0; index < triangles; ++index)
            result.surface.triangles.push_back(triangle(records[index]));
        for (size_t index = 0; index < models; ++index)
            result.surface.models.emplace_back(key(dust3d_source_baked_mesh_model_id(owner, index)));
        for (size_t index = 0; index < components; ++index)
            result.surface.components.push_back({ std::string(key(dust3d_source_baked_mesh_component_id(owner, index))), static_cast<uint32_t>(dust3d_source_baked_mesh_component_kind(owner, index)) });
        result.textures = detail::copyBakedTextureViews(views, limits.textures);
        return result;
    }
}
}
