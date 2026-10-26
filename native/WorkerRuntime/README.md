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


The development XPC host now uses a non-Sendable Swift collecting-thread adapter
around that C coordinator. File/main and socket/reader owners prepare requests
before transport submission; socket writer/main consumers receive copied request
values through the retained prepared event. Actual EOF and joined authenticated
OS-peer/control observations precede queue-current admission and copied geometry.
Four healthy file/socket base/UV fixtures verify that route. Pure Swift tests
exercise source replacement, retirement and independent data without native failure
work. This is prototype host integration, not reviewed/pinned editor adoption or
proof of resource/failed-peer containment. See
[ADR0133](../../docs/adr/0133-coordinated-xpc-reply-admission.md) and #188.

## Source-hint value codec

The internal C++ WorkerSourceHints codec measures and transfers owned identity,
corner and influence values with explicit endian layout and aggregate quotas.
It preserves aliases and inherited nonnegative weights without normalization.
Validation precedes decoded output arrays/text. This standalone value codec is
compiled by CMake and SwiftPM but is not yet an active worker operation or public
C ABI output. Geometry pairing, source-aware generation and Swift adoption remain
required under #213 and Apple #279. See
[ADR 0134](../../docs/adr/0134-worker-source-hint-values.md).

`WorkerSourcedMesh` pairs complete geometry and source values atomically. Shared
source inspection and validated mesh node counts enforce correspondence and one
aggregate logical/wire quota before retaining decoded output. It preserves inner
typed errors and never returns partial geometry after source rejection. This is
not an active operation, C ABI output or default editor backend. Indexed/material/
mip parity and authenticated publication remain required. See
[ADR 0135](../../docs/adr/0135-atomic-worker-source-geometry.md) and #215.

`WorkerSourceOperation` now generates a complete pair through the existing
serialized source C API. It retains one native owner until geometry, original
identity strings, corners and unnormalized influences have been copied. Source
preflight bounds both retained records/text and wire size; its charge reduces
the geometry allowance before either output copy. Complete codec validation
precedes return. Temporary encoding and native generation workspace are outside
these logical quotas. Known-positive fixture and bounded copy-policy tests do
not prove failed-engine containment. No source opcode, C output getter or default
editor backend is activated. See
[ADR 0136](../../docs/adr/0136-native-worker-source-generation.md) and #217.

## Source-aware runtime route

Operation 3 now carries a versioned opaque archive/source-budget request and
returns the complete geometry/source pair. `dust3d_worker_source_request_create`
owns framing without parsing the archive. Runtime dispatch and child/service/queue
admission retain source values; decoded C owners provide source counts, record
copies and borrowed original identity text. Copy text before destroying its owner.
Existing C layouts and numbers remain unchanged; new enums/functions are additive.
ABI version 1 is a layout version, not a source-feature negotiation flag. Use
matching pinned client/helper sources; older workers reject the unknown opcode.

Request source budget zero selects 64MiB, and a request can only tighten the
worker's configured source copy/native-retention allowance. Parent semantic quotas
remain independent and must reflect its own policy; a header alone does not prove
that a producer honored request options. Logical limits exclude workspace/RSS.
No Swift value decoder, Apple pin or default editor backend changes in this stage.
Indexed/material/mip parity and host/editor acceptance remain open under Apple #279.
See [ADR 0137](../../docs/adr/0137-source-aware-worker-runtime.md) and #219.

`WorkerIndexedMesh` now transfers an owned vertex table and exact UInt32 triangle
index order with source counts and warning text. It shares the expanded codec's
finite/unit/color/area/UTF8 validation. Complete frame, count, index and geometry
inspection precedes decoded arrays; no deduplication or normalization occurs in
the codec. This closes a wire-value prerequisite, not indexed/source runtime or
editor adoption. See [ADR 0138](../../docs/adr/0138-indexed-worker-geometry.md)
and #221.

`WorkerIndexedSources` now pairs indexed geometry and source hints with a
version-two pair prefix. Source corners correspond to triangle **indices**, not
unique vertex rows. Both full inner payloads, source counts and aggregate quotas
validate before decoded arrays. Original inner error categories survive rejection.

`WorkerIndexedSourceOperation` generates one source owner, derives its indexed
mesh from that same borrowed geometry and copies the complete pair. Shared source
inspection bounds records/text; a pure export-budget calculation reserves source,
warning, index and vertex allowances before indexing. Native lookup/generation
workspace and encoding temporaries are outside these logical quotas. Direct copy
callers must supply owners with the same-generation derivation; count agreement
alone does not authenticate provenance. No active opcode/C ABI/Apple pin changes.
Runtime/Swift adoption, material/mip/statistics parity and the supported-range
audit in Apple #280 remain open. See
[ADR 0139](../../docs/adr/0139-indexed-source-generation.md) and #223.

## Indexed runtime output

Operations 4 (indexed/source) and 5 (ordinary indexed) now route through request
construction, native generation, child/service/queue admission and owned C output.
Use `dust3d_worker_indexed_source_request_create` for source-budget requests, or
`dust3d_worker_request_create` with ordinary indexed operation and no UV options.
Decoded kinds 5/6 expose unique vertex counts, derived triangle counts and a
separate index count/range-copy API. Sources remain per index position. Existing
expanded, UV and source kinds/layouts remain supported.

The editor currently recovers from optional source status 3/4 by generating
ordinary indexed geometry and retaining the original source issue. Operation 5
provides that future worker capability; this stage does not implement a retry
controller or permit parent native fallback. No Apple pin/backend changes.
ABI version 1 remains a layout version, not feature negotiation; matching pinned
client/helper revisions are required. Payload-range (#280), Swift conversion,
materials/mips/statistics and source-aware XPC/editor acceptance remain open under
Apple #279. See [ADR 0140](../../docs/adr/0140-indexed-worker-runtime.md) and #225.

## Baked texture value transport

The internal portable `WorkerBakedTextures` codec preserves five map roles, exact
straight RGBA8 pixels, base-inclusive mip chains and the producer's chart-safe
sampling LOD. It inspects the entire packet in fixed stack storage before decoded
arrays. Mandatory color and optional maps, tight dimensions, halving shape,
1×1 termination, independent quotas and exact framing are validated. There is no
image decoding/filtering or inferred chart safety. See
[ADR 0141](../../docs/adr/0141-worker-baked-textures.md) and #227 for the schema and
measured pure validation coverage.

No C runtime alternative/opcode, native copying, geometry/triangle/material pairing
or Swift consumer is added here. Existing public operations remain unchanged. Full
rendering/statistics, supported-range (#280) and authenticated editor acceptance
remain required under Apple #279. No Apple pin/backend/quarantine change.

## Coherent source-baked output

The internal `SourceBakedPayload` pairs expanded retained geometry with its exact
packed UV/tangent/alpha-policy records, byte-exact model/component catalogs and
baked texture/mip values. Complete inner and aggregate inspection precedes decoded
arrays. `copyNativeSourceBakedMesh` preflights and copies all families from one
stable borrowed source-baked owner; no foreign geometry/source hints are paired
by count. Fixed borrowed image slots share the existing texture validation rules.
See [ADR 0142](../../docs/adr/0142-source-baked-worker-output.md) and #229 for the
wire layouts, ownership, scoped coverage and defensive-boundary limits.

There is no new C decoded kind/opcode, Swift consumer or Apple pin/backend change.
Imported authored samplers/factors/images and legacy charts remain distinct paths.
Native payload/session/cache metrics, range parity (#280), complete transport and
editor acceptance remain open under Apple #279. Logical quotas exclude native and
encoded-wire workspace, validation sets, capacity and process RSS.
