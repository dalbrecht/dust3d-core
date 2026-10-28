# ADR 0180: Segmented native preview serving

Status: proposed for reviewed delivery. Tracks #270 and Apple #279/#280.

## Pipeline and admission

Add `servePreviewSessionJob` and `dust3d_worker_preview_session_serve` as explicit
worker-local library entry points. Require separate valid borrowed descriptors,
archive-direction correlation, original captured native controls and explicit request/
reply chunk policies before I/O. Every session has mandatory session/statistics prefixes;
check that minimum reply size without fabricating observed geometry or statistics.

Read the complete correlated archive through actual EOF into its existing owner, then
invoke `ConnectionGenerationSession::generatePreview`. Preserve legacy serving order:
invalid transport is preadmission; connection document/lifetime/replay admission follows
complete input. Once admitted, IDs remain consumed even on later work or output errors.
Caller serializes jobs and guarantees session lifetime; concurrent destruction is forbidden.
The existing connection/native mutexes and engine lock retain their roles.

Generate/copy under original native source/indexed policies, release the raw archive
after independent native copies return, move the session outcome/observations, and write
bounded session output. No legacy Frame/envelope, duplicate archive vector, whole reply
buffer or consumer count/text quota is introduced. Same-session ordinary fallback and
original failure/statistics semantics remain those of ADR0178.

Factor the existing preview C receipt translation for both read and serve. Add explicit
protocol7/admission8 statuses; preserve all previous values/layouts. Recognize original
SourceRequestError in the shared semantic mapper instead of degrading an archive-prefix
rejection to an internal error. Preserve original class/code/cause/truncation and I/O
errno/stage. The C serve call returns a self-owned receipt and catches all exceptions.

OK means complete session values were written, including native-failure outcomes. It
does not mean mesh success, actual reply EOF, healthy process exit, authenticated peer,
current-source admission or permission to publish. Host establishes signal/resource
policy, isolated non-UI execution/supervision and channel finalization. No deadline,
cancellation, WorkerMain mode, active service/SDK/app route or backend is activated.

## Evidence

Missing serving API red compile preceded implementation. Existing licensed small Capsule
jobs cross archive framing, actual input EOF, native generation/cache, output framing,
C ownership and copied values. C/C++ cases verify cache reuse/reset, stable indices and
owned values after channels close. Direct C++ serving with disabled cache returns the
same geometry and actual disabled-cache observations. Strict Swift6 runs healthy jobs on
Task.detached, copies every vertex attribute/index/node identity and actual cache reuse,
destroys C owners/session, then transfers only Sendable values to an actor.

Argument/protocol cases cover missing owner, negative descriptors without syscalls,
aliased descriptors, wrong direction, mandatory-prefix policy rejection, actual early
EOF, unsupported archive-prefix version and replay. Invalid prefix bytes never reach a
native parser. A tiny reply-codec allowance after healthy native work rejects before
output bytes and consumes the request ID; the native archive and native budgets remain
unchanged. This is logical transport-policy validation, not native allocation failure,
resource pressure or exhaustion. It also proves prior transport rejection did not consume
that request ID. No new native fixture or malformed-native-input probe is introduced.

After interruption, the original execution handle was missing and process inspection
confirmed validation was stopped; the partial40/227 log was not treated as success.
Restore only the temporary alias to the existing cache, verify unchanged source hashes
and rerun the gate. Final full build and227/227 CTest pass (41.16s). Actual SwiftPM runtime
cached build (0.18s) and current C/C++/strict Swift consumers compile/link/run against that
actual product. Both C/C++ formatters, strict Swift formatting, quarantine guard and
nineteen frozen source/fixture/build hashes pass. Source stayed frozen during gates.

Scoped ASan/UBSan passes for healthy serving and, separately, existing protocol-only
ownership cases. Instrument serving/channel/C bridge/shared owner-mapper code and the
contracts; linked engine/other libraries are uninstrumented and leak detection is off.
Serving uses only the existing healthy fixture; protocol scope performs no native
generation. Separate per-executable profiles avoid ambiguous test-main aggregation.

New C++ serving entry:11/11 regions,8/8 branch outcomes. C serving entry:7/7 regions,
2/2 branches. Under the serving-only profile, the broader I/O file41/48 lines,26/38
regions,4/4 functions,15/21 branches; broader C bridge62/79 lines,32/45 regions,6/6
functions,12/22 branches. These scopes do not prove all shared paths. The separate
protocol regression profile also passes; do not add its counts as independent coverage.
Syscall/EINTR/partial-write, allocation/unknown-internal and actual failure-driven native
paths remain unforced. No whole-engine sanitizer, full-core coverage or full-budget
runtime/performance/RSS acceptance claim.

## Remaining delivery

Expose the profile through a supervised worker endpoint and production SDK with independent
process/peer/current-source admission, then review backend/dependency adoption. Baked and
imported profiles remain separate. Apple pin48b0ce2d, active routes and core67/69GLB,
83writer,84fault,95animation and Apple199rig-pose quarantines remain unchanged. No crash,
allocation-failure, signal/broken-pipe/cancellation/large-allocation/pressure/exhaustion
probes, stale binaries, app control or protected-state changes.
