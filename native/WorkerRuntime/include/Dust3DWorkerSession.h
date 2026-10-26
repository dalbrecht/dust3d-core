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
/** Release owner/cache, accepting NULL. No concurrent calls or destruction.
 * Example: dust3d_worker_session_destroy(owner);
 */
void dust3d_worker_session_destroy(Dust3DWorkerSession* owner) DUST3D_WORKER_SESSION_LIFECYCLE_NOEXCEPT;
#ifdef __cplusplus
}
#endif
#undef DUST3D_WORKER_SESSION_LIFECYCLE_NOEXCEPT
#endif
