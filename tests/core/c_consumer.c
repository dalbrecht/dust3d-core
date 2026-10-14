#include <Dust3DBridge.h>

int main(void)
{
    Dust3DError error = { 0 };
    Dust3DMesh* mesh = dust3d_mesh_load(NULL, 0, &error);
    if (mesh != NULL || error.status != DUST3D_STATUS_INVALID_DOCUMENT)
        return 1;
    dust3d_mesh_destroy(mesh);
    return 0;
}
