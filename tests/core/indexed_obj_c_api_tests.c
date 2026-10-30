/* SPDX-License-Identifier: MIT */
#include <Dust3DBridge.h>
int main(void)
{
    Dust3DObjExportOptions options = { 0 };
    Dust3DError error = { 0 };
    options.struct_size = sizeof(options);
    if (dust3d_indexed_mesh_export_obj(NULL, &options, &error) != NULL)
        return 1;
    return error.status == DUST3D_STATUS_INVALID_DOCUMENT ? 0 : 1;
}
