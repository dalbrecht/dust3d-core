# ADR 0165: Host-admitted request preflight before body retention

Status: proposed for reviewed delivery. Tracks #251.

## Decision

Add an optional WorkerJobExpectation to the shared exact-frame reader and session
serving path. After validating the observed request header and recording its context,
validate the independent host expectation and compare operation/document/request/
revision before allocating or reading the body. Invalid expectation or mismatch
preserves the original ProtocolError(invalidIdentity), leaves body bytes unread and
writes no response. This is request correlation, not transport authentication or
current-parent publication admission.

Add dust3d_worker_session_serve_correlated with a copied host-admitted identity and
fixed operation9. It uses the existing C exception/receipt path and retained session
owner; all original channel/isolation/lifetime/resource obligations remain. Existing
session_serve remains available without a side-channel expectation, and ordinary
stateless behavior is unchanged. No existing C function signature/ABI is removed.
The Swift native adapter now requires an explicit expected identity in its job and
calls the correlated entry point; it never falls back to uncorrelated generation.

The forthcoming session RPC must derive this expectation from the authenticated
host's admitted metadata, not read it from the request channel. The bound session
still validates its independent document/lifetime/replay policy before native work,
and the parent still uses dedicated completion admission for current publication.

## Verification

Pure framing and C-call contracts started red on missing expectation/type/API.
Tiny protocol-only frames exercise exact matching, foreign document/request/revision,
wrong operation and invalid host identity; these bytes never reach native generation.
Descriptor offsets demonstrate rejection after exactly the64-byte header, before
body reading, and no output. The source check is placed before body allocation.
C-boundary cases use independently valid licensed Capsule requests with mismatched
host metadata, then successfully use the same first request identity, showing that
rejected framing does not consume native replay state. Matching correlated generation,
legacy uncorrelated cache reuse and reset/rebuild/owned output also pass. The C11
consumer imports/calls the additive boundary. No malformed native archive is added.

Full CMake build and209/209 ordinary CTest contracts pass (37.82s), including existing
healthy stateless XPC fixtures. SwiftPM runtime builds (1.31s), and the updated Swift
consumer links/runs against that actual archive. Strict Swift6 concurrency/warnings-
as-errors, formatting, manifest lint, guard and pinned clang-format18 pass. Ten
source/build/test hashes match final validation.

Fresh changed I/O/runtime objects pass scoped ASan/UBSan for pure protocol values and
ordinary Capsule C serving. Matching separate profiles each cover the shared reader
at20/24 regions and11/18 branch outcomes; their exercised outcomes differ. The new C
wrapper covers1/1 region. Existing I/O/recovery guards remain unforced; these are not
whole-runtime/core coverage counts. Engine libraries remain uninstrumented and leak
detection disabled. The old instrumentation subdirectory was absent after reboot,
so outputs were written directly into the existing shared cache root; no second
build folder. No new malformed-native-input, crash/fault/allocation-failure/signal,
broken-pipe/native-cancellation, pressure/exhaustion probes or new XPC scenarios.

## Remaining work

Wire explicit authenticated connection session RPC metadata to this correlated path
and establish initialized-cache XPC lifetime/cleanup evidence. Platform decoding,
complete authored/procedural/source-baked output and Apple #279/#280 supported-range
acceptance remain open. Production pin/backend and all quarantines are unchanged.
Completed clean exact-head automated review and ALL CI remain merge requirements.
