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

## Legacy procedural baked output

`LegacyBakedPayload` retains the existing procedural geometry, full UV
charts/gradients/associations, per-triangle tangent frames and optional base maps.
It validates complete inner spans, mapping correspondence and aggregate quotas
before decoded arrays. `copyNativeLegacyBakedMesh` preflights and copies one
stable borrowed baked owner. Shared native geometry/UV inspection and scalar
validators avoid maintaining a second set of copy rules; source-baked output keeps
its own metadata contract. See [ADR 0143](../../docs/adr/0143-legacy-baked-worker-output.md)
and #231 for schemas, ownership, measured coverage and defensive-boundary limits.

No request/C/Swift activation or Apple pin/backend change is included. Imported
samplers/factors/images, real metrics, range parity (#280), complete transport and
editor acceptance remain open under Apple #279. Logical quotas exclude roots,
capacity, validation/wire/native workspace and process RSS.

## Authored imported material facts

`ImportedMaterialsPayload` preserves original PBR factors, alpha policy, optional
samplers/filter presence, GL wrapping, declared PNG/JPEG bytes and part/component
bindings. Owned/wire/native readers share complete scalar/key/reference/storage
and quota validation before retained arrays or encoded-image copies. Native copying
uses one stable imported-assets owner and never generates geometry or decodes
pixels. See [ADR 0144](../../docs/adr/0144-imported-material-worker-facts.md) and
#233 for layouts, ownership and measured pure/native boundary coverage.

A declaration does not prove valid pixels. Full authored surface/decoded-image
pairing, real metrics, range parity (#280), request/C/Swift transport and editor
acceptance remain open under Apple #279. No Apple pin/backend or quarantine change.

### Authored imported surface values

`WorkerImportedSurface.h` encodes/inspects/decodes owned geometry and authored
UV/color/catalog records. `WorkerImportedSurfaceCopy.h` copies one stable native
surface owner after complete preflight. Repeating UVs and numerical RGB encoding
are preserved without conversion. No opcode or Swift consumer is added here.
See [ADR 0145](../../docs/adr/0145-imported-surface-worker-output.md) for layout,
quotas, lifetime, measured verification and remaining adoption gates.

### Joined imported rendering values

`WorkerImportedRender.h` preserves complete source geometry, surface-ordered
material definitions (including encoded bytes) and optional owned straight RGBA8
pixels. Inspect validates inner schemas, exact identities, image presence and all
aggregate quotas before decoded arrays. The codec does not decode images or prove
archive provenance. See [ADR 0146](../../docs/adr/0146-imported-render-worker-values.md)
for layout, charges, tests and producer/transport/Apple adoption gates.

### Imported render preparation

`WorkerImportedProducer.h` selects surface-order definitions and invokes an explicit
quota-aware `ImportedRenderImageDecoder`. `WorkerImportedProducerNative.h` loads
facts and surface from the same stable unchanged archive and releases native owners
before decoding. The platform implementation belongs in the eventual Apple worker.
See [ADR 0147](../../docs/adr/0147-imported-render-producer.md) for contracts,
layered limits, measured verification and remaining adoption gates.

### Platform decoder ownership bridge

`Dust3DWorkerImageDecoder.h` exposes callback-scoped encoded input, opaque owned
straight RGBA8 images and self-owned bounded original diagnostics.
`WorkerImageDecoder.h` adapts that callback to the portable producer and transfers
returned pixels safely. No worker opcode or Apple decoder is registered here.
See [ADR 0148](../../docs/adr/0148-worker-image-decoder-ownership.md) for lifetime,
quotas, caller contracts and measured verification limits.

## Legacy baked rendering

Use `dust3d_worker_legacy_request_create` for opcode 7 with explicit optional
`Dust3DWorkerRequestLegacyOptions`. Byte budgets only tighten receiver policy;
dimensions/grid select native settings within separate host work caps. The ordinary
factory can create default legacy requests with no UV options. No archive parsing
or generation occurs in either factory.

Inside the worker, `dust3d_worker_serve_with_decoders` distinguishes imported and
procedural callback slots. The procedural callback must preserve the legacy PNG
transparent-grayscale policy. An all-zero slot is unavailable. No provider is needed
for a legacy archive without referenced images; an unavailable required provider
returns an original failure frame. Copy/decode owners and channel duties follow the
same admission/cleanup contracts above.

A `LEGACY_BAKED` decoded owner supports existing vertex/UV/chart/gradient/component
getters plus `dust3d_worker_decoded_baked_summary`, tangent, level-count and texture
getters. Copy all borrowed map bytes before owner destruction. These counts describe
actual retained pixels and tangent records, not native cache/session metrics. See
[ADR 0151](../../docs/adr/0151-legacy-bake-worker-runtime.md) for measured verification,
remaining native error coverage and open Apple/range/acceptance gates.

Source-baked rendering uses opcode8 and decoded kind9. Dedicated source-bake
request creation retains complete settings/archive framing. Source surface/catalog
and existing baked texture/tangent getters return one coherent owner's values;
borrowed spans must be copied before release. Contextual image failures retain
original cause bytes separately from role/identity. See
[ADR 0152](../../docs/adr/0152-source-bake-worker-runtime.md) for wire layout,
validation status and adoption limits.

## Session output value ownership

Include `Dust3DWorkerSessionOutput.h` to decode a complete session-envelope body
into one immutable owner. Primary/source/statistics issue views are borrowed const
decoded handles: use existing getters, copy needed data, and destroy only the root
with `dust3d_worker_session_output_destroy`. Actual statistics availability is explicit;
zero observations remain distinguishable from absent values. Cache diagnostics are
opaque borrowed bytes. Sized/null getter behavior is documented in the header.

This API only decodes values. It does not authenticate a peer, correlate current
work or authorize publication. Legacy operation-9 serving/admission remains rejected
until dedicated host ownership and admission are implemented. Source allowance zero
uses 64 MiB; independent inner count/diagnostic limits still apply. See
[ADR 0158](../../docs/adr/0158-worker-session-output-c.md) for quotas, verification
and unforced recovery-guard coverage.

## Correlated session service admission

Use `dust3d_worker_session_service_admit` with an unused EOF-framed service collector,
authoritative current identity, independent connection/peer observations and the
callback receipt. Use `dust3d_worker_session_service_queue_admit` for queue-bound
collectors; current identity and captured lifetime then come from the coordinator's
private active slot. A null result pointer leaves the attempt untouched. Matching
attempts are single-use even on rejection, and queue cleanup retains newer waiting
work. Preserve original framing receipts and abandon after transport cleanup.

The host still authenticates the connection and provides actual observed peer/EOF;
these APIs correlate that evidence and never control a process. Legacy generic
admission remains unsuitable for session envelopes, and native C session serving
and request factories remain pending. See
[ADR 0159](../../docs/adr/0159-worker-session-admission.md) for gate rules, lifetime
contracts and scoped verification limits.

### Connection-scoped C session ownership

Include `Dust3DWorkerSession.h`. Fill `Dust3DWorkerSessionConfiguration` from the
host-authenticated document and connection lifetime, then call
`dust3d_worker_session_create(configuration, &result)`. Zero cache allowance disables
retention; the256MiB ceiling counts logical retained payload only. Reset validates
the live generation and preserves replay ordering; close permanently releases cache
and is idempotent. Destroy the owner after all calls finish. No concurrent destruction.
Required result pointers preserve original typed errors; NULL performs no work.
This owner has no C channel-serving method yet and does not authenticate peers,
interrupt generation or activate an Apple backend. See [ADR0160](../../docs/adr/0160-worker-session-c-lifecycle.md).

### Retained-session job channels

Use `dust3d_worker_session_request_create` for an owned operation9 request frame,
then `dust3d_worker_session_serve` on a live session with an independent trusted
connection generation. The host owns separate channels, serializes jobs/lifecycle,
establishes resource/signal/sandbox/stdout policy and closes channels after return.
COMPLETE means a full envelope was written, including valid generation failures.
Inspect output outcome and complete the dedicated current-publication admission
before displaying anything. Cache state persists across calls until reset/close;
reset does not reset replay protection. Stateless serving still rejects operation9.
Actual authenticated XPC and full Apple output/range acceptance remain pending.
See [ADR0161](../../docs/adr/0161-worker-session-c-serving.md).

### Swift module and ownership evidence

The checked-in `include/module.modulemap` explicitly exports runtime, image,
session-output and session-owner headers; SwiftPM uses this instead of an incomplete
automatic target-named umbrella. `worker_session_swift` checks Swift6 strict
concurrency and warnings-as-errors, both module-import paths, ordinary retained-cache
serving and copied Sendable values after native destruction. Its same-process peer
observations exercise correlation, not XPC authentication. The same consumer was
also linked/run against the actual SwiftPM static archive. See
[ADR0162](../../docs/adr/0162-worker-session-swift-consumer.md).

### Lazy Swift connection owner

`tests/swift/WorkerServiceSession.swift` coordinates a synchronous backend under
one lock: immutable first scope, lazy creation, matching reset, permanent close and
final destruction. Jobs/receipts are Sendable and opaque handles stay private.
`WorkerNativeServiceSession.swift` adapts existing C calls and copies original
opaque diagnostics. The ordinary Swift consumer uses both; fake-only lifecycle
contracts cover failures without inducing native faults. The XPC fixture now uses this coordinator for explicit session RPCs. See
[ADR0163](../../docs/adr/0163-worker-service-session-lifetime.md).

### Retirement ordering

The internal Swift scheduler accepts `beforeRetired` cleanup and passes its original
`Result` to the retirement observer. Cleanup is queued after accepted work, runs
outside the admission lock and is invoked once only for the owning connection.
The XPC fixture closes its lazy session coordinator before process retirement and
logs only non-content diagnostic metadata on failure. Its legacy serving route stays stateless; explicit session RPCs use the retained owner. See
[ADR0164](../../docs/adr/0164-worker-session-retirement.md).

### Correlated session serving

Use `dust3d_worker_session_serve_correlated(owner, expected, generation, input, output, limits)`
when the host has independently admitted a document/request/revision. Header mismatch
or invalid expectation rejects before body retention/read and output, preserving the
observed valid header context and typed error. The older `session_serve` remains for
callers without a side-channel declaration; the Swift prototype adapter requires one
and uses the correlated entry point. Neither route authenticates transport or replaces
current-parent completion admission. See
[ADR0165](../../docs/adr/0165-worker-host-request-preflight.md).


### Healthy retained-session XPC integration

The development fixture exposes session scope and independent admitted identity as
RPC metadata, retains both received FileHandle owners through queued execution,
and returns bounded original diagnostic bytes. Parent admission still requires
actual EOF, current queue identity and independently observed OS peer PID. A healthy
Capsule fixture sends two requests over one authenticated sandboxed connection,
observes actual cache reuse, closes before invalidation and retains copied geometry
after retirement. This is development integration evidence, not Apple app adoption,
full output/range acceptance or a production signing/distribution guarantee. See
[ADR0166](../../docs/adr/0166-worker-session-rpc.md).


The explicit `resetSession` fixture RPC shares ordered admission and immutable scope
with generation. A healthy same-peer sequence observes reuse, reset, real rebuilding
and unchanged copied geometry before close; see
[ADR0167](../../docs/adr/0167-worker-session-rpc-reset.md). Reset never initializes an
absent session and does not authorize native interruption or production app adoption.


### Healthy procedural PNG XPC path

The development service now supplies only the procedural Apple decoder slot through
`dust3d_worker_serve_with_decoders`; imported configuration stays empty. The licensed
ProceduralImage fixture returns complete8-pixel legacy baked output through actual
EOF/peer/queue admission, with copied pixels valid after retirement. See
[ADR0169](../../docs/adr/0169-worker-procedural-image-xpc.md). This does not activate
an app backend, enable imported/GLB decoding or establish full pixel/range parity.


### Native preview range accounting

`planPreviewTransfer` measures trusted actual scalar observations against independent
native indexed/source policies and exact existing wire/retained layouts. It can expose
range gaps without allocating large data. Explicit positive indexed policies are not
capped at the native default. It does not authenticate or inspect payload semantics,
change active caps, transfer chunks or enable app adoption. See
[ADR0170](../../docs/adr/0170-worker-preview-range-accounting.md) for measured evidence
and remaining segmented transport/admission work under #270 and Apple #280.


### Bounded preview chunk framing

The internal D3PC header/state API separates whole-transfer bounds from canonical
payload chunks of at most64KiB. Physical accounting includes begin/data/end headers;
identity, offsets and real terminal-plus-EOF state are checked without retaining body
bytes or allocating a full transfer. See [ADR0171](../../docs/adr/0171-worker-preview-chunk-framing.md).
No actual byte transport, native dispatch or semantic/app admission is enabled here.


### Incremental preview byte endpoints

The internal reader/writer now handle arbitrarily fragmented bytes using one frozen
chunk workspace and fixed header storage. Original callback/decode failures seal the
owner; borrowed callback spans expire on return. Writer completion is not physical
EOF, and reader framing is not semantic/peer/current-source admission. See
[ADR0172](../../docs/adr/0172-worker-preview-byte-stream.md). OS channels, complete
wide bodies, C/Swift ownership and native/app dispatch remain separate reviewed work.
