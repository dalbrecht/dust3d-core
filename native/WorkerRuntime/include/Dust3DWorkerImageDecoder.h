/* SPDX-License-Identifier: MIT */
#ifndef DUST3D_WORKER_IMAGE_DECODER_H_
#define DUST3D_WORKER_IMAGE_DECODER_H_
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#define DUST3D_WORKER_IMAGE_NOEXCEPT noexcept
#else
#define DUST3D_WORKER_IMAGE_NOEXCEPT
#endif
#define DUST3D_WORKER_IMAGE_ABI_VERSION 1
#define DUST3D_WORKER_IMAGE_CAUSE_BYTES 512
/** An independent tight straight RGBA8 owner. Never share concurrently or fabricate handles. */
typedef struct Dust3DWorkerImage Dust3DWorkerImage;
typedef enum Dust3DWorkerImageStatus {
    DUST3D_WORKER_IMAGE_OK = 0,
    DUST3D_WORKER_IMAGE_INVALID_INPUT = 1,
    DUST3D_WORKER_IMAGE_LIMIT_EXCEEDED = 2,
    DUST3D_WORKER_IMAGE_DECODE_FAILED = 3,
    DUST3D_WORKER_IMAGE_INTERNAL_FAILED = 4
} Dust3DWorkerImageStatus;
/** Self-owned diagnostic. cause_length is 0..512, cause_truncated is 0/1.
 * Bytes are opaque and may include NUL/non-UTF8; never interpret them as commands.
 * Success has code/length/truncation zero. No destruction required. */
typedef struct Dust3DWorkerImageResult {
    Dust3DWorkerImageStatus status;
    int32_t code;
    uint32_t cause_length;
    uint8_t cause_truncated;
    uint8_t cause[DUST3D_WORKER_IMAGE_CAUSE_BYTES];
} Dust3DWorkerImageResult;
/** Make an owned failure diagnostic without allocation; preserves code and the first
 * 512 actual readable cause bytes, explicitly marking truncation. Null is valid only
 * for zero length. Unsupported/success status or inconsistent span becomes INVALID_INPUT.
 * Example: result = dust3d_worker_image_error_create(DUST3D_WORKER_IMAGE_DECODE_FAILED, 7, bytes, count); */
Dust3DWorkerImageResult dust3d_worker_image_error_create(Dust3DWorkerImageStatus status, int32_t code, const uint8_t* cause, size_t count) DUST3D_WORKER_IMAGE_NOEXCEPT;
/** Validate positive dimensions/tight bytes and a positive quota <=256 MiB before
 * copying pixels. Input is borrowed for this call; output must be destroyed.
 * result is required, reset on entry, and retains original status/cause on failure.
 * No color/orientation conversion, premultiplication, platform decoding or native
 * archive parsing occurs. C++ exceptions are caught. Allocation limit excludes
 * root/capacity/allocator overhead, callback/platform workspace and process RSS.
 * Example: image = dust3d_worker_image_create(1, 1, rgba, 4, 4, &result); */
Dust3DWorkerImage* dust3d_worker_image_create(uint32_t width, uint32_t height, const uint8_t* pixels, size_t count, uint64_t maximum_bytes, Dust3DWorkerImageResult* result) DUST3D_WORKER_IMAGE_NOEXCEPT;
/** Release a quiescent owned image; null is safe. */
void dust3d_worker_image_destroy(Dust3DWorkerImage* image) DUST3D_WORKER_IMAGE_NOEXCEPT;
/** Null getters return zero. No borrowed pixel pointer escapes. */
uint32_t dust3d_worker_image_width(const Dust3DWorkerImage* image) DUST3D_WORKER_IMAGE_NOEXCEPT;
uint32_t dust3d_worker_image_height(const Dust3DWorkerImage* image) DUST3D_WORKER_IMAGE_NOEXCEPT;
uint64_t dust3d_worker_image_byte_count(const Dust3DWorkerImage* image) DUST3D_WORKER_IMAGE_NOEXCEPT;
/** Copy a bounded range only after all guards; invalid requests return 0 without writes.
 * Empty range at end accepts null destination. Destination must actually hold count bytes. */
uint8_t dust3d_worker_image_copy_pixels(const Dust3DWorkerImage* image, uint64_t offset, uint8_t* destination, size_t count) DUST3D_WORKER_IMAGE_NOEXCEPT;
/** Callback-scoped borrowed immutable readable spans. encoding is PNG1 or JPEG2;
 * model is exact non-NUL UTF8 identity bytes, not a terminated string. Input/output
 * quotas are positive <=256 MiB and independent. struct_size equals sizeof this type. */
typedef struct Dust3DWorkerImageInput {
    size_t struct_size;
    const uint8_t* encoded;
    size_t encoded_count;
    const uint8_t* model;
    size_t model_count;
    uint32_t encoding;
    uint64_t maximum_encoded_bytes;
    uint64_t maximum_decoded_bytes;
} Dust3DWorkerImageInput;
/** Execute synchronously in the calling worker, enforce quotas before allocation,
 * return straight RGBA8 source scanlines and preserve original failure diagnostics.
 * Return a newly owned image with OK, or null with a non-success result. Ownership
 * of ANY returned image transfers to the adapter even when result reports failure.
 * Do not retain borrowed input, share context concurrently, throw across C/Swift,
 * call parent native fallbacks or alter process security/global policies. */
typedef Dust3DWorkerImage* (*Dust3DWorkerDecodeImage)(void* context, const Dust3DWorkerImageInput* input, Dust3DWorkerImageResult* result);
/** Adapter copies this configuration. Context and code must outlive exclusive use.
 * No retain/release callbacks or global registration; struct_size must match exactly. */
typedef struct Dust3DWorkerImageDecoder {
    size_t struct_size;
    void* context;
    Dust3DWorkerDecodeImage decode;
} Dust3DWorkerImageDecoder;
#ifdef __cplusplus
}
#endif
#endif
