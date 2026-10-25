# ADR 0119: Semantic worker reply values across C

Status: prepared Swift-host integration for #170; no production adoption.

## Decision

Add a one-shot semantic decoder to the separate worker runtime C product. It
accepts a live successfully EOF-framed owner, uses its copied expected header
and wire policy, and retires it before semantic validation/allocation. A rejected
completion cannot be replayed with a larger quota. NULL receipt preflight does
not consume. Subsequent stream modification or repeated semantic completion
rejects, while a previously returned independent semantic owner remains valid.

Reuse the maintained base mesh, paired geometry/UV and failure codecs. Read the
body directly from retained frame storage, without another wire-body copy. The
caller supplies a positive aggregate logical decoded-byte quota up to 128 MiB;
standard codec count/text/field policies also apply. Both geometry and UVs are
validated and aggregate charged before retained paired arrays/text are built.
Failure quotas count retained raw cause bytes. Roots, capacities, allocator
metadata, temporary validation workspace and caller copies are excluded; no
RSS or process containment guarantee is implied.

Publish one immutable opaque semantic owner with a distinct base/UV/native-failure
kind. Bounded range copies of vertex fields and per-record UV/chart/gradient
copies expose C values, never native C++ layouts or STL. Summary scalars preserve
source counts and atlas dimensions. Borrowed warning/component bytes and opaque
failure causes remain valid until semantic-owner destruction; Swift copies first.
Original component spellings, binary64 UV/visual values, unknown associations and
native diagnostic bytes remain intact. Diagnostics are data, never format strings
or commands, and are not logged. All new sources remain MIT.

Separate semantic status/receipt types distinguish a valid native failure from
codec/protocol rejection. Original typed sources/codes/messages cross C, and all
C++ exceptions are caught. Defensive runtime allocation/unknown errors retain
their available cause. Valid getters perform no allocation or engine operation.
C callers must provide live handles and valid independent writable output spans;
range checks cannot prove arbitrary pointer storage. Destroy each owner exactly
once, NULL safely. Existing C result layouts/version-one fields are unchanged;
this is an additive unreleased API, not a binary stability claim.

Semantic decoding does not establish authoritative current-document policy,
process exit, authenticated transaction completion, producer provenance or
publication admission. A service callback must never fabricate a successful
process outcome. There is no native generation, I/O, sandbox, launch, signing,
production dependency or quarantine change in this stage.

## Verification

The C11 fixture initially failed on the missing semantic API. Its independent
hand-built failure frame now preserves a known wire status and raw FF/NUL cause
after framed-owner destruction. Pure C++ fixtures independently check triangle
positions/normals/colors, source counts, UTF-8 warning/component text, exact UV
corners, braced image identity, chart scalar/color/rotation/association fields and
gradient endpoints/resolution. Source/framed owners are destroyed or overwritten
before reading semantic values. Invalid copy ranges perform no writes; invalid
record accesses clear outputs. NULL and wrong-kind accessors are checked.

Exact/one-byte-under mesh and paired decoded quotas accept/reject as intended.
Small malformed mesh/UV/failure codec values reject atomically with the original
source/code/cause; failures and successful decoding cannot replay. No malformed
native archive, crash, allocation-failure, signal, broken-pipe or resource-
exhaustion probe is used. Scoped pure bridge/collector/codec ASan/UBSan pass.
Measured new semantic bridge coverage is 202/213 lines, 138/142 regions and 88/90
branch outcomes; defensive runtime/diagnostic guards remain unforced. This is not
whole-core coverage or exceptional OS/engine containment evidence.

Fresh quarantine guard and registered-source audit precede a clean CMake build.
All 129 ordinary contracts pass in 21.40 seconds. The strict Swift 6 consumer feeds
the actual ordinary Capsule reply, copies 240 validated vertices (2400 scalar
values), metadata, warning and framed bytes before destroying both C owners,
then checks the independent Sendable value across an actor. No raw C owner crosses
that actor boundary. This is importer/ownership evidence, not XPC stream lifecycle
acceptance or full rendered parity.

SwiftPM runtime/helper builds pass in 1.08/1.29 seconds with disjoint explicit
source sets (thirteen runtime, five CLI). Ordinary base/resource-profile and UV
verifiers pass against the rebuilt helper. Strict C11/C++ warnings, C++/Swift
formatting, Swift6 strict concurrency/warnings and diff checks pass. The existing
native-build-system deprecation warning remains. Gated runtime/test/build hashes
remain unchanged after documentation-only README/plan/ADR/changelog updates.

Authenticated transaction admission, actual descriptor reader lifecycle, isolated
XPC engine service, trusted packaging, containment and reviewed Apple adoption
remain open. All pins/quarantines remain unchanged; full editor M4/M8 acceptance
and parity are not complete.


## Serial delivery verification

Replayed after #197 with accepted aggregate text admission and checked pure
metadata fixture access preserved. Semantic/collector/header/C/C++/Swift sources
match the inspected saved stage; the historical runtime README hash predates
saved-stage documentation only. The current audited inventory contains 130
ordinary contracts, adding the accepted pure text-quota regression. Fresh clean
build, seventeen pure preflight checks (1.74 seconds), strict Swift semantic-copy
pilot (0.28 seconds) and all 130 contracts pass (21.55 seconds). The Swift consumer
copies the healthy Capsule's 240 validated vertices and metadata before destroying
both native owners, then transfers the independent Sendable value across an actor.

Fresh pure semantic-owner/collector/codecs ASan/UBSan passes without engine,
I/O or process work. Previous semantic-bridge coverage remains historical
prepared-stage evidence; defensive runtime/diagnostic paths remain unforced.
No whole-engine instrumentation or OS failure-path guarantee follows.

SwiftPM runtime/helper products build in 0.98/1.55 seconds with disjoint thirteen
runtime/five startup sources. Fresh healthy packaged-helper UV dispatch and
visual-family checks plus export compatibility pass. Strict formatting,
whitespace and recorded executable-source hashes pass. Existing native build-
system deprecation and duplicate resource-library warnings remain. No new native
invalid-input, failed-peer, cancellation, signal, crash, allocation-failure or
pressure pilot is introduced. Production pin, authenticated/current-source
completion, release/containment gates and all quarantines remain unchanged.
