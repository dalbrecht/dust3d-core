# ADR 0164: Drain session cleanup before service retirement

Status: proposed for reviewed delivery. Tracks #251.

## Decision

Extend the internal Swift scheduler retirement operation with a throwing cleanup
callback and deliver its original Result<Void, Error> to the retirement observer.
Admission is sealed under the scheduler lock. Cleanup is enqueued under the same
ordering lock after every accepted job, but executes outside it; cleanup can inspect
scheduler state without deadlocking. Duplicate or foreign retirement never queues
cleanup. The default empty cleanup returns success for existing callers.

Each XPC fixture object now owns the lazy session coordinator from ADR0163. Its
invalidation path queues session.close through scheduler retirement, so any accepted
synchronous work drains before cache destruction and process retirement. An unclaimed
connection cannot close the active owner's session or retire its process. The fixture
observer exits successfully only after successful cleanup; on failure it exits with
failure and emits typed status/source/code/truncation metadata. Opaque native cause
bytes remain available in the original callback Result and are intentionally not
written to logs because they may contain document text. Accepted job receipts are
not replaced by a later secondary cleanup result.

This changes only internal Swift prototype interfaces and their callers, not the C
ABI, native generation, app pin or backend selection. StaticWorkerObject's job route
is still stateless; its session coordinator remains lazy until the forthcoming
explicit authenticated session RPC path uses it.

## Verification

A fake-executor contract began red on the missing retirement Result/cleanup API.
It verifies accepted-job/cleanup/observer ordering, no inline execution, inspection
outside the scheduler lock, exact-once cleanup, foreign/duplicate rejection, and
preservation of a typed synthetic failure with opaque bytes including NUL/non-UTF8.
The existing success case now verifies custom cleanup before the stop observer.
No native failure is induced by these tests.

Full CMake build and208/208 ordinary CTest contracts pass (53.03s), including the four
existing healthy stateless XPC fixtures. SwiftPM runtime builds (15.51s); strict Swift6
concurrency/warnings-as-errors, formatting, manifest lint and quarantine guard pass.
Four source/build/test hashes match final validation. The reboot removed only the
/private/tmp cache symlink; it was restored to the existing persistent shared cache.
No second build folder was created.

The matching fake-executor Swift profile covers the retirement method/default and
its closures at8/8 code regions. Overall scheduler coverage is200/202 lines (99.01%),
67/71 regions and27/29 functions. Inherited executor/defensive paths are not fully
represented by that fake-only profile. Swift reports no branch outcomes here;
regions are not branch coverage. No whole-core coverage or new sanitizer claim.
The native service cleanup-failure observer remains unforced; its original-result
contract is exercised with fakes. No new malformed-native-input, crash/fault/signal,
native-cancellation, pressure/exhaustion probes, new XPC scenarios or app control.

## Remaining work

Add explicit authenticated per-connection session RPC scope and request routing,
then establish session-operation transport evidence and platform decoding/full-output/
range acceptance under #251 and Apple #279/#280. Existing stateless XPC tests do not
prove session RPC authentication or initialized-cache cleanup through XPC. Production
pin/backend and all quarantines remain unchanged. Completed clean exact-head review
and ALL CI remain required before merge.
