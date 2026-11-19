// SPDX-License-Identifier: MIT
#include <Dust3DBridge.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
_Static_assert(sizeof(Dust3DRigTemplateDrawVertex) == 32, "Draw record size");
_Static_assert(offsetof(Dust3DRigTemplateDrawVertex, normal) == 12, "Normal offset");
_Static_assert(offsetof(Dust3DRigTemplateDrawVertex, bone_index) == 24, "Bone offset");
_Static_assert(offsetof(Dust3DRigTemplateDrawVertex, role) == 28, "Role offset");
int main(void)
{
    Dust3DError error = { 0 };
    Dust3DRigTemplateOptions options = dust3d_rig_template_default_options();
    Dust3DRigTemplateBoneInput bone = { 0 };
    bone.name = "Root";
    bone.name_length = 4;
    bone.to_position[1] = 1;
    Dust3DRigTemplateMesh* mesh = dust3d_rig_template_mesh_create(&bone, 1, &options, &error);
    if (!mesh || error.status != DUST3D_STATUS_OK)
        return 1;
    size_t length = 0;
    const char* name = dust3d_rig_template_mesh_bone_name(mesh, 0, &length);
    const Dust3DRigTemplateDrawVertex* vertices = dust3d_rig_template_mesh_vertices(mesh);
    int valid = vertices && dust3d_rig_template_mesh_vertex_count(mesh) == 1764
        && name && length == 4 && memcmp(name, "Root", 4) == 0;
    dust3d_rig_template_mesh_destroy(mesh);
    if (!valid)
        return 1;
    puts("Pure C template owner and layout contracts passed");
    return 0;
}
