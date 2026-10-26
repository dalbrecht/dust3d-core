# ADR 0154: Worker-owned native generation session

Status: proposed for reviewed delivery. Tracks #251.

## Decision

Retain one RAII native generation session for one immutable host-selected document
identity. A mutex serializes generation, owned output copies, final-call statistics
and reset. Native engine calls retain their existing global serialization. Zero
cache allowance disables retention; the supported maximum is 256 MiB. Document,
request framing and output policies are admitted before generation. Session lifetime
must outlive callers; concurrent destruction is unsupported.

Reuse the existing checked indexed/source copy implementation with a borrowed
session owner instead of duplicating native adapters. Stateless entry points retain
their behavior. Source generation statuses 3/4 invoke ordinary indexed regeneration
inside the same worker session. Preserve the original source issue and any ordinary
failure independently. Status 5 represents successful emptiness. Returned storage
remains owned after subsequent generation or reset. Cache observations refer to the
final actual native call, including an ordinary fallback when one occurs.

Resolve outcomes through a portable pure policy with fake fallback callbacks.
Statistics are secondary: their original exception is retained as a process-local
exception_ptr without discarding geometry or manufacturing zero observations.
Consumers must inspect statisticsException; it is not a wire format or a C ABI.
Explicit mapped statistics failures are also supported by the pure result policy.
Wire/C transport must map original exception types and causes in a later stage.
No statistics exception or STL object crosses the existing C boundary.

## Verification

Both new contracts started red before their implementation. The full CMake build
and all 195 CTest contracts pass (33.88 s final run). SwiftPM Dust3DWorkerRuntime
build, strict manifest lint, quarantine guard and pinned clang-format 18 checks pass.
The ordinary licensed Capsule fixture demonstrates actual sequential native cache
reuse, document mismatch rejection before work, reset, and independently owned output.
Fake outcomes cover exact fallback selection, successful emptiness, both causes,
resource context, statistics failure isolation and measured-zero availability.

Scoped ASan/UBSan checks pass for the pure policy and the positive native session
with changed session/index-copy adapters instrumented. Inherited engine libraries
are uninstrumented; leak detection is disabled. Pure policy coverage is 36/36 lines,
25/25 regions, 2/2 functions and 16/16 branch outcomes. This is not inherited-engine
coverage. Native failure, concurrent destruction and allocation-failure paths were
not probed. No new malformed native inputs or crash/resource probes were run.

## Remaining adoption

Connection-scoped host lifecycle, request/revision correlation, protocol/C/Swift
ownership, platform decoding, production backend activation, supported range parity
and M4–M8 acceptance remain open. This does not close #251 or Apple #279/#280.
The Apple production pin and all quarantines are unchanged. Automated exact-head
review and all CI must complete before merge.
