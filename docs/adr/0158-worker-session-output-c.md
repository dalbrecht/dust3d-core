# ADR 0158: C ownership for session output values

Status: proposed for reviewed delivery. Tracks #251.

## Decision

Expose Dust3DWorkerSessionOutput as one immutable C owner for a codec-validated
SessionOutput body. The public header contains only C types and catches every
exception at the decode boundary. No input pointer survives decoding. Publication
waits for complete root/subview construction under RAII; no partial owner escapes.

The root moves geometry and original failures into optional inline decoded subowners,
reusing existing indexed/source/failure getters. Primary/source-issue/statistics-issue
views are borrowed const Dust3DWorkerDecodedReply pointers. Never destroy those views:
they expire with the root and const types prevent passing them directly to the
mutable decoded destroy function. Copy needed data before root destruction; no
concurrent destruction is supported. Empty output has no primary view.

Outcome and statistics availability are scalar copies. A sized statistics query
copies actual configured/retained cache bytes, component/part reuse/rebuild counts,
outcome and signed status. Valid output storage clears when observations are absent;
measured zero remains available and returns success. Incorrect size or NULL output
rejects without writes. Cache-only diagnostic bytes are a separate immutable borrowed
opaque span. NULL/empty getters clear counts; NULL destruction is harmless.

## Policies and diagnostics

Positive aggregate wire/decoded policies are at most 256 MiB. Source allowance zero
selects 64 MiB; explicit positive source allowance is at most 256 MiB and is tightened
by the aggregate decoded allowance. Existing indexed/source count and diagnostic
policies remain independent. Roots/scalars, capacity, temporary workspace and RSS
are excluded from logical accounting. This does not establish full Apple range parity.

Typed SessionOutput, statistics, indexed/source, source-hint, mesh and failure errors
retain original classifications/codes and bounded self-owned causes. Additive detail
sources 29/30 identify session-output/statistics codec errors. Existing C structs and
function layouts are unchanged; no stable distribution ABI claim is made. Allocation
and generic exception recovery preserve available causes without throwing across C.

Raw body decode is VALUES ONLY. It neither authenticates a peer nor correlates a
request, receipt, connection lifetime, EOF or current revision. Existing stateless
serving and legacy semantic admission still reject operation 9. Dedicated host/C
admission and serving must be implemented before Apple adopts this owner.

## Verification and limits

C++ ownership and independent C11 consumer contracts began red with the missing
header. All 202 ordinary CTest contracts pass (36.26 s), together with the full
CMake build, SwiftPM runtime product (0.80 s), strict manifest lint, quarantine guard
and pinned clang-format18. Eight source/build/test hashes match the final gate.

Tiny pure bodies cover sourced/ordinary/empty/failure views, independent storage
after input overwrite, const child views, opaque causes, sized outputs, explicit
availability/zero distinction, scalar policy rejection, null behavior and original
inner-codec errors. The C consumer constructs an independent empty wire body and
compiles the public header as C11. An ordinary licensed Capsule request demonstrates
actual cache reuse, source-corner/index agreement and copied values surviving both
native-session close and C owner destruction.

Scoped ASan/UBSan passes with the new C bridge instrumented. Matching coverage is
116/127 lines, 93/96 regions, 12/12 functions and 56/58 branch outcomes. Allocation,
standard/nonstandard recovery and diagnostic truncation guards remain unforced;
these boundary gaps are explicit exceptions to the adoption coverage target, not
claims of 95% line or whole-core coverage. Native engine libraries are uninstrumented
and leak detection is disabled. No new malformed native archive, allocation-failure,
crash, signal, cancellation, pressure or exhaustion probes were run.

## Remaining work

Dedicated authenticated service/queue admission, C native-session lifecycle/serving,
Swift/XPC consumption, platform decoding and Apple #280 range evidence remain open
under #251/#279/#280. Production backend/pin, all quarantines and the single shared
build cache are unchanged. M4–M8 acceptance remains incomplete. Completed clean
exact-head automated review and all CI are required before merge.
