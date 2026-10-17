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
    return 0;
}
