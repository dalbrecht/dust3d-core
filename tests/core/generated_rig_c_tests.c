// SPDX-License-Identifier: MIT
#include <Dust3DBridge.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
_Static_assert(sizeof(Dust3DGeneratedRigJoint) == 296, "Joint layout");
_Static_assert(offsetof(Dust3DGeneratedRigJoint, rest_world) == 40, "World layout");
_Static_assert(offsetof(Dust3DGeneratedRigJoint, inverse_bind) == 168, "Inverse layout");
_Static_assert(sizeof(Dust3DGeneratedRigWeights) == 32, "Weight layout");
int main(int argc, char** argv)
{
    if (argc != 2)
        return 1;
    FILE* file = fopen(argv[1], "rb");
    if (!file)
        return 2;
    if (fseek(file, 0, SEEK_END)) {
        fclose(file);
        return 3;
    }
    long size = ftell(file);
    if (size <= 0 || size > 100000) {
        fclose(file);
        return 4;
    }
    rewind(file);
    unsigned char* bytes = (unsigned char*)malloc((size_t)size);
    if (!bytes) {
        fclose(file);
        return 5;
    }
    if (fread(bytes, 1, (size_t)size, file) != (size_t)size) {
        free(bytes);
        fclose(file);
        return 6;
    }
    fclose(file);
    Dust3DGeneratedRigBoneInput bone = { 0 };
    bone.name = "Body";
    bone.name_length = 4;
    Dust3DError error = { 0 };
    Dust3DGeneratedRig* owner = dust3d_generated_rig_load(bytes, (size_t)size, "Test", 4, &bone, 1, NULL, &error);
    free(bytes);
    if (!owner) {
        fprintf(stderr, "%s\n", error.message);
        return 7;
    }
    const Dust3DMesh* mesh = dust3d_generated_rig_mesh(owner);
    const Dust3DGeneratedRigWeights* weights = dust3d_generated_rig_weights(owner);
    size_t length = 0;
    const char* name = dust3d_generated_rig_joint_name(owner, 0, &length);
    int valid = dust3d_generated_rig_joint_count(owner) == 1 && dust3d_mesh_vertex_count(mesh) > 0 && name && length == 4 && !memcmp(name, "Body", 4) && weights && weights[0].joints[0] == UINT32_MAX && weights[0].weights[0] == 0;
    if (!valid || !dust3d_mesh_vertices(mesh)) {
        dust3d_generated_rig_destroy(owner);
        return 8;
    }
    Dust3DVertex copy = dust3d_mesh_vertices(mesh)[0];
    dust3d_generated_rig_destroy(owner);
    valid = valid && isfinite(copy.position[0]);
    dust3d_generated_rig_destroy(NULL);
    return valid ? 0 : 8;
}
