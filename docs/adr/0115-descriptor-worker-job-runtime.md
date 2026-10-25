# ADR 0115: Reuse descriptor-based worker job I/O

Status: prepared runtime extraction for #166; no XPC service or production adoption.

## Decision

Keep document/reply payload bytes in bounded framed descriptor streams. A
Foundation object-message callback receives its Data before application byte-count
validation; an admitted-output quota there would not establish pre-allocation
containment. The future XPC control plane can transfer job channel handles and
completion metadata while the shared native runtime owns frame parsing/dispatch.
This decision does not itself implement that transport or claim hard RSS limits.

Extract the existing helper single-job reader/dispatcher/writer into `WorkerJobIO`.
Both CMake and SwiftPM helpers call it. Validate the existing fixed header and
input quota before allocating the declared body, read exactly that body, require
EOF, dispatch the unchanged base/UV operations and write one complete encoded
reply. Input storage is released before reply framing. The function borrows
separate channels without opening paths, closing descriptors or changing global
stdio/signal/resource policy. The host retains those responsibilities and closes
job channels after return/throw. I/O is blocking and must not run on a UI actor.
No retry, cancellation, deadline, sandbox or kernel-blocking guarantee is added.

Preserve actual syscall causes in a typed input/output error without dynamic
members. Throwing still uses the C++ exception runtime; this is not an allocation-
failure guarantee. Context resets before input, then retains the validated request
and operation. CLI diagnostics report the actual operation, or unknown before a
valid request, fixing the previous hardcoded base-mesh label for UV jobs.
Secondary diagnostic write failure remains best effort and cannot replace the
primary result. Existing exit/status, framing, stdout isolation and native failure
schemas remain unchanged.

C++17 and existing formatting/lifetimes remain authoritative. This is private
Unix C++ worker infrastructure, not a new Swift-facing C ABI or a second Apple
engine adapter. No dependencies, production pin or quarantine changes.

## Evidence

A missing-header red test preceded implementation. The fresh regular-file-channel
contract serves licensed Capsule base and UV jobs, validates exact identity and
240 corners/80 triangles, verifies both descriptors remain open, and tests empty/
invalid-span/channel preflight without syscalls plus synthetic error/name values.
No new malformed native inputs, broken-pipe, signal, crash, allocation-failure or
resource-exhaustion probes are introduced.

Fresh source quarantine guard/audit and clean build pass all 121 ordinary CTest
contracts (21.09 seconds), including existing process/transport cases. SwiftPM
builds the current 14-source worker in 20.76 seconds; actual packaged-helper base/
resource-profile, UV and visual contracts pass. Scoped ASan/UBSan instruments the
new runtime and ordinary test; inherited engine/operation libraries remain
uninstrumented. Scoped runtime coverage is 63/79 lines, 58/84 regions, 31/50
branches. Interrupted/error syscall and unusual short-write paths remain unforced;
no whole-core coverage or OS termination guarantee is claimed. Changed formatting
and whitespace pass. Header comments alone changed after gates, with an explicit
non-comment code equality proof; other audited runtime/test hashes match.

## Remaining adoption

Expose a narrow caught-exception worker C boundary, implement parent bounded
stream reading and authenticated transaction admission, then build the real XPC
host/control/lifecycle and sandbox/release packaging. Do not invent a normal
process exit from an XPC callback: a persistent service transaction needs its own
validated completion predicate, while the existing `PendingReply` process contract
continues to require an actually observed exit. Parent-death/resource/cancellation
acceptance and full M4/M8 remain open under #123. Preserve every quarantine.


## Serial delivery verification

Replayed after #193, preserving #190 aggregate component/chart text admission and
#191 bounds-checked pure metadata fixture access. The saved header-documentation
proof matches current header bytes; executable extraction/test source matches the
prepared stage. The current audited inventory adds the accepted pure quota test
and contains 122 ordinary contracts. After a fresh clean CMake build, eleven pure
preflight checks and the healthy file-channel pilot pass; all 122 contracts pass
in 23.45 seconds. The file-channel pilot passes in 0.25 seconds.

Fresh scoped ASan/UBSan instruments `WorkerJobIO.cc` and its ordinary test with
normal Capsule base/UV file channels; inherited operation/engine libraries are
not thereby instrumented. No syscall failure, interrupted/short write, native
invalid input, cancellation, signal, pressure or crash pilot is added. The
previous scoped coverage measurements above remain historical prepared-stage
evidence, not a new whole-core measurement.

The fourteen-source SwiftPM helper builds in 1.96 seconds; fresh UV dispatch and
visual-family verifiers pass against that packaged helper with normal completion.
Export compatibility, strict formatting, whitespace and recorded executable-source
hashes pass. Existing native build-system deprecation and duplicate resource
static-library warnings remain. Production dependency pin, release/containment
acceptance and all quarantines remain unchanged.
