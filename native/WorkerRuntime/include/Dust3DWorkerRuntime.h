/* SPDX-License-Identifier: MIT */
#ifndef DUST3D_WORKER_RUNTIME_H_
#define DUST3D_WORKER_RUNTIME_H_
#include <stddef.h>
#include <stdint.h>
#define DUST3D_WORKER_RUNTIME_ABI_VERSION 1
#ifdef __cplusplus
extern "C" {
#define DUST3D_WORKER_RUNTIME_NOEXCEPT noexcept
#else
#define DUST3D_WORKER_RUNTIME_NOEXCEPT
#endif

typedef enum Dust3DWorkerJobStatus {
    DUST3D_WORKER_JOB_COMPLETE = 0,
    DUST3D_WORKER_JOB_PROTOCOL_REJECTED = 1,
    DUST3D_WORKER_JOB_CODEC_REJECTED = 2,
    DUST3D_WORKER_JOB_IO_FAILED = 3,
    DUST3D_WORKER_JOB_LIMIT_EXCEEDED = 4,
    DUST3D_WORKER_JOB_INTERNAL_FAILED = 5
} Dust3DWorkerJobStatus;
typedef enum Dust3DWorkerOperation {
    DUST3D_WORKER_OPERATION_UNKNOWN = 0,
    DUST3D_WORKER_OPERATION_MESH = 1,
    DUST3D_WORKER_OPERATION_UV_MESH = 2
} Dust3DWorkerOperation;
typedef enum Dust3DWorkerDetailSource {
    DUST3D_WORKER_DETAIL_NONE = 0,
    DUST3D_WORKER_DETAIL_FRAME = 1,
    DUST3D_WORKER_DETAIL_UV_REQUEST = 2,
    DUST3D_WORKER_DETAIL_MESH = 3,
    DUST3D_WORKER_DETAIL_UV = 4,
    DUST3D_WORKER_DETAIL_UV_MESH = 5,
    DUST3D_WORKER_DETAIL_FAILURE = 6,
    DUST3D_WORKER_DETAIL_RUNTIME = 7,
    DUST3D_WORKER_DETAIL_SYSTEM = 8
} Dust3DWorkerDetailSource;
typedef enum Dust3DWorkerIOStage {
    DUST3D_WORKER_IO_NONE = 0,
    DUST3D_WORKER_IO_INPUT = 1,
    DUST3D_WORKER_IO_OUTPUT = 2
} Dust3DWorkerIOStage;

/** Self-owned result, never a wire struct. COMPLETE means a complete reply was
 * written, including valid native-failure frames; it does not mean mesh success
 * or process exit. No pointer/handle/destruction is needed. Nonzero status may
 * leave partial output: host must discard it and close its channels.
 * detail_code preserves the source-native diagnostic enum number for this pinned
 * revision, not a separately stable numeric schema. system_error preserves errno.
 * message contains up to 511 original diagnostic bytes plus NUL; truncation is
 * explicit. Treat descriptions as diagnostic data, never commands or format strings.
 */
typedef struct Dust3DWorkerJobResult {
    Dust3DWorkerJobStatus status;
    Dust3DWorkerOperation operation;
    uint64_t request_identity;
    int32_t system_error;
    Dust3DWorkerIOStage io_stage;
    Dust3DWorkerDetailSource detail_source;
    int32_t detail_code;
    uint8_t message_truncated;
    char message[512];
} Dust3DWorkerJobResult;

/** Return the header/layout version. No I/O, allocation, logging or exception. */
uint32_t dust3d_worker_runtime_abi_version(void) DUST3D_WORKER_RUNTIME_NOEXCEPT;

/** Serve one bounded frame plus EOF through the maintained base/UV runtime.
 * @param input_descriptor Separate borrowed readable job channel, never closed.
 * @param output_descriptor Borrowed writable reply channel, never closed.
 * @return Independent result with original cause/request/operation. C++ exceptions
 * are caught; signals, memory corruption and OS blocking are not contained here.
 * Worker-host-only: call away from UI, exclusively serialize jobs, establish
 * stdout/stderr isolation, SIGPIPE/resource/sandbox policy before invocation, and
 * close owned channels after return. This function changes none of those global
 * policies. No app adoption, cancellation/deadline or process-exit claim follows.
 * @code
 * Dust3DWorkerJobResult result = dust3d_worker_serve(input_fd, output_fd);
 * if (result.status != DUST3D_WORKER_JOB_COMPLETE) { // discard partial reply
 *     // Preserve result.system_error/detail_source/detail_code/message.
 * }
 * @endcode
 */
Dust3DWorkerJobResult dust3d_worker_serve(int32_t input_descriptor, int32_t output_descriptor) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Parent-owned incremental frame collector; never share it concurrently. */
typedef struct Dust3DWorkerReplyCollector Dust3DWorkerReplyCollector;
typedef struct Dust3DWorkerWireLimits {
    uint64_t maximum_input_bytes;
    uint64_t maximum_output_bytes;
    uint64_t maximum_failure_bytes;
} Dust3DWorkerWireLimits;
typedef enum Dust3DWorkerReplyStatus {
    DUST3D_WORKER_REPLY_READY = 0,
    DUST3D_WORKER_REPLY_FRAMED = 1,
    DUST3D_WORKER_REPLY_PROTOCOL_REJECTED = 2,
    DUST3D_WORKER_REPLY_LIMIT_EXCEEDED = 3,
    DUST3D_WORKER_REPLY_INTERNAL_FAILED = 4
} Dust3DWorkerReplyStatus;
/** Self-owned framing receipt with bounded original diagnostic bytes.
 * READY means an operation succeeded but EOF is not established. FRAMED means
 * exactly one frame plus actual EOF was supplied; neither status implies payload
 * semantics, process exit, authenticated completion or publication admission.
 * Diagnostic fields follow Dust3DWorkerJobResult's source/code/truncation rules.
 */
typedef struct Dust3DWorkerReplyResult {
    Dust3DWorkerReplyStatus status;
    Dust3DWorkerDetailSource detail_source;
    int32_t detail_code;
    uint8_t message_truncated;
    char message[512];
} Dust3DWorkerReplyResult;

/** Create an independent owner from a stable expected request header (exactly64
 * bytes) and explicit supported wire quotas. Copies identity/policy, reserves only
 * the fixed reply header. No descriptor I/O or native generation occurs.
 * @param expected_header Readable independent wire header, not a native struct.
 * @param header_size Exact readable byte count.
 * @param limits Positive supported limits; logical bytes, not allocator/RSS caps.
 * @param result Required writable self-owned receipt; NULL returns NULL.
 * @return Owner or NULL with original caught diagnostic in result. Destroy once.
 */
Dust3DWorkerReplyCollector* dust3d_worker_reply_create(const uint8_t* expected_header, size_t header_size,
    Dust3DWorkerWireLimits limits, Dust3DWorkerReplyResult* result) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Obtain bounded next channel read size, stopping at the fixed header first.
 * @param collector Live exclusively owned collector.
 * @param maximum_chunk Caller read cap, 1..65536 bytes.
 * @param count Required output; initialized to zero on failure.
 * @return READY and size, or caught diagnostic. Rejection of a live owner is sticky.
 */
Dust3DWorkerReplyResult dust3d_worker_reply_next_read(Dust3DWorkerReplyCollector* collector,
    size_t maximum_chunk, size_t* count) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Copy an independent span after header identity/quota validation.
 * @param collector Live exclusively owned collector.
 * @param bytes Stable readable input, independent of all collector-owned bytes;
 * NULL is allowed only for zero size. All borrowed frame views expire on this call.
 * @param size Exact readable count, not an allocation request.
 * @return READY/FRAMED or original caught diagnostic; failure is sticky.
 */
Dust3DWorkerReplyResult dust3d_worker_reply_append(Dust3DWorkerReplyCollector* collector,
    const uint8_t* bytes, size_t size) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Supply actual channel EOF, never inferred from a callback or expected size.
 * @param collector Live exclusively owned collector. Repeated complete EOF is idempotent.
 * @return FRAMED or caught truncation/state diagnostic; failure is sticky.
 */
Dust3DWorkerReplyResult dust3d_worker_reply_finish(Dust3DWorkerReplyCollector* collector) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Borrow one framed header+body only after successful EOF, otherwise NULL/zero.
 * @param collector Live owner, or NULL.
 * @param size Required output; NULL returns NULL. Initialized to zero on failure.
 * @return Borrowed bytes valid until any modifying call or owner destruction.
 * Swift must copy before destroying the owner; framing alone cannot authorize use.
 * @code
 * size_t size = 0;
 * const uint8_t* bytes = dust3d_worker_reply_bytes(owner, &size);
 * // Copy bytes while owner lives, then apply semantic/completion admission.
 * dust3d_worker_reply_destroy(owner);
 * @endcode
 */
const uint8_t* dust3d_worker_reply_bytes(const Dust3DWorkerReplyCollector* collector, size_t* size) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Destroy one owner after copied-value consumers are done. NULL is harmless. */
void dust3d_worker_reply_destroy(Dust3DWorkerReplyCollector* collector) DUST3D_WORKER_RUNTIME_NOEXCEPT;
#ifdef __cplusplus
}
#endif
#undef DUST3D_WORKER_RUNTIME_NOEXCEPT
#endif
