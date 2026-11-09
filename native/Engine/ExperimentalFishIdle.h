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
/* Resolve the exact authored ID from the archive. With PCM enabled, read
   its authored surface or the first catalog surface when the field is absent. */
Dust3DExperimentalFishIdle* dust3d_experimental_fish_idle_load_selected(const uint8_t* archive, size_t archive_size,
    const Dust3DGeneratedRigBoneInput* bones, size_t bone_count, const char* animation_id, size_t animation_id_length,
    uint32_t include_pcm, const Dust3DExperimentalFishIdleOptions*, Dust3DError*);
/* The borrowed selected ID expires with the owner. NULL for legacy loads. */
const char* dust3d_experimental_fish_idle_selected_id(const Dust3DExperimentalFishIdle*, size_t* length);
/* NULL-safe. Invalidates the rest and every borrowed view. */
void dust3d_experimental_fish_idle_destroy(Dust3DExperimentalFishIdle*);
/* Returns 1 on success, 0 on invalid input, and clears output on failure. */
int dust3d_experimental_fish_idle_describe(const Dust3DExperimentalFishIdle*, Dust3DExperimentalFishIdleInfo*);
/* Copies one typed record, preserving source joint order. Returns 0 for an
   invalid owner/index/destination and leaves any destination unchanged. */
int dust3d_experimental_fish_idle_matrix_at(const Dust3DExperimentalFishIdle*, size_t frame, size_t joint,
    Dust3DExperimentalFishIdleMatrix*);

/* Ground travel in the same producer space as the poses. Invalid owner or
   destination returns 0 and leaves any destination unchanged. */
typedef struct Dust3DExperimentalFishIdleGroundMotion {
    float speed;
    float direction_x;
    float direction_z;
} Dust3DExperimentalFishIdleGroundMotion;
int dust3d_experimental_fish_idle_ground_motion(const Dust3DExperimentalFishIdle*,
    Dust3DExperimentalFishIdleGroundMotion*);

/* V2 opt-in PCM request. The surface must be an exact reference catalog name.
   The default options require the caller to supply a surface span. */
typedef struct Dust3DExperimentalFishIdlePCMOptions {
    uint32_t version;
    uint32_t struct_size;
    Dust3DGeneratedRigOptions rig;
    const char* surface;
    size_t surface_length;
} Dust3DExperimentalFishIdlePCMOptions;

typedef struct Dust3DExperimentalFishIdlePCMInfo {
    Dust3DExperimentalFishIdleInfo visual;
    uint32_t sample_rate;
    uint32_t channel_count;
    size_t sample_count;
    /* Combined retained visual and PCM bytes. All audio fields are zero for
       an owner with no samples. The nested visual receipt excludes PCM. */
    size_t output_bytes;
} Dust3DExperimentalFishIdlePCMInfo;

Dust3DExperimentalFishIdlePCMOptions dust3d_experimental_fish_idle_default_pcm_options(void);
/* Same borrowed input and owned result rules as the visual load. A shared
   maximum_output_bytes also bounds the PCM and temporary reference mix. */
Dust3DExperimentalFishIdle* dust3d_experimental_fish_idle_load_pcm(const uint8_t* archive, size_t archive_size,
    const Dust3DGeneratedRigBoneInput* bones, size_t bone_count,
    const Dust3DExperimentalFishIdleParameter* parameters, size_t parameter_count,
    const Dust3DExperimentalFishIdlePCMOptions*, Dust3DError*);
/* As for the visual describe, every borrowed field expires with the owner. */
int dust3d_experimental_fish_idle_describe_pcm(const Dust3DExperimentalFishIdle*, Dust3DExperimentalFishIdlePCMInfo*);
/* Copies one signed mono PCM sample. Invalid owner/index/destination returns
   0 and leaves any destination unchanged. */
int dust3d_experimental_fish_idle_pcm_at(const Dust3DExperimentalFishIdle*, size_t sample, int16_t*);

#ifdef __cplusplus
}
#endif

#endif
