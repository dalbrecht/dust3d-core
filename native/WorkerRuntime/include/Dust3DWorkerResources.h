/* SPDX-License-Identifier: MIT */
#ifndef DUST3D_WORKER_RESOURCES_C_H_
#define DUST3D_WORKER_RESOURCES_C_H_
#include <stdint.h>
#define DUST3D_WORKER_RESOURCE_ABI_VERSION 1
#define DUST3D_WORKER_RESOURCE_COUNT 4
#ifdef __cplusplus
extern "C" {
#define DUST3D_WORKER_RESOURCE_NOEXCEPT noexcept
#else
#define DUST3D_WORKER_RESOURCE_NOEXCEPT
#endif

typedef struct Dust3DWorkerResourceBudget {
    uint64_t soft;
    uint64_t hard;
} Dust3DWorkerResourceBudget;

/** Explicit process-lifetime requests. No value means infinity. Address/RSS limits are not supplied.
 * Descriptor ceilings are highest admissible descriptor number plus one, not open-file counts.
 * Cumulative CPU seconds are not per-job wall-clock deadlines. Core dumps are always disabled. */
typedef struct Dust3DWorkerResourceProfile {
    uint32_t abi_version;
    Dust3DWorkerResourceBudget cpu_seconds;
    Dust3DWorkerResourceBudget file_bytes;
    Dust3DWorkerResourceBudget descriptors;
    uint64_t minimum_descriptor_ceiling;
} Dust3DWorkerResourceProfile;

typedef enum Dust3DWorkerResourceStatus {
    DUST3D_WORKER_RESOURCE_READY = 0,
    DUST3D_WORKER_RESOURCE_POLICY_REJECTED = 1,
    DUST3D_WORKER_RESOURCE_SYSTEM_FAILED = 2,
    DUST3D_WORKER_RESOURCE_READBACK_REJECTED = 3,
    DUST3D_WORKER_RESOURCE_UNSUPPORTED = 4,
    DUST3D_WORKER_RESOURCE_INTERNAL_FAILED = 5
} Dust3DWorkerResourceStatus;
typedef enum Dust3DWorkerResourceOperation {
    DUST3D_WORKER_RESOURCE_OPERATION_NONE = 0,
    DUST3D_WORKER_RESOURCE_OPERATION_INSPECT = 1,
    DUST3D_WORKER_RESOURCE_OPERATION_APPLY = 2,
    DUST3D_WORKER_RESOURCE_OPERATION_VERIFY = 3
} Dust3DWorkerResourceOperation;

/** Self-owned observation; no pointer, wire layout or destruction contract.
 * Effective order is CPU/file/core/descriptors. Only applied_count entries were applied;
 * verified_count reports successful exact readbacks. Partial mutations are never rolled back.
 * detail_resource/code retain pinned native enum numbers, not unrelated runtime domains.
 * has_detail_resource distinguishes absent metadata from resource zero (CPU).
 * runtime is 0 unknown, 1 ordinary, 2 address-sanitized. system_error preserves original errno.
 * message preserves up to 255 diagnostic bytes plus NUL; truncation is explicit. */
typedef struct Dust3DWorkerResourceResult {
    uint32_t abi_version;
    Dust3DWorkerResourceStatus status;
    Dust3DWorkerResourceOperation operation;
    uint32_t runtime;
    uint32_t detail_resource;
    int32_t detail_code;
    int32_t system_error;
    uint8_t has_detail_resource;
    uint8_t application_started;
    uint32_t applied_count;
    uint32_t verified_count;
    Dust3DWorkerResourceBudget effective[DUST3D_WORKER_RESOURCE_COUNT];
    uint8_t message_truncated;
    char message[256];
} Dust3DWorkerResourceResult;

/** No I/O, allocation or resource mutation. */
uint32_t dust3d_worker_resource_abi_version(void) DUST3D_WORKER_RESOURCE_NOEXCEPT;
/** MUTATES CURRENT PROCESS LIMITS. Fresh helper bootstrap ONLY, exclusively before listener/native work.
 * Never call in the app/test runner. Validate all requests/capabilities before any application;
 * exact readback is required for READY. Failure can leave partial limits: fail closed before work,
 * preserve this result and exit normally; do not retry or claim rollback/containment.
 * No signals, native cancellation, address-space/RSS cap or process-exit proof. Exceptions are caught.
 * @param profile Explicit trusted own-bundle policy, never archive-derived or an environment fallback.
 * @return Original status/resource/operation/errno and independent application/readback progress. */
Dust3DWorkerResourceResult dust3d_worker_resources_apply(Dust3DWorkerResourceProfile profile) DUST3D_WORKER_RESOURCE_NOEXCEPT;
#ifdef __cplusplus
}
#endif
#endif
