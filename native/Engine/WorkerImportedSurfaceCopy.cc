// SPDX-License-Identifier: MIT
#include "WorkerImportedSurfaceCopy.h"
#include "WorkerImportedSurfaceValidation.h"
#include "WorkerMeshCopyInspection.h"
#include "WorkerOperation.h"
namespace dust3d {
namespace worker {
    namespace {
        [[noreturn]] void reject(ImportedSurfaceErrorCode code, const char* cause) { throw ImportedSurfaceError(code, cause); }
        void charge(uint64_t& total, uint64_t count, uint64_t width, uint64_t maximum)
        {
            if (total > maximum || count > (maximum - total) / width)
                reject(ImportedSurfaceErrorCode::limitExceeded, "Native imported surface exceeds its configured quota.");
            total += count * width;
        }
        std::string_view key(const char* bytes)
        {
            if (!bytes)
                reject(ImportedSurfaceErrorCode::invalidIdentity, "Native imported identity storage is missing.");
            size_t size = 0;
            while (size <= 38 && bytes[size])
                ++size;
            if (!size || size > 38)
                reject(ImportedSurfaceErrorCode::invalidIdentity, "Native imported identity length is invalid.");
            return { bytes, size };
        }
        ImportedSurfaceTriangle triangle(const Dust3DImportedSurfaceTriangle& native)
        {
            ImportedSurfaceTriangle value;
            for (size_t corner = 0; corner < 3; ++corner) {
                for (size_t axis = 0; axis < 2; ++axis)
                    value.uv[corner][axis] = native.uv[corner][axis];
                for (size_t channel = 0; channel < 4; ++channel)
                    value.colors[corner][channel] = native.color[corner][channel];
            }
            value.uvKnown = native.uv_known;
            value.colorEncoding = static_cast<uint32_t>(native.color_encoding);
            value.modelIndex = native.model_index == SIZE_MAX ? noImportedIdentity : native.model_index;
            value.componentIndex = native.component_index == SIZE_MAX ? noImportedIdentity : native.component_index;
            return value;
        }
    }
    ImportedSurfacePayload copyNativeImportedSurface(const Dust3DImportedSurfaceMesh* owner, const ImportedSurfaceLimits& limits)
    {
        validateImportedSurfaceLimits(limits);
        if (!owner)
            reject(ImportedSurfaceErrorCode::invalidInput, "Native imported surface owner is missing.");
        const auto* mesh = dust3d_imported_surface_mesh_geometry(owner);
        const auto geometry = detail::inspectNativeMeshCopy(mesh, limits.geometry);
        const auto count = dust3d_imported_surface_mesh_triangle_count(owner);
        const auto models = dust3d_imported_surface_mesh_model_count(owner);
        const auto components = dust3d_imported_surface_mesh_component_count(owner);
        (void)detail::measureImportedSurface(count, models, components, 0, geometry.vertexCount / 3, limits.surface);
        const auto payload = dust3d_imported_surface_mesh_payload_bytes(owner);
        if (!payload || payload > maximumPayloadBytes)
            reject(ImportedSurfaceErrorCode::invalidLayout, "Native imported surface payload is outside supported bounds.");
        uint64_t minimum = 0;
        charge(minimum, geometry.vertexCount, sizeof(Dust3DVertex), payload);
        charge(minimum, count, sizeof(Dust3DImportedSurfaceTriangle), payload);
        charge(minimum, models, sizeof(const char*), payload);
        charge(minimum, components, sizeof(const char*), payload);
        const auto* records = dust3d_imported_surface_mesh_triangles(owner);
        if (!records)
            reject(ImportedSurfaceErrorCode::invalidLayout, "Native imported surface records are missing.");
        uint64_t text = 0;
        std::unordered_set<std::string_view> keys;
        for (size_t index = 0; index < models; ++index) {
            const auto id = key(dust3d_imported_surface_mesh_model_id(owner, index));
            detail::validateImportedKey(id, keys);
            charge(text, id.size(), 1, limits.surface.maximumTextBytes);
        }
        keys.clear();
        for (size_t index = 0; index < components; ++index) {
            const auto id = key(dust3d_imported_surface_mesh_component_id(owner, index));
            detail::validateImportedKey(id, keys);
            detail::validateImportedKind(static_cast<uint32_t>(dust3d_imported_surface_mesh_component_kind(owner, index)));
            charge(text, id.size(), 1, limits.surface.maximumTextBytes);
        }
        for (size_t index = 0; index < count; ++index)
            detail::validateImportedTriangle(triangle(records[index]), models, components);
        const auto metadata = detail::measureImportedSurface(count, models, components, text, geometry.vertexCount / 3, limits.surface);
        uint64_t wire = importedSurfacePrefixBytes, logical = 0;
        charge(wire, geometry.wire, 1, limits.maximumWireBytes);
        charge(wire, metadata.wire, 1, limits.maximumWireBytes);
        charge(logical, geometry.logical, 1, limits.maximumDecodedBytes);
        charge(logical, metadata.logical, 1, limits.maximumDecodedBytes);
        ImportedSurfacePayload value;
        value.geometry = copyNativeMesh(mesh, limits.geometry);
        value.triangles.reserve(count);
        value.models.reserve(models);
        value.components.reserve(components);
        for (size_t index = 0; index < count; ++index)
            value.triangles.push_back(triangle(records[index]));
        for (size_t index = 0; index < models; ++index)
            value.models.emplace_back(key(dust3d_imported_surface_mesh_model_id(owner, index)));
        for (size_t index = 0; index < components; ++index)
            value.components.push_back({ std::string(key(dust3d_imported_surface_mesh_component_id(owner, index))), static_cast<uint32_t>(dust3d_imported_surface_mesh_component_kind(owner, index)) });
        return value;
    }
}
}
