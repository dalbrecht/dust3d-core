// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_EXPERIMENTAL_FISH_IDLE_FBX_H_
#define DUST3D_NATIVE_EXPERIMENTAL_FISH_IDLE_FBX_H_

#include "include/Dust3DBridge.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Dust3DExperimentalFishIdleFbx Dust3DExperimentalFishIdleFbx;

typedef struct Dust3DExperimentalFishIdleFbxClipID {
    const char* id;
    size_t length;
} Dust3DExperimentalFishIdleFbxClipID;

/* Private source-only entry point. Resolve unique authored FishIdle IDs in
   caller order; all archive/ID/template spans are borrowed only for this call.
   On success owns independent animated FBX bytes. No filesystem publication. */
Dust3DExperimentalFishIdleFbx* dust3d_experimental_fish_idle_fbx_create(
    const uint8_t* archive, size_t archive_size, const Dust3DGeneratedRigBoneInput* bones, size_t bone_count,
    const Dust3DExperimentalFishIdleFbxClipID* ids, size_t clip_count, const Dust3DGeneratedRigOptions*,
    size_t maximum_fbx_bytes, Dust3DError*);
/* Borrow bytes until destruction; null owner returns null/zero. */
const uint8_t* dust3d_experimental_fish_idle_fbx_bytes(const Dust3DExperimentalFishIdleFbx*);
size_t dust3d_experimental_fish_idle_fbx_size(const Dust3DExperimentalFishIdleFbx*);
void dust3d_experimental_fish_idle_fbx_destroy(Dust3DExperimentalFishIdleFbx*);

#ifdef __cplusplus
}
#endif

#endif
