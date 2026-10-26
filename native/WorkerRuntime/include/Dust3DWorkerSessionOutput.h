/* SPDX-License-Identifier: MIT */
#ifndef DUST3D_WORKER_SESSION_OUTPUT_H_
#define DUST3D_WORKER_SESSION_OUTPUT_H_
#include "Dust3DWorkerRuntime.h"
#ifdef __cplusplus
extern "C" {
#define DUST3D_WORKER_SESSION_NOEXCEPT noexcept
#else
#define DUST3D_WORKER_SESSION_NOEXCEPT
#endif
/** One immutable owner for decoded session geometry, diagnostics and statistics. */
typedef struct Dust3DWorkerSessionOutput Dust3DWorkerSessionOutput;
typedef enum Dust3DWorkerSessionOutcome {
    DUST3D_WORKER_SESSION_NONE = 0,
    DUST3D_WORKER_SESSION_SOURCED = 1,
    DUST3D_WORKER_SESSION_ORDINARY = 2,
    DUST3D_WORKER_SESSION_EMPTY = 3,
    DUST3D_WORKER_SESSION_FAILURE = 4
} Dust3DWorkerSessionOutcome;
/** Independent aggregate quotas, each positive and at most 256 MiB. Source allowance
 * zero selects 64 MiB; a nonzero allowance is at most 256 MiB and is also bounded by
 * maximum_decoded_bytes. Existing indexed/source count/diagnostic limits still apply.
 * Logical storage excludes roots/scalars, capacities, temporary workspace and RSS.
 */
typedef struct Dust3DWorkerSessionOutputLimits {
    uint64_t maximum_wire_bytes, maximum_decoded_bytes, maximum_source_bytes;
} Dust3DWorkerSessionOutputLimits;
/** Scalar copies; NONE/zero for NULL. Availability distinguishes measured zero. */
typedef struct Dust3DWorkerSessionOutputSummary {
    Dust3DWorkerSessionOutcome outcome;
    uint8_t statistics_available;
} Dust3DWorkerSessionOutputSummary;
/** Actual observations only; no pointers/native layout. Query availability separately. */
typedef struct Dust3DWorkerSessionStatistics {
    uint64_t maximum_cache_payload_bytes, retained_payload_bytes;
    uint64_t reused_components, rebuilt_components, rebuilt_parts;
    uint32_t cache_outcome;
    int32_t cache_status;
} Dust3DWorkerSessionStatistics;
/** Decode an exact body into one independent owner; no input pointers escape.
 * @param bytes Complete stable session-envelope body, borrowed through return.
 * @param size Exact readable count; NULL/invalid/trailing bodies reject.
 * @param limits Supported aggregate/source policy; independent inner defaults apply.
 * @param result Required self-owned status and original typed bounded diagnostic.
 * @return Owner on OK, NULL on failure or NULL result. Destroy with the matching
 * session function. All exceptions are caught. This is values-only decoding:
 * it does NOT authenticate peers, correlate a request or admit current publication.
 * Host must complete those gates separately before consuming/publishing output.
 * Example: owner = dust3d_worker_session_output_decode(body, size, limits, &result);
 */
Dust3DWorkerSessionOutput* dust3d_worker_session_output_decode(const uint8_t* bytes, size_t size, Dust3DWorkerSessionOutputLimits limits, Dust3DWorkerDecodeResult* result) DUST3D_WORKER_SESSION_NOEXCEPT;
/** Copy immutable outcome and availability; zero for NULL. */
Dust3DWorkerSessionOutputSummary dust3d_worker_session_output_summary(const Dust3DWorkerSessionOutput* owner) DUST3D_WORKER_SESSION_NOEXCEPT;
/** Borrow immutable geometry/failure view; NULL for empty/NULL owner. Use existing
 * decoded getters. Never destroy this const view; it expires with the session owner.
 * Example: view = dust3d_worker_session_output_primary(owner);
 */
const Dust3DWorkerDecodedReply* dust3d_worker_session_output_primary(const Dust3DWorkerSessionOutput* owner) DUST3D_WORKER_SESSION_NOEXCEPT;
/** Borrow original source failure view, or NULL. Same owner lifetime; never destroy.
 * Existing failure getters copy/borrow its opaque cause and classification.
 */
const Dust3DWorkerDecodedReply* dust3d_worker_session_output_source_issue(const Dust3DWorkerSessionOutput* owner) DUST3D_WORKER_SESSION_NOEXCEPT;
/** Borrow original statistics failure view, or NULL. Same lifetime; never destroy. */
const Dust3DWorkerDecodedReply* dust3d_worker_session_output_statistics_issue(const Dust3DWorkerSessionOutput* owner) DUST3D_WORKER_SESSION_NOEXCEPT;
/** Copy actual scalars if available. Require non-NULL output and exact sizeof the
 * current statistics struct; invalid storage/size rejects without writes. For valid
 * storage, NULL owner/unavailable observations clear output and return 0. Available
 * measured zeros return 1. No allocation. Example: dust3d_worker_session_output_statistics(owner, &stats, sizeof(stats)).
 */
int dust3d_worker_session_output_statistics(const Dust3DWorkerSessionOutput* owner, Dust3DWorkerSessionStatistics* output, size_t output_size) DUST3D_WORKER_SESSION_NOEXCEPT;
/** Borrow opaque cache-only diagnostic bytes until owner destruction. Required size
 * clears on NULL/unavailable/empty; returns NULL when no bytes or size is NULL.
 * Bytes may contain NUL/non-UTF8. Never log/interpret them as commands or format text.
 */
const uint8_t* dust3d_worker_session_output_cache_cause(const Dust3DWorkerSessionOutput* owner, size_t* size) DUST3D_WORKER_SESSION_NOEXCEPT;
/** Admit one dedicated operation9 service response after transport cleanup.
 * @param collector Unused actual-EOF-framed service owner; queue-bound owners reject.
 * @param active Authoritative current host document/request/revision.
 * @param observed Independent authenticated connection generation and OS peer.
 * @param receipt Callback controls; never authoritative peer/current identity evidence.
 * @param limits Independent session semantic policies.
 * @param result Required self-owned typed receipt; NULL leaves attempt untouched.
 * @return Independent session owner or NULL. Non-NULL result consumes
 * this collector once, including on rejection. Host authenticates transport and supplies actual observations;
 * this function only correlates them. No I/O, native generation or cancellation.
 * Example: output = dust3d_worker_session_service_admit(collector, active, observed, receipt, limits, &result);
 */
Dust3DWorkerSessionOutput* dust3d_worker_session_service_admit(Dust3DWorkerReplyCollector* collector,
    Dust3DWorkerRequestIdentity active, Dust3DWorkerServiceObservation observed,
    Dust3DWorkerServiceReceipt receipt, Dust3DWorkerSessionOutputLimits limits, Dust3DWorkerDecodeResult* result) DUST3D_WORKER_SESSION_NOEXCEPT;
/** Admit through the queue's private active identity/generation and current source.
 * @param queue Externally serialized service coordinator, retained through return.
 * @param observed Independent authenticated transport lifetime and OS peer.
 * @param receipt Callback controls from that connection.
 * @param limits Independent session semantic policies.
 * @param result Required typed receipt; NULL leaves active attempt untouched.
 * @return Independent root or NULL. Matching collection releases its slot on success
 * or rejection while preserving newer waiting work. No authentication or I/O occurs.
 * Example: output = dust3d_worker_session_service_queue_admit(queue, observed, receipt, limits, &result);
 */
Dust3DWorkerSessionOutput* dust3d_worker_session_service_queue_admit(Dust3DWorkerServiceQueue* queue,
    Dust3DWorkerServiceObservation observed, Dust3DWorkerServiceReceipt receipt,
    Dust3DWorkerSessionOutputLimits limits, Dust3DWorkerDecodeResult* result) DUST3D_WORKER_SESSION_NOEXCEPT;
/** Destroy the root and all borrowed views after consumers copy needed data.
 * NULL harmless; caller keeps owner alive through all getters and never destroys
 * a borrowed primary/issue with dust3d_worker_decoded_destroy.
 */
void dust3d_worker_session_output_destroy(Dust3DWorkerSessionOutput* owner) DUST3D_WORKER_SESSION_NOEXCEPT;
#ifdef __cplusplus
}
#endif
#undef DUST3D_WORKER_SESSION_NOEXCEPT
#endif
