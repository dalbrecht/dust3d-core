// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_EXPERIMENTAL_FISH_IDLE_H_
#define DUST3D_NATIVE_EXPERIMENTAL_FISH_IDLE_H_

#include "include/Dust3DBridge.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Private, unregistered source contract for the procedural FishIdle clip. */
typedef struct Dust3DExperimentalFishIdle Dust3DExperimentalFishIdle;

typedef struct Dust3DExperimentalFishIdleOptions {
    uint32_t version;
    uint32_t struct_size;
    /* V1 rig limits and shared borrowed token. maximum_output_bytes covers
       simultaneous original rest, copied poses, and draw staging. */
    Dust3DGeneratedRigOptions rig;
} Dust3DExperimentalFishIdleOptions;

typedef struct Dust3DExperimentalFishIdleParameter {
    const char* name;
    size_t name_length;
    const char* value;
    size_t value_length;
} Dust3DExperimentalFishIdleParameter;

typedef struct Dust3DExperimentalFishIdleInfo {
    /* All pointers borrow the same owner until destroy. The rest must not be
       destroyed separately. Clip type and warning are length-delimited. */
    const Dust3DGeneratedRig* rest;
    const double* sample_times;
    const char* clip_type;
    const char* warning;
    size_t clip_type_length;
    size_t warning_length;
    size_t frame_count;
    size_t joint_count;
    size_t matrix_count;
    size_t output_bytes;
    double duration_seconds;
    uint32_t loops;
} Dust3DExperimentalFishIdleInfo;

typedef struct Dust3DExperimentalFishIdleMatrix {
    /* Column-major, grounded model-from-joint transform. */
    double values[16];
} Dust3DExperimentalFishIdleMatrix;

Dust3DExperimentalFishIdleOptions dust3d_experimental_fish_idle_default_options(void);
/* Borrows all inputs for this call. Parameter names are unique exact ASCII
   catalog keys, values are bounded decimal spans. Optional audio metadata
   belongs to the caller and is not a visual parameter. NULL on typed failure. */
Dust3DExperimentalFishIdle* dust3d_experimental_fish_idle_load(const uint8_t* archive, size_t archive_size,
    const Dust3DGeneratedRigBoneInput* bones, size_t bone_count,
    const Dust3DExperimentalFishIdleParameter* parameters, size_t parameter_count,
    const Dust3DExperimentalFishIdleOptions*, Dust3DError*);
/* NULL-safe. Invalidates the rest and every borrowed view. */
void dust3d_experimental_fish_idle_destroy(Dust3DExperimentalFishIdle*);
/* Returns 1 on success, 0 on invalid input, and clears output on failure. */
int dust3d_experimental_fish_idle_describe(const Dust3DExperimentalFishIdle*, Dust3DExperimentalFishIdleInfo*);
/* Copies one typed record, preserving source joint order. Returns 0 for an
   invalid owner/index/destination and leaves any destination unchanged. */
int dust3d_experimental_fish_idle_matrix_at(const Dust3DExperimentalFishIdle*, size_t frame, size_t joint,
    Dust3DExperimentalFishIdleMatrix*);

#ifdef __cplusplus
}
#endif

#endif
