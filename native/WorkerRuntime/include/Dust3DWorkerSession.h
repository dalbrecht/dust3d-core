/* SPDX-License-Identifier: MIT */
#ifndef DUST3D_WORKER_SESSION_H_
#define DUST3D_WORKER_SESSION_H_
#include "Dust3DWorkerSessionOutput.h"
#ifdef __cplusplus
extern "C" {
#define DUST3D_WORKER_SESSION_LIFECYCLE_NOEXCEPT noexcept
#else
#define DUST3D_WORKER_SESSION_LIFECYCLE_NOEXCEPT
#endif
/** Exclusive connection/document cache. Caller retains through calls and serializes
 * lifetime changes; concurrent destruction is forbidden. No transport authentication.
 */
typedef struct Dust3DWorkerSession Dust3DWorkerSession;
/** Host-selected authenticated scope, never copied from untrusted request/receipt.
 * Document must be nonzero; generation must be positive. Cache quota is logical
 * retained payload, at most256MiB; zero disables retention. It is not an RSS limit.
 */
typedef struct Dust3DWorkerSessionConfiguration {
    uint8_t document[16];
    uint64_t connection_generation;
    uint64_t maximum_cache_payload_bytes;
} Dust3DWorkerSessionConfiguration;
/** Create one independent native cache after validating host scope and policy.
 * @param configuration Immutable host-selected scope and cache allowance.
 * @param result Required self-owned bounded typed error; NULL performs no work.
 * @return Exclusive owner on OK, NULL on error. All exceptions are caught.
 * Destroy with session_destroy. No generation, I/O, global policy or app activation.
 * Example: owner = dust3d_worker_session_create(configuration, &result);
 */
Dust3DWorkerSession* dust3d_worker_session_create(Dust3DWorkerSessionConfiguration configuration, Dust3DWorkerDecodeResult* result) DUST3D_WORKER_SESSION_LIFECYCLE_NOEXCEPT;
/** Reset live retained cache without resetting request replay protection.
 * @param owner Live exclusive session, borrowed through return; NULL rejects.
 * @param connection_generation Independently established host connection lifetime.
 * @param result Required typed status/cause; NULL leaves state untouched.
 * @return 1 on OK, 0 on rejection. Closed/wrong lifetime rejects; all exceptions caught.
 * Example: dust3d_worker_session_reset(owner, generation, &result);
 */
int dust3d_worker_session_reset(Dust3DWorkerSession* owner, uint64_t connection_generation, Dust3DWorkerDecodeResult* result) DUST3D_WORKER_SESSION_LIFECYCLE_NOEXCEPT;
/** Permanently close admission and release cache after serialized work finishes.
 * @param owner Exclusive owner, still allocated after close; NULL rejects.
 * @param result Required typed status/cause; NULL leaves state untouched.
 * @return 1 on OK (including repeated close), 0 on error. Does not interrupt native
 * execution or close host channels. All exceptions caught.
 * Example: dust3d_worker_session_close(owner, &result);
 */
int dust3d_worker_session_close(Dust3DWorkerSession* owner, Dust3DWorkerDecodeResult* result) DUST3D_WORKER_SESSION_LIFECYCLE_NOEXCEPT;
/** Create an owned operation9 source-budget request; no native generation.
 * Same stable byte ownership and archive/source/wire quotas as indexed_source_request_create.
 * @param identity Exact document/request/revision; request must increase in session.
 * @param archive Stable archive bytes borrowed through return, copied into owner.
 * @param size Exact readable archive byte count.
 * @param maximum_source_bytes Zero selects64MiB; explicit policy only tightens host allowance.
 * @param limits Supported frame quotas; body includes source-request prefix.
 * @param result Required typed status; NULL performs no work.
 * @return Owner on OK, NULL on rejection. Destroy with worker_request_destroy.
 * Example: request = dust3d_worker_session_request_create(id, bytes, size, 0, wire, &result);
 */
Dust3DWorkerRequest* dust3d_worker_session_request_create(Dust3DWorkerRequestIdentity identity,
    const uint8_t* archive, size_t size, uint64_t maximum_source_bytes, Dust3DWorkerWireLimits limits, Dust3DWorkerRequestResult* result) DUST3D_WORKER_SESSION_LIFECYCLE_NOEXCEPT;
/** Serve one exact operation9 request followed by EOF through this retained session.
 * @param owner Live worker-owned session scoped from host authentication; NULL rejects.
 * @param connection_generation Independent host connection lifetime.
 * @param input_descriptor Separate borrowed readable request channel.
 * @param output_descriptor Borrowed writable reply channel.
 * @param limits Supported output policies; logical quotas do not cap RSS.
 * @return Self-owned COMPLETE receipt after complete response write, including valid
 * generation-failure envelopes. Other statuses preserve original causes; discard partial
 * output. Host serializes jobs/lifecycle, retains owner, establishes signal/resource/
 * sandbox/stdout isolation, closes channels after return and admits current publication.
 * No authentication, process containment, cancellation or parent fallback occurs here.
 * All C++ exceptions are caught. Call off UI. Legacy stateless serve still rejects9.
 * Example: receipt = dust3d_worker_session_serve(owner, generation, input, output, limits);
 */
Dust3DWorkerJobResult dust3d_worker_session_serve(Dust3DWorkerSession* owner, uint64_t connection_generation,
    int32_t input_descriptor, int32_t output_descriptor, Dust3DWorkerSessionOutputLimits limits) DUST3D_WORKER_SESSION_LIFECYCLE_NOEXCEPT;
/** Release owner/cache, accepting NULL. No concurrent calls or destruction.
 * Example: dust3d_worker_session_destroy(owner);
 */
void dust3d_worker_session_destroy(Dust3DWorkerSession* owner) DUST3D_WORKER_SESSION_LIFECYCLE_NOEXCEPT;
#ifdef __cplusplus
}
#endif
#undef DUST3D_WORKER_SESSION_LIFECYCLE_NOEXCEPT
#endif
