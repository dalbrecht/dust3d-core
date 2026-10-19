// SPDX-License-Identifier: MIT
#include <Dust3DBridge.h>
int main(void)
{
    Dust3DSourceBakeOptions options = { 0 };
    options.struct_size = sizeof(options);
    options.legacy.struct_size = sizeof(options.legacy);
    Dust3DError error = { 0 };
    Dust3DSourceBakedMesh* owner = dust3d_source_baked_mesh_load(NULL, 0, &options, &error);
    if (owner != NULL || error.status != DUST3D_STATUS_INVALID_DOCUMENT)
        return 1;
    if (dust3d_source_baked_mesh_geometry(owner) != NULL || dust3d_source_baked_mesh_triangles(owner) != NULL
        || dust3d_source_baked_mesh_triangle_count(owner) != 0 || dust3d_source_baked_mesh_model_count(owner) != 0
        || dust3d_source_baked_mesh_component_count(owner) != 0 || dust3d_source_baked_mesh_payload_bytes(owner) != 0
        || dust3d_source_baked_mesh_texture(owner, DUST3D_TEXTURE_COLOR).pixels != NULL
        || dust3d_source_baked_mesh_component_kind(owner, 0) != DUST3D_SURFACE_COMPONENT_UNKNOWN
        || dust3d_source_baked_mesh_maximum_sampling_lod(owner) != 0)
        return 2;
    dust3d_source_baked_mesh_destroy(owner);
    return 0;
}
