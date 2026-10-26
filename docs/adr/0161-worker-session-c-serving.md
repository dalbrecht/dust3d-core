# ADR 0161: Explicit retained-session channel serving

Status: proposed for reviewed delivery. Tracks #251.

## Decision

Expose an operation9 C request factory using the existing bounded source-request
builder and an explicit dust3d_worker_session_serve entry point using the retained
connection/document owner. Stateless serving continues to reject operation9.
Request creation owns a full frame independently of archive bytes and catches all
exceptions. Host generation remains an independent call argument, not wire metadata.

Extract the existing exact request-plus-EOF reader and response writer into shared
WorkerJobIO helpers. Both serving routes retain channel validation, bounded framing,
original I/O causes, context capture and no descriptor ownership. Session serving
calls generateSessionReply through its existing scope/replay/native-cache/output
policies, releases request payload before writing, and returns one complete envelope.
The C boundary reuses the existing job receipt/error path, preserving scope codes,
original native detail/truncation and session/statistics codec causes. COMPLETE means
complete response write, including valid primary-generation-failure envelopes; it
is not proof of mesh success, authentication, EOF observation by the parent or process exit.

The host serializes jobs/lifecycle, retains owner throughout calls, establishes
signal/resource/sandbox/stdout isolation and closes its channels afterward. This
API neither interrupts native work nor provides containment or parent fallback.
Parent publication still requires the dedicated completion admission gate. Configuration
and receipt values never authenticate the peer. No new dependency is introduced.

## Validation

The ordinary serving contract began red on missing factory/serve entry points.
Licensed Capsule requests run through separate temporary regular-file channels,
not signal-delivery/broken-pipe probes. A second request reports actual cache reuse;
reset then produces rebuilding. Owned decoded output remains usable after session
close/destruction. The independent C11 consumer checks additive declarations,
NULL-result request construction and NULL-owner serving rejection before I/O.

Full CMake build and206/206 ordinary CTest contracts pass (38.54s); SwiftPM runtime
builds (1.29s), strict manifest lint, quarantine guard and clang-format18 pass.
Eight source/build/test hashes match final validation inputs. No source changed
during the gate.

Fresh changed I/O/runtime/request objects pass scoped ASan/UBSan through the ordinary
Capsule path. Each of the new C serving/request wrappers and C++ session-serving
function covers its1/1 code region. Shared request reader covers13/17 regions and6/12
outcomes, writer2/3 regions and1/2 outcomes in this positive-only profile. Existing
legacy functional contracts also pass, but are not represented as matching scoped
coverage here. Error/recovery guards remain unforced; engine libraries are
uninstrumented and leak detection disabled. No whole-runtime/core coverage claim.
No new malformed native archive, fault/crash/signal/native-cancellation, pressure or
resource-exhaustion probes were run. No app/GPU acceptance is claimed.

## Remaining work

Swift/XPC owner lifetime and decoder integration, actual authenticated connection
acceptance and Apple #279/#280 full-output/range parity remain open. This explicit
session operation currently supplies indexed/source geometry and actual observations;
imported/authored, procedural and source-baked output parity remains a separate
adoption gate. Production pin/backend and all quarantines remain unchanged.
Completed clean exact-head automated review and ALL CI are required before merge.
