/* SPDX-License-Identifier: MIT */
#ifndef DUST3D_WORKER_PREVIEW_ARCHIVE_H_
#define DUST3D_WORKER_PREVIEW_ARCHIVE_H_
#include "Dust3DWorkerRuntime.h"
#ifdef __cplusplus
extern "C" {
#define DUST3D_WORKER_PREVIEW_NOEXCEPT noexcept
#else
#define DUST3D_WORKER_PREVIEW_NOEXCEPT
#endif
/** Independent host correlation; direction is always archive. Never infer these
 * values from the incoming stream. Correlation alone is not peer authentication.
 */
typedef struct Dust3DWorkerPreviewArchiveScope {
    Dust3DWorkerRequestIdentity identity;
    uint64_t connection_generation;
} Dust3DWorkerPreviewArchiveScope;
/** Captured native controls. Archive is 1 byte through 256 MiB; source zero selects
 * 64 MiB, otherwise at most 256 MiB. Indexed zero selects 256 MiB, otherwise any
 * positive addressable native budget. Preserve raw zero/default settings.
 */
typedef struct Dust3DWorkerPreviewArchiveContext {
    uint64_t archive_bytes, maximum_source_bytes, maximum_indexed_bytes;
} Dust3DWorkerPreviewArchiveContext;
/** Explicit whole body bound plus chunk size 1..65536. Physical headers add bytes.
 * Logical quotas do not bound RSS/CPU. No existing wire/default quota is changed.
 */
typedef struct Dust3DWorkerPreviewChunkPolicy {
    uint64_t maximum_transfer_bytes;
    uint32_t chunk_payload_bytes;
} Dust3DWorkerPreviewChunkPolicy;
typedef enum Dust3DWorkerPreviewArchiveStatus {
    DUST3D_WORKER_PREVIEW_ARCHIVE_OK = 0,
    DUST3D_WORKER_PREVIEW_ARCHIVE_CHUNK_REJECTED = 1,
    DUST3D_WORKER_PREVIEW_ARCHIVE_SOURCE_REJECTED = 2,
    DUST3D_WORKER_PREVIEW_ARCHIVE_IO_FAILED = 3,
    DUST3D_WORKER_PREVIEW_ARCHIVE_ALLOCATION_FAILED = 4,
    DUST3D_WORKER_PREVIEW_ARCHIVE_INTERNAL_FAILED = 5
} Dust3DWorkerPreviewArchiveStatus;
/** Self-owned receipt; detail_code preserves the original enum for this pinned
 * revision, system_error the original errno. Message retains up to 255 bytes plus
 * NUL with explicit truncation. Treat it as diagnostic data, never format/command text.
 * Failure may leave partial output: discard it and finalize host-owned channels.
 */
typedef struct Dust3DWorkerPreviewArchiveResult {
    Dust3DWorkerPreviewArchiveStatus status;
    int32_t detail_code, system_error;
    Dust3DWorkerIOStage io_stage;
    uint8_t message_truncated;
    char message[256];
} Dust3DWorkerPreviewArchiveResult;
/** Complete immutable archive storage; destroy only with its matching function. */
typedef struct Dust3DWorkerPreviewArchive Dust3DWorkerPreviewArchive;
/** Write captured opaque archive bytes through bounded chunks, no native parsing.
 * @param output Borrowed writable job descriptor; never closed or shut down here.
 * @param scope Independent host correlation, checked before first write.
 * @param context Captured request controls, checked before any output.
 * @param archive Stable readable input retained by caller until return, not afterward.
 * @param size Exact archive count, must equal context.archive_bytes.
 * @param policy Frozen bounded transfer/chunk policy.
 * @return OK for completed writes, not EOF/native success; all exceptions are caught.
 * Host establishes broken-pipe signal/resource policy and finalizes output to produce
 * actual EOF. Blocking non-UI operation; no cancellation/deadline/peer admission.
 * Example: result = dust3d_worker_preview_archive_write(fd, scope, controls, data, size, policy);
 */
Dust3DWorkerPreviewArchiveResult dust3d_worker_preview_archive_write(int32_t output, Dust3DWorkerPreviewArchiveScope scope, Dust3DWorkerPreviewArchiveContext context, const uint8_t* archive, size_t size, Dust3DWorkerPreviewChunkPolicy policy) DUST3D_WORKER_PREVIEW_NOEXCEPT;
/** Receive exact framing/prefix plus actual descriptor EOF before publishing owner.
 * @param input Exclusive borrowed readable job descriptor; never opened/closed here.
 * @param scope Independent host correlation, never trusted from incoming bytes.
 * @param context Independently admitted request controls, frozen before reading.
 * @param policy Frozen bounded transfer/chunk policy, checked before reading.
 * @param result Required self-owned receipt; NULL returns NULL without any work.
 * @return Immutable complete owner on OK, NULL on original typed failure. All
 * exceptions are caught; partial owners are destroyed. Host provides isolated non-UI
 * execution/supervision. No deadline/cancellation or native parsing/dispatch. This does
 * not authenticate the peer or admit current source/application publication.
 * Example: owner = dust3d_worker_preview_archive_read(fd, scope, controls, policy, &result);
 */
Dust3DWorkerPreviewArchive* dust3d_worker_preview_archive_read(int32_t input, Dust3DWorkerPreviewArchiveScope scope, Dust3DWorkerPreviewArchiveContext context, Dust3DWorkerPreviewChunkPolicy policy, Dust3DWorkerPreviewArchiveResult* result) DUST3D_WORKER_PREVIEW_NOEXCEPT;
/** Borrow immutable archive bytes until owner destruction. Required size clears
 * for NULL owner; NULL size returns NULL. Copy before destruction or actor transfer.
 */
const uint8_t* dust3d_worker_preview_archive_bytes(const Dust3DWorkerPreviewArchive* owner, size_t* size) DUST3D_WORKER_PREVIEW_NOEXCEPT;
/** Copy captured native policies; all zero for NULL owner. */
Dust3DWorkerPreviewArchiveContext dust3d_worker_preview_archive_context(const Dust3DWorkerPreviewArchive* owner) DUST3D_WORKER_PREVIEW_NOEXCEPT;
/** Destroy owned storage; accepts NULL. Never closes any borrowed descriptor. */
void dust3d_worker_preview_archive_destroy(Dust3DWorkerPreviewArchive* owner) DUST3D_WORKER_PREVIEW_NOEXCEPT;
#ifdef __cplusplus
}
#endif
#endif
