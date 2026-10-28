# ADR 0195: Enforce service-owned job wall deadlines

## Decision

Require an explicit trusted `WorkerServiceJobBudget` (1...3600000ms) and terminal callback in the
service listener. The executable reads schema 3 own-bundle `Dust3DWorkerJobWallMilliseconds` as a
canonical decimal string. The unsigned stager requires the value; legacy bundles cannot silently
acquire a default. Cumulative CPU/file/core/descriptor limits and peer trust remain independent.

Start checked monotonic bounds when the scheduler acquires an admitted owner/request/lease, before
native queueing. Retain one current attempt/timer and one terminal failure. All ownership checks,
clock observations and completion/expiry selection share the scheduler lock. Old callbacks validate
attempt identity before reading time. Early wakes rearm only the original remaining interval.
Queued entry and completion reject exact-end/late observations even if timer delivery is delayed.
Overflow/reversal/expiry preserve original scalar owner, request, lease, budget and clock cause.

The timer runs independently of the native serial queue. Expiry seals admission before invoking a
required terminal callback outside the lock; it does not release the active native slot as success,
start newer work, or run queued normal-retirement cleanup. A dispatch transition admitted before
expiry is running ownership, not a promise about the exact CPU instruction when native code begins.
The library never exits its caller. The dedicated product logs metadata via unified logging and
calls `_exit`, avoiding blocked native cleanup, finalizers and stderr pipes. This intentionally
abandons helper memory/owners; host authenticated RPC/EOF/semantic/current-source gates remain
mandatory and cannot interpret abandonment as complete output. Logging delivery is best effort;
there is no promise that a terminal cause reaches the parent before the service disappears.

The per-job policy does not bound idle retirement cleanup, parent channel joins, process startup,
OS scheduling or memory. Signed release trust/actual host embedding, parent-death policy, native
high-resource acceptance, Apple dependency adoption and M4–M8 parity remain separate gates.
No production pin/quarantine changes or new dependencies are included.

## Verification and limits

Tests precede implementation: missing scalar/scheduler APIs, missing stager argument support and
loss of original metadata each produced recorded red failures. Fake clocks/timers/recording native
executors and terminal callbacks cover queued/running ownership, late completion/dispatch, early
wake, stale timer correlation, busy preflight, clock overflow/reversal and suppressed retirement.
Pure bundle/stager tests cover mandatory schema/policy and exact copied bytes/entitlements.
Only existing healthy licensed native integrations receive explicit generous job budgets.

No real watchdog expiry/exit, native cancellation, signal/crash, malformed-native-input, allocation
failure, pressure/exhaustion, broken-peer or denied-peer probe is introduced. The actual `_exit`/OS
logging boundary is deliberately unexercised. Code integration and synthetic policy evidence must
not be presented as measured hard process-termination acceptance. The full fresh ordinary gate passed 236/236 CTest checks in 47.54 seconds, including healthy XPC
jobs and actual-product unsigned bundle import. The affected service suite passed 38 XCTest cases;
seven Python staging cases passed. All 31 frozen source hashes remained unchanged. Swift 6 strict
concurrency/warnings-as-errors product compilation, both source quarantine/format checks and diff
validation passed. Initial closure-style lint failures were corrected before rebuilding the gate.

A separate coverage build compiled only the five canonical admission/ownership/deadline/timing/
scheduler sources, with no engine/runtime linked. Thirteen synthetic XCTest cases plus the existing
recording admission smoke passed. LLVM mapped coverage: JobDeadline 25/25 lines and 11/11 regions;
JobTiming 25/25 and 6/6; Scheduler 344/346 and 110/115, 37/39 functions; Ownership 24/24 and 15/15.
Mapped closure line counts can overlap physical lines. Unforced adapter/lifetime paths remain
unclaimed; no probe was added solely to force coverage. This profile is not actual native/OS-exit
coverage or inherited engine completeness.
