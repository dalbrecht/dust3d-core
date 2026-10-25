# ADR 0117: Shared incremental worker reply framing

Status: prepared integration stage for #168; no production adoption.

## Decision

Extract the existing Unix parent transport's header-first reply collection into
`WorkerReplyCollector`. It copies the expected request identity and wire limits,
borrows an initially empty caller-owned vector, and reserves only the fixed
64-byte header initially. The caller serializes access, retains the vector object
and does not mutate its storage while using the collector. Input spans must be
independent of that borrowed output.

Reads stop at the header boundary before body collection. Validate the declared
quota, response/failure kind, document, request, revision and operation before
retaining body bytes. Subsequent reads are capped by the caller's supported
chunk size and remaining body plus one trailing-byte probe. This is logical
retained-byte bounding, not an allocator-capacity/RSS or containment guarantee.
No whole advertised body is reserved in advance. Overlong body spans reject
before inserting that span; collected prefix bytes remain diagnostic-only.
Append errors and incomplete EOF reject permanently, preserving original errors.
Actual complete EOF is explicit and idempotent.

The existing Unix transport reuses the read sizing/append logic but continues to
return its actually observed process outcome and collected bytes. It does not
call `finish`: existing PendingReply admission retains process-outcome-first
error precedence and detects incomplete frames. No channel, spawn, cancellation,
cleanup or diagnostic ownership changes. A collector reference exists until
after child/channel cleanup and before attempt diagnostics move their buffers.

Framing is not semantic, process, authenticated transaction or publication
admission. An empty failure payload can be framed but is not thereby a valid
operational failure. A future descriptor/XPC parent must observe EOF and apply
the appropriate semantic and authoritative completion contracts. Never invent a
successful process exit from an XPC callback. The collector is also compiled in
the SwiftPM runtime source set; no new C API, host service or dependency is added.

## Verification

The focused test initially failed on the missing collector header. Pure byte
fixtures then cover fragmented/combined reads, caller chunk caps, empty failure
framing, separate failure/output quotas, all identity fields, request-kind
rejection, incomplete/complete EOF, trailing bytes, sticky errors and fail-fast
borrowed-size mutation. Rejected headers retain no body. Original typed cause
and copied-value lifetime are checked without native malformed-archive probes.

Fresh source quarantine guard and registered-source audit include 125 ordinary
contracts. A clean CMake build and all 125 CTest contracts pass (21.87 seconds).
Scoped AddressSanitizer/UndefinedBehaviorSanitizer on the pure collector/protocol
fixture pass. Measured collector coverage is 63/63 lines, 54/54 regions and 32/32
branch outcomes, not whole-core coverage or allocation-failure verification.
No crash, fault, signal, broken-pipe or resource-exhaustion probes were added.

SwiftPM runtime/helper builds pass (1.01/1.40 seconds); explicit source ownership
is disjoint: eleven runtime and five CLI sources. The actual rebuilt SwiftPM
helper passes ordinary base/resource-profile, UV and UV visual-family verifiers.
Strict C++ warnings, formatting, Swift manifest formatting and diff checks pass.
The existing SwiftPM native-build-system deprecation warning remains. Runtime
hashes are unchanged after the gate; documentation was added afterward.

Parent C ownership, authenticated transaction admission, async host lifecycle,
real isolated XPC engine service, trusted packaging, sandbox/containment and
reviewed production adoption remain open. All production pins and quarantines
remain unchanged; this stage does not complete M4/M8 or full editor parity.


## Serial delivery verification

Replayed after #195 with accepted aggregate text admission and checked pure
metadata fixture access preserved. Collector, transport and pure fixture source
match the inspected saved stage. The current audited inventory contains 126
ordinary contracts, adding the accepted pure quota regression to the prepared
stage's 125. Fresh clean build, thirteen pure preflight checks (1.40 seconds),
healthy UV dispatch pilot (0.61 seconds) and all 126 contracts pass (21.23 seconds).
Fresh pure collector/protocol ASan/UBSan passes without native or process work.
The earlier collector coverage above remains historical prepared-stage evidence;
this does not claim whole-engine instrumentation or OS failure-path coverage.

SwiftPM runtime/helper products build in 0.86/2.88 seconds with disjoint eleven
runtime/five startup sources. Fresh healthy UV dispatch and visual-family
verifiers pass against the packaged helper; export compatibility builds.
Strict formatting, whitespace and recorded executable-source hashes pass.
Existing native build-system deprecation and duplicate resource-library warnings
remain. No new native invalid input, failed-peer, cancellation, signal, crash,
allocation-failure or resource-pressure pilot is introduced. Production pin,
semantic/completion admission, release/containment gates and all quarantines
remain unchanged.
