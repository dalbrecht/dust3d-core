# ADR 0118: C-compatible incremental parent reply owner

Status: prepared descriptor-host integration for #169; no production adoption.

## Decision

Add an opaque parent reply owner to the separate `Dust3DWorkerRuntime` C product.
It reuses `WorkerReplyCollector`, not a second Swift/parser implementation. The
caller supplies an exact independent expected request wire header and explicit
positive wire quotas. Creation copies policy/identity and reserves only the fixed
reply header. Bounded next-read sizing precedes collection; supplied byte spans
are copied after header correlation and quota checks. The owner performs no I/O,
native generation or process control, and the caller serializes all access.

Use separate reply status/result types so `READY` (successful operation before
EOF) and `FRAMED` (one complete frame plus supplied actual EOF) cannot be mistaken
for the worker runtime's complete-frame-written status. All C++ exceptions are
caught. Original protocol category/code/message bytes are returned inline with
explicit truncation, and allocation/other exceptions retain their runtime cause.
Any rejected modifying/sizing call makes a live owner permanently rejected.
Subsequent calls return a typed state error; callers retain the original receipt.

No frame view is exposed until successful EOF. Views are borrowed and expire on
modification/destruction; Swift copies while the owner lives. Rejection hides
previously framed views. Destroy is NULL-safe, but all non-NULL handles must be
live and destroyed exactly once. Input, output-count and receipt storage must be
valid independent caller-owned spans; raw C pointers cannot establish that
precondition dynamically. The C header exposes no STL, C++ owner or exception.
Existing job-result layouts and version-one fields remain unchanged; this is an
additive unreleased API, not a binary stability claim.

Framed bytes are not admitted geometry or a successful native operation. Empty
response/failure framing is possible independently of semantic validity. The
parent still needs semantic decoding, current identity/publication policy and
an authoritative completion contract. Never replace observed process exit with
an XPC callback. Persistent authenticated transactions require a separate contract.
No production pin, service, sandbox, signing or quarantine policy changes.

## Verification

A C11 consumer initially failed on missing types/functions. It now verifies
creation, bounded read sizing, hidden pre-EOF bytes, explicit EOF, borrowed view,
NULL-safe destruction and bounded diagnostics. New pure C++ fixtures check copied
lifetime after owner/input destruction, fragmented reads, original stale/quota/
trailing/truncation/state errors, sticky rejection, zeroed count outputs and
invalid expected headers/limits. No malformed native archive or OS failure probe
was introduced.

The strict Swift 6 consumer uses the actual ordinary Capsule reply, feeds bounded
chunks through the C collector away from its actor, copies Data before owner
destruction and transfers that owned value across an actor. Its independent
existing fixture checks still verify request/operation and 240 mesh corners. This
is importer/ownership evidence, not actual asynchronous XPC stream acceptance.

Fresh quarantine guard and registered-source audit precede a clean CMake build;
all 127 ordinary CTest contracts pass in 20.97 seconds. Scoped ASan/UBSan on the
pure C bridge/collector/protocol fixture pass. Bridge coverage is 101/112 lines,
57/61 regions and 30/32 branch outcomes. Defensive allocation/unknown-exception
and diagnostic nil/truncation cases remain unforced under the quarantine rules;
no whole-core coverage/containment claim follows. C11/C++ strict warnings,
formatting, Swift6 strict concurrency/warnings and diff checks pass.

SwiftPM runtime/helper builds pass (0.93/1.25 seconds), with disjoint explicit
source sets of twelve runtime and five CLI inputs. Ordinary base/resource and
UV verifiers pass against the rebuilt helper. The pre-existing native-build-system
deprecation warning remains. Runtime/test source hashes match the gate; only
README/plan/ADR/changelog documentation was added afterward.

Real descriptor reader lifecycle, semantic C admission, authenticated transaction
completion, the isolated XPC engine service, trusted packaging, containment and
reviewed Apple adoption remain open. All pins/quarantines remain unchanged; full
M4/M8 and editor parity are not complete.


## Serial delivery verification

Replayed after #196 with accepted aggregate text admission and checked pure
metadata fixture access preserved. C owner/header/C/C++/Swift sources match the
inspected saved stage. The historical runtime README hash predates saved-stage
documentation only; its current bytes match that stage. The current audited
inventory contains 128 ordinary contracts, adding the accepted pure quota test.
Fresh clean build, fifteen pure preflight checks (1.57 seconds), strict Swift
ownership pilot (0.26 seconds) and all 128 contracts pass (23.17 seconds).
The Swift consumer copies the actual healthy Capsule frame before owner
destruction and transfers independent Data across its actor.

Fresh pure C-owner/collector/protocol ASan/UBSan passes without engine/I/O/process
work. The previous scoped bridge coverage remains historical prepared-stage
evidence; defensive allocation/unknown-exception/formatter cases stay unforced.
No whole-engine instrumentation or OS failure-path guarantee follows.

SwiftPM runtime/helper products build in 0.78/1.50 seconds with disjoint twelve
runtime/five startup sources. Fresh healthy packaged-helper UV dispatch and
visual-family checks plus export compatibility pass. Strict formatting,
whitespace and recorded executable-source hashes pass. Existing native build-
system deprecation and duplicate resource-library warnings remain. No new native
invalid-input, failed-peer, cancellation, signal, crash, allocation-failure or
pressure pilot is introduced. Production pin, semantic/completion admission,
release/containment gates and every quarantine remain unchanged.
