# Dust3DWorkerRuntime

Private Unix worker-host C interface to the maintained descriptor job runtime.
Build with CMake or the `Dust3DWorkerRuntime` SwiftPM static product; the
`dust3d-worker` executable depends on the same runtime target. No additional
package or external dependency is introduced.

Import `Dust3DWorkerRuntime.h` from C or `Dust3DWorkerRuntime` from Swift. Check
`dust3d_worker_runtime_abi_version()`, then call `dust3d_worker_serve` on a non-UI
worker with separate borrowed readable/writable job descriptors. The host must
own startup resource/SIGPIPE/stdio isolation and channel cleanup, serialize jobs,
and discard partial output on noncomplete results. A complete frame can carry a
native failure; no process-exit or geometry-success assertion is implied.

Results own all fields and bounded diagnostic bytes inline. C++ exceptions never
cross the C boundary; original category/code/errno/context remain available.
No native object owner or result destroy function is needed. This interface does
not launch, authenticate, sandbox, cancel, save or alter global stream policy.
See the public header for exact field and lifetime contracts. Tests use ordinary
licensed static fixtures and small pre-generation codec/preflight rejections;
all existing quarantines remain in place. Binary ABI stability is not claimed
before the project's release gates; consume an exact reviewed source revision.

## Parent reply collection

Create a `Dust3DWorkerReplyCollector` from the exact pending request wire header
and explicit wire quotas. Before each descriptor read, use
`dust3d_worker_reply_next_read`; append only the independently owned readable
bytes received, then call `dust3d_worker_reply_finish` only on actual EOF. Errors
are sticky and own their original bounded diagnostic. No OS I/O is performed.

`dust3d_worker_reply_bytes` exposes a borrowed header+body only after successful
EOF. Copy it before modifying/destroying the owner; destroy exactly once. Framed
bytes still require semantic and authoritative process/transaction completion
admission. No XPC callback can stand in for observed process exit. See
[ADR 0118](../../docs/adr/0118-parent-reply-c-boundary.md) and the public header
for exact lifetimes, NULL handling, quotas and ownership examples.

## Semantic reply values

After actual EOF, call `dust3d_worker_reply_decode` once with an explicit aggregate
logical decoded-byte budget (positive, at most 128 MiB). It consumes the framed
owner and reuses the maintained codecs to return an independent immutable base
mesh, paired geometry/UV or native-failure owner. Copy vertices/UV records and
borrowed text/raw cause bytes before destroying the semantic owner. Destroy the
framed owner separately; neither owner depends on the other's lifetime afterward.

Native failure is a valid decoded result, distinguished by summary kind/status.
Codec/protocol failures retain original diagnostic category/code/cause and remain
retired. Do not use semantic success as process, authenticated transaction or
publication admission. See [ADR 0119](../../docs/adr/0119-semantic-worker-reply-c-boundary.md)
and the public header for range, lifetime, quota and NULL contracts.

## Request encoding

Use `dust3d_worker_request_create` for a copied parent identity, nonempty borrowed
archive and base/UV options. The shared codecs validate quotas and UV/image values
before retained body encoding. No native archive is parsed. Copy the full frame
for transport and exact expected header for parent collection before destroying
the request owner. The input archive/options may be released after creation.
Original typed failures remain in the self-owned result. See
[ADR0121](../../docs/adr/0121-c-worker-request-encoding.md) and the public header.

Use `dust3d_worker_service_reply_create` before service submission to capture the
expected connection generation. Feed actual framing bytes/EOF, then call
`dust3d_worker_service_admit` with authoritative current identity, observed peer
and control receipt. Copy decoded values before destroying owners. Ordinary
semantic decode rejects service collectors. Receipt opcodes are raw integers
validated before enum conversion. Neither API authenticates its caller inputs or
publishes app state; real XPC/Apple adoption remains pending. See
[ADR0129](../../docs/adr/0129-service-admission-c-boundary.md) and #180.


## Coordinated service requests

`dust3d_worker_service_queue_create` owns the existing bounded RequestQueue. Submit
copied exact requests, begin with a trusted nonzero connection generation, and
borrow outbound bytes/collector for actual transport and EOF framing. Submit,
cancel and close preserve active transport views; admission, abandonment and
destruction invalidate them. Never destroy the borrowed collector. It cannot use
standalone semantic/service admission to bypass queue current-source validation.

After transport cleanup, call `dust3d_worker_service_queue_admit` with actual
connection/OS-peer observations and scalar control receipt. The queue supplies
its own current source and captured request/generation. Copy decoded data before
destroying its independent owner. A required result consumes active work once;
NULL result leaves it untouched. Preserve original framing/transport causes and
abandon matching work after cleanup. Newer waiting work survives rejection.
Cancellation/close retire publication without stopping native work. Call all
owner operations on one serialized caller, away from blocking I/O. No editor
adoption, authentication or process-memory guarantee follows. See
[ADR0132](../../docs/adr/0132-c-service-request-coordinator.md), #183 and the public
header for exact lifetimes, quotas, error categories and current test limits.
