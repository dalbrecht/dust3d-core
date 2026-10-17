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
    return 0;
}
