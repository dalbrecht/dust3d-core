# ADR 0189: Async preview coordination with drain-preserving cancellation

## Decision

Expose `WorkerPreviewPipeline<Executor>` in the existing pure client product. The actor
reuses the authoritative publication queue, holds one active and newest waiting immutable
source/continuation, and rejects foreign scopes/replays before replacing owned waiting work.
Waiting replacement/cancellation finishes promptly. Active cancellation retires publication
without cancelling the detached backend execution or releasing its slot before completion.
Only the newest waiting request starts after matching cleanup, including failed/stale work.

`WorkerPreviewExecuting` is an explicit trusted backend contract: execute must return/throw
only after its owned execution/channel cleanup drains, preserving original failures. It
returns copied Sendable values, authenticated original receipts and independent request-write
and semantic-EOF observations. The actor checks these through the existing admission gate;
it does not authenticate caller-supplied observations or create descriptor/runtime evidence.
No native owner may cross its actor boundary. A caller must recheck captured identity on the
UI actor before applying an admitted value. Backend deadline/process supervision is separate.

Retirement seals new submission/publication, removes waiting work and awaits active cleanup.
Only one retirement waiter is retained; concurrent extra waiters receive a typed rejection.
Retirement is not connection invalidation, process disappearance or proof of healthy work.
Active callers keep original backend errors; compound cleanup errors retain both causes.
No forced reconnect, deadline, native cancellation or Apple route activation is introduced.

The actor owns asynchronous work entries separately from the existing synchronous source
coordinator; both use the SAME metadata queue/admission implementation. No second policy,
native adapter, transport or dependency is added. Payloads remain caller-chosen immutable
Sendable snapshots/values, so this layer imposes no legacy geometry/count/policy normalization.

## Verification

Missing public executor/completion/pipeline and explicit publication-cancellation APIs gave
verified importing-test reds. An initial generic continuation inference diagnostic was fixed
with explicit types; a private completion-signature refinement compile error was corrected
before running tests. Neither compiler failure was a passing validation run.

Eleven fake-only tests cover success, newest replacement, stale active rejection, active
caller cancellation without backend-task cancellation/early slot release, waiting cancellation
without old-source resurrection, original backend failure/recovery, foreign scope/replay/
foreign cancellation, missing write/EOF, original receipt diagnostic bytes, pre-cancelled
caller admission and bounded retirement with successful/failed active drain. No native calls,
actual cancellation/failed-peer/overlap/pressure probes occur in these tests. Existing healthy
QA integration remains unchanged rather than being presented as live pipeline adoption.

Measured new application-orchestration file coverage:141/143 lines98.60%,59/61 regions96.72%,
19/19 functions100%. The two missing lines are cancellation arriving between the entry check
and continuation registration; the private queue-cleanup invariant-failure region is also
unforced. No race-pressure/private-corruption hooks were added to manufacture coverage.
There is no branch/whole-core coverage claim. Matched executable/profile retained separately.

Final fresh ordinary build and233/233 CTest pass46.96 seconds, including40 importing public
package XCTest cases and existing actual-product/XPC/unsigned-bundle gates. Strict formatting,
diff and10 frozen input hashes pass. Normal build flags restored after scoped profiling;
one shared build root only. No C++/parser/owner change or new sanitizer claim.

## Remaining work

A real descriptor/XPC executor must independently join archive writing, semantic EOF decoding,
authenticated receipt/OS peer and owned cleanup without blocking the UI or treating control
errors as native/process drain. Preserve complete supported native ranges, optional statistics,
source fallback and original diagnostics. Signed embedding/release trust, backend deadlines,
Apple adoption/pin review, all quarantines and M4–M8 acceptance remain open. Tracks #292/#270.
