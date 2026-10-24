#include <Dust3DBridge.h>

int main(void)
{
    Dust3DError error = { 0 };
    Dust3DMesh* mesh = dust3d_mesh_load(NULL, 0, &error);
    if (mesh != NULL || error.status != DUST3D_STATUS_INVALID_DOCUMENT)
        return 1;
    dust3d_mesh_destroy(mesh);
    Dust3DIndexedMesh* indexed = dust3d_mesh_create_indexed(NULL, 0, &error);
    if (indexed != NULL || error.status != DUST3D_STATUS_INVALID_DOCUMENT)
        return 2;
    if (dust3d_indexed_mesh_vertices(indexed) != NULL || dust3d_indexed_mesh_indices(indexed) != NULL
        || dust3d_indexed_mesh_vertex_count(indexed) != 0 || dust3d_indexed_mesh_index_count(indexed) != 0)
        return 3;
    dust3d_indexed_mesh_destroy(indexed);
    Dust3DGenerationSession* session = dust3d_generation_session_create(DUST3D_DEFAULT_CACHE_PAYLOAD_BYTES, &error);
    if (session == NULL || error.status != DUST3D_STATUS_OK)
        return 4;
    Dust3DGenerationStats stats = { 0 };
    if (dust3d_generation_session_get_stats(session, &stats, sizeof(stats), &error) != DUST3D_STATUS_OK
        || stats.maximum_cache_payload_bytes != DUST3D_DEFAULT_CACHE_PAYLOAD_BYTES)
        return 5;
    mesh = dust3d_generation_session_load(session, NULL, 0, &error);
    if (mesh != NULL || error.status != DUST3D_STATUS_INVALID_DOCUMENT)
        return 6;
    if (dust3d_generation_session_reset(session, &error) != DUST3D_STATUS_OK)
        return 7;
    dust3d_generation_session_destroy(session);
    Dust3DSourceOptions source_options = { sizeof(Dust3DSourceOptions), 1024 };
    Dust3DSourcedMesh* sourced = dust3d_sourced_mesh_load(NULL, 0, &source_options, &error);
    if (sourced != NULL || error.status != DUST3D_STATUS_INVALID_DOCUMENT
        || dust3d_sourced_mesh_geometry(sourced) != NULL
        || dust3d_sourced_mesh_corners(sourced) != NULL
        || dust3d_sourced_mesh_influences(sourced) != NULL
        || dust3d_sourced_mesh_corner_count(sourced) != 0)
        return 8;
    dust3d_sourced_mesh_destroy(sourced);
    session = dust3d_generation_session_create(0, &error);
    if (session == NULL)
        return 9;
    sourced = dust3d_generation_session_load_sourced(session, NULL, 0, &source_options, &error);
    if (sourced != NULL || error.status != DUST3D_STATUS_INVALID_DOCUMENT)
        return 10;
    dust3d_sourced_mesh_destroy(sourced);
    dust3d_generation_session_destroy(session);
    Dust3DUvOptions uv_options = { sizeof(Dust3DUvOptions), 0, 0, NULL, 0 };
    Dust3DUvMesh* uv_mesh = dust3d_uv_mesh_load(NULL, 0, &uv_options, &error);
    if (uv_mesh != NULL || error.status != DUST3D_STATUS_INVALID_DOCUMENT
        || dust3d_uv_mesh_geometry(NULL) != NULL || dust3d_uv_mesh_triangles(NULL) != NULL
        || dust3d_uv_mesh_charts(NULL) != NULL || dust3d_uv_mesh_gradients(NULL) != NULL
        || dust3d_uv_mesh_triangle_count(NULL) != 0 || dust3d_uv_mesh_payload_bytes(NULL) != 0)
        return 11;
    dust3d_uv_mesh_destroy(uv_mesh);
    dust3d_cancellation_token_cancel(NULL);
    dust3d_cancellation_token_destroy(NULL);
    if (dust3d_cancellation_token_is_cancelled(NULL))
        return 12;
    Dust3DCancellationToken* cancellation = dust3d_cancellation_token_create(&error);
    if (cancellation == NULL || error.status != DUST3D_STATUS_OK
        || dust3d_cancellation_token_is_cancelled(cancellation))
        return 13;
    dust3d_cancellation_token_cancel(cancellation);
    dust3d_cancellation_token_cancel(cancellation);
    if (!dust3d_cancellation_token_is_cancelled(cancellation))
        return 14;
    dust3d_cancellation_token_destroy(cancellation);
    Dust3DBakeOptions bake_options = { 0 };
    bake_options.struct_size = sizeof(bake_options);
    Dust3DBakedMesh* baked = dust3d_baked_mesh_load(NULL, 0, &bake_options, &error);
    Dust3DTextureView texture = dust3d_baked_mesh_texture(NULL, DUST3D_TEXTURE_COLOR);
    if (baked != NULL || error.status != DUST3D_STATUS_INVALID_DOCUMENT
        || texture.pixels != NULL || texture.byte_count != 0
        || dust3d_baked_mesh_uvs(NULL) != NULL || dust3d_baked_mesh_tangents(NULL) != NULL
        || dust3d_baked_mesh_tangent_count(NULL) != 0 || dust3d_baked_mesh_payload_bytes(NULL) != 0)
        return 15;
    dust3d_baked_mesh_destroy(baked);
    Dust3DImportedAssetOptions asset_options = { sizeof(Dust3DImportedAssetOptions), 0 };
    Dust3DImportedAssets* assets = dust3d_imported_assets_load(NULL, 0, &asset_options, &error);
    Dust3DImportedMaterialView material = dust3d_imported_assets_material(NULL, SIZE_MAX);
    Dust3DImportedBindingView binding = dust3d_imported_assets_binding(NULL, SIZE_MAX);
    if (assets != NULL || error.status != DUST3D_STATUS_INVALID_DOCUMENT
        || dust3d_imported_assets_model_count(NULL) != 0 || dust3d_imported_assets_binding_count(NULL) != 0
        || dust3d_imported_assets_payload_bytes(NULL) != 0 || material.model_id != NULL
        || material.image.bytes != NULL || material.image.byte_count != 0 || binding.part_id != NULL)
        return 16;
    dust3d_imported_assets_destroy(assets);
    Dust3DImportedSurfaceOptions surface_options = { sizeof(Dust3DImportedSurfaceOptions), 0 };
    Dust3DImportedSurfaceMesh* surface = dust3d_imported_surface_mesh_load(NULL, 0, &surface_options, &error);
    if (surface != NULL || error.status != DUST3D_STATUS_INVALID_DOCUMENT
        || dust3d_imported_surface_mesh_geometry(NULL) != NULL || dust3d_imported_surface_mesh_triangles(NULL) != NULL
        || dust3d_imported_surface_mesh_triangle_count(NULL) != 0 || dust3d_imported_surface_mesh_model_count(NULL) != 0
        || dust3d_imported_surface_mesh_component_count(NULL) != 0 || dust3d_imported_surface_mesh_payload_bytes(NULL) != 0
        || dust3d_imported_surface_mesh_model_id(NULL, SIZE_MAX) != NULL || dust3d_imported_surface_mesh_component_id(NULL, SIZE_MAX) != NULL
        || dust3d_imported_surface_mesh_component_kind(NULL, SIZE_MAX) != DUST3D_SURFACE_COMPONENT_UNKNOWN)
        return 17;
    dust3d_imported_surface_mesh_destroy(surface);
    if (dust3d_imported_surface_mesh_load(NULL, 0, NULL, NULL) != NULL)
        return 18;
    Dust3DFbxExportOptions fbx_options = { 0 };
    fbx_options.struct_size = sizeof(fbx_options);
    fbx_options.mode = DUST3D_FBX_EXPORT_GEOMETRY;
    Dust3DFbxExport* fbx = dust3d_fbx_export_load(NULL, 0, &fbx_options, &error);
    if (fbx != NULL || error.status != DUST3D_STATUS_INVALID_DOCUMENT
        || dust3d_fbx_export_bytes(NULL) != NULL || dust3d_fbx_export_byte_count(NULL) != 0
        || dust3d_fbx_export_payload_bytes(NULL) != 0 || dust3d_fbx_export_warning(NULL) == NULL)
        return 19;
    dust3d_fbx_export_destroy(fbx);
    return 0;
}
