# ADR 0179: Segmented session-output channels and C ownership

Status: proposed for reviewed delivery. Tracks #270 and Apple #279/#280.

## Decision and lifetime

Add `writePreviewSessionChannel` and `readPreviewSessionChannel` for borrowed POSIX
descriptors. The sender delegates complete semantic/range preflight to the accepted
session encoder, then writes bounded spans with original I/O errors. The receiver
constructs a private collector from independent scope/native controls and an explicit
transfer/chunk policy, reads with a fixed64KiB buffer, and retries EINTR only. That buffer
is additional to the collector's bounded workspace and retained typed values.

Only actual read0 calls finishAtEOF and permits the collector's one-time ownership move.
A terminal frame alone, early EOF, trailing bytes or semantic failure cannot publish a
value. The channel functions never open, close or shut down descriptors, parse archives
or generate geometry. They are blocking isolated-worker operations without deadline or
cancellation. The sender's host establishes signal/resource policy and finalizes EOF;
logical limits do not bound generator workspace, capacities or RSS.

Expose `dust3d_worker_preview_session_read` with a dedicated self-owned C receipt and
the existing captured archive/native controls and chunk policy. Session scope aliases
the existing independent correlation fields, with session direction fixed by this entry
point. C status preserves framing/range/codec/I/O/allocation/internal classification,
detail_source/code preserves the original enum, and errno/stage are separate. Diagnostic
messages retain up to511 bytes with explicit truncation. Append C detail sources33/34
for preview chunk/range; existing field layouts and meanings remain unchanged.

After complete semantics and actual EOF, adopt the independent value through existing
`bridge::adoptSessionOutput`. Reuse `Dust3DWorkerSessionOutput`, its immutable borrowed
getters and matching destroy function. Move geometry/source arrays and diagnostics;
do not duplicate a whole wire body or invoke legacy envelope/count/text policy builders.
The shared exception mapper now recognizes chunk/range errors without changing old
decode routes. All C exceptions are caught, result storage initializes on each call,
partial roots are destroyed, and NULL result performs no work.

Caller retains descriptors through calls and copies getter values before owner release
or actor transfer. Independent correlation and EOF are not peer authentication, healthy
process completion, current-source admission or permission to publish. Those host gates
remain mandatory and separate. No active service/IPC/app backend is switched here.

## Verification

Missing-API red compile preceded implementation. Tiny protocol-only C++ fixtures cover
sourced/ordinary/failure/empty outcomes, original indices and weights, binary source
causes/classification, statistics availability versus absence,1500-byte identity and
5000-byte warning. Direct C++ re-encoding matches original bytes. C values remain valid
after channel closure, with no legacy text cap reapplied. Tests also cover negative-FD
preflight without a syscall, empty actual EOF, narrowed logical record policy, unsupported
session version, complete transfer plus trailing byte, and invalid mesh-tagged emptiness
rejecting before any output. None of these bytes reach a native archive parser.

A pure-C consumer verifies self-contained headers/receipts and existing destruction.
A strict Swift6 consumer receives on Task.detached, copies geometry/indices/text and
declared available-zero statistics, destroys the C owner, then transfers only Sendable
copied values to an actor. It demonstrates boundary lifetime, not a production SDK
profile or proof that fixture statistics originated in an engine. Generated protocol
fixtures exist only in the shared build cache; no new native fixture is added.

Fresh build and225/225 CTest pass (35.41s). Actual SwiftPM runtime builds (0.16s with
retained cache), and the updated C, C++ and strict Swift consumers compile/link/run
against that actual static product. Both C/C++ formatters, strict Swift formatting,
quarantine guard and nineteen frozen source/build hashes pass. Scoped ASan/UBSan
instruments the new channel, C bridge, shared owner/exception mapper and C++ contract;
linked engine/other libraries are uninstrumented, no engine generation is called by
this protocol contract, and leak detection is off.

Channel file28/33 lines,19/27 regions,3/3 functions,9/13 branch outcomes; C bridge45/47
lines,22/26 regions,1/1 function,11/16 branches. Residuals include syscall failure/EINTR,
partial-write failure, allocation and unknown-internal exception paths. Do not provoke
signals, broken pipes, allocation failure or resource pressure to cover them. Shared
semantic rejection is exercised through original typed C receipts, not claimed as
whole-bridge/core coverage. No full-budget runtime/performance or whole-engine sanitizer
acceptance is implied.

## Remaining delivery

Connect borrowed archive ownership/native generation to output serving; integrate C/Swift
host supervision, process/peer/current-source admission and the production SDK/backend.
Baked/imported profiles require separate integration. Apple pin48b0ce2d, active routes
and core67/69GLB,83writer,84fault,95animation and Apple199rig-pose quarantines remain
unchanged. No new malformed-native-input/crash/fault/allocation-failure/signal/broken-pipe,
cancellation/large-allocation/pressure/exhaustion probes or protected app changes.
