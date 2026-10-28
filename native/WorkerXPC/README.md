# Dust3DWorkerXPC

macOS worker RPC contract and async connection adapter. Import the static SwiftPM product
Dust3DWorkerXPC with Dust3DWorkerClient. Dependencies are that pure client module and system
Foundation/Security only; no engine, Qt, signing identity, service identifier or development
pin is embedded. The library is not directly runnable and does not package a worker service.

## Connection and source ownership

Create WorkerXPCConfiguration with an explicit embedded service identifier and trusted
host-selected signing requirement. Security validates requirement syntax and retains its
original OS status on failure; this is not a distribution-policy or peer-verification claim.
Create WorkerXPCConnection from that configuration. submit/submitSession/submitPreview,
resetSession and closeSession are async throws and return original profile-specific scalar
receipts. FileHandles remain caller-owned. Passive options retain exact raw budgets; the
adapter neither encodes archives nor interprets native semantics.

One RPC is active per connection. Overlap and idle invalidation reject while pending;
connection generation and independently observed OS PID cannot silently change. Duplicate
callbacks cannot complete twice. Original remote errors and bounded diagnostic bytes retain
their domains. Interruption/invalidation retires the lifetime; no automatic reconnect,
native cancellation or process termination is performed. Task cancellation alone does not
interrupt work or finish a continuation before the service responds.

A callback/error is not proof of frame EOF, semantic success, native drain or process exit.
Retain sources/channels and independently drain/clean them, then complete/abandon the
client source coordinator. Invalidate the idle connection only after that cleanup. Build
worker/client from the same reviewed source revision; binary ABI stability is not claimed.

## Development and limits

CMake builds this same module and the existing healthy XPC fixture uses the adapter through
a C-value-only test facade. Public SwiftPM XCTest uses fake channels for errors, duplicate
callbacks, interruption, PID changes and diagnostic limits; those cases never probe native
workers. Use the existing shared build root/native SwiftPM backend with the CTest public
package wrapper. Canonical production service executable/bundle, distribution trust and
mutable SwiftUI backend integration remain open. QA IDs and private CDHash sidecars remain
outside this library. No Apple dependency pin or quarantine is changed here.

## Pre-job identity

Call `try await connection.handshake(generation: 1, timeoutMilliseconds: 5000)` before creating a real executor's job
channels. Retain its immutable receipt separately: it preserves reported PID/sandbox/ownership
status, independent OS-observed PID and fixed host generation after later control failure.
The method rejects unmatched/missing PID, unverified sandbox or rejected ownership and retires
that lifetime. It does not consume job IDs or create a native session. Repetition is ownership
only, not native-idle/admission proof. Existing direct job methods remain compatible and do not
implicitly handshake. Connection invalidation and known PID do not establish native drain or
process disappearance; the descriptor executor/supervisor must observe those independently.

## Bounded pre-job handshake (#302)

Every handshake requires an explicit 1...10000ms budget. Invalid budgets reject before token
consumption or dispatch. A checked monotonic deadline begins before connection preparation;
reply, transport error and timer callbacks capture their scalar metadata before actor scheduling.
A reply captured before the original end remains eligible during delayed actor delivery. Exact-end
and late replies fail. Early timer wakes reschedule only the original remaining interval.
Capture storage is bounded and duplicate callbacks coalesce. Global connection loss still retires
the lifetime even when a timely reply was already selected.

`WorkerXPCHandshakeFailure` retains the RPC token (nil before ownership), generation, explicit
budget and original overflow/reversal/expiry cause, plus any coincident original transport error.
Expiry completes the owned continuation once, invalidates the connection and retains the first
failure for later handshake attempts. Stale callbacks cannot revive it. This is a pre-job RPC wait,
not a native job deadline, remote cancellation, process-exit proof or real-time scheduling guarantee.
Both executor identity handshakes use this contract; failure prevents job factory/channel creation.
Negative timing/order coverage uses fake clocks, timers and channels only. See ADR0194.

## Registered process exit (#298)

Handshake receipts now retain a nonzero128-bit service-process incarnation. It is correlation
metadata generated once per service process, not a signing credential. A fresh handshake must
match the original incarnation even when the independent OS PID matches; zero/rebinding retains
original receipts and retires the connection. The exact-revision wire reply gains two scalars:
client/service must be rebuilt together, with no binary compatibility or production-pin claim.

`WorkerXPCExitObserver(peer:)` starts unregistered. `register(timeoutMilliseconds:)` waits for
Dispatch's underlying event-source registration, then callers must complete a **fresh matching
authenticated handshake** before opening job channels. `waitForExit(timeoutMilliseconds:)`
returns only an event from that registered source. The preview executor enforces the entire
barrier and correlated retirement path. Direct observer use alone does not authenticate the
process incarnation, admit a result or prove native cleanup. No PID is signalled or reaped.

Each budget is explicit1...10000ms; only one registration/exit waiter is retained.
Monotonic capture timestamps enforce the checked original phase end even if timer delivery
is late. Early wakeups reschedule remaining time without extending it. A queued exit captured
before a later wait begins remains eligible. Overflow and source-relative clock reversal retain
original typed causes. This is event-capture timing, not an OS real-time completion guarantee. Expiry and
unexpected raw event metadata retain their original cause for later callers and never fabricate
exit. Stale/late callbacks do not reopen the observer. Sources/timers resume before ownership
escapes and cancel locally on completion/failure/destruction. Public Dispatch/Darwin system
APIs add no third-party dependency. The thin OS adapter has healthy integration evidence;
negative/lifecycle coverage uses fake event sources and timers only. Hard native deadline/resource
containment, signed release trust and Apple adoption remain open. See ADR0192.

Callbacks synchronously capture/enqueue metadata in one bounded inbox before waking the actor.
One pending value per five roles prevents duplicate growth; scheduled Task order cannot let a
timeout overtake an earlier timely event. Checked clock overflow also retires and retains its
original cause, so later callbacks cannot promote failed observation into success.
