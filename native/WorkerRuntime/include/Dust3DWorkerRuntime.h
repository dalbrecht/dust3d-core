/* SPDX-License-Identifier: MIT */
#ifndef DUST3D_WORKER_RUNTIME_H_
#define DUST3D_WORKER_RUNTIME_H_
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
#ifdef __cplusplus
}
#endif
#undef DUST3D_WORKER_RUNTIME_NOEXCEPT
#endif
