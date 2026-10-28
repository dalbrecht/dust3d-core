/* SPDX-License-Identifier: MIT */
#ifndef DUST3D_WORKER_PREVIEW_SESSION_H_
#define DUST3D_WORKER_PREVIEW_SESSION_H_
#include "Dust3DWorkerPreviewArchive.h"
#include "Dust3DWorkerSessionOutput.h"
#ifdef __cplusplus
extern "C" {
#define DUST3D_WORKER_PREVIEW_SESSION_NOEXCEPT noexcept
#else
#define DUST3D_WORKER_PREVIEW_SESSION_NOEXCEPT
#endif
/** Same independent correlation fields as archive scope; this entry point fixes
 * direction to session. Correlation is not peer/current-source admission. */
typedef Dust3DWorkerPreviewArchiveScope Dust3DWorkerPreviewSessionScope;
typedef enum Dust3DWorkerPreviewSessionStatus {
    DUST3D_WORKER_PREVIEW_SESSION_OK = 0,
    DUST3D_WORKER_PREVIEW_SESSION_CHUNK_REJECTED = 1,
    DUST3D_WORKER_PREVIEW_SESSION_RANGE_REJECTED = 2,
    DUST3D_WORKER_PREVIEW_SESSION_CODEC_REJECTED = 3,
    DUST3D_WORKER_PREVIEW_SESSION_IO_FAILED = 4,
    DUST3D_WORKER_PREVIEW_SESSION_ALLOCATION_FAILED = 5,
    DUST3D_WORKER_PREVIEW_SESSION_INTERNAL_FAILED = 6
} Dust3DWorkerPreviewSessionStatus;
/** Self-owned original typed receipt for this pinned revision. detail_source/code
 * retain the original semantic enum; system_error/io_stage retain errno and stage.
 * Message is bounded to511 bytes plus NUL with explicit truncation. Diagnostic bytes
 * are data, never commands, format strings or proof of peer/process completion. */
typedef struct Dust3DWorkerPreviewSessionResult {
    Dust3DWorkerPreviewSessionStatus status;
    Dust3DWorkerDetailSource detail_source;
    int32_t detail_code, system_error;
    Dust3DWorkerIOStage io_stage;
    uint8_t message_truncated;
    char message[512];
} Dust3DWorkerPreviewSessionResult;
/** Read segmented values through real EOF into the existing immutable session owner.
 * @param input Exclusive borrowed readable job descriptor, never opened/closed here.
 * @param scope Independently captured request/lifetime correlation, never from input.
 * @param context Original native request controls, frozen before reading.
 * @param policy Explicit whole-transfer/chunk bounds, no legacy default/cap changes.
 * @param result Required independent original receipt; NULL returns NULL without work.
 * @return Fully validated owner on OK, NULL on failure; all exceptions caught. Destroy
 * with dust3d_worker_session_output_destroy. Existing const getters borrow until then;
 * copy all values before owner destruction or actor transfer. No legacy whole-envelope
 * decode or count/text quotas are reapplied and no duplicate wire body is retained.
 * Caller supplies isolated non-UI execution/supervision and separately authenticates
 * peer, verifies completion/current source and admits publication. No native generation,
 * deadline or cancellation. Example: owner = dust3d_worker_preview_session_read(fd,
 * scope, controls, chunking, &result);
 */
Dust3DWorkerSessionOutput* dust3d_worker_preview_session_read(int32_t input, Dust3DWorkerPreviewSessionScope scope, Dust3DWorkerPreviewArchiveContext context, Dust3DWorkerPreviewChunkPolicy policy, Dust3DWorkerPreviewSessionResult* result) DUST3D_WORKER_PREVIEW_SESSION_NOEXCEPT;
#ifdef __cplusplus
}
#endif
#undef DUST3D_WORKER_PREVIEW_SESSION_NOEXCEPT
#endif
