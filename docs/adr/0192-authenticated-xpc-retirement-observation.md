# ADR 0192: Observe a registered authenticated XPC process lifetime

## Decision

The launchd-owned XPC service is not the parent's waitpid child. Do not reuse Unix supervisor
PID/signal/reap authority. Add a public Dispatch process-exit boundary and one bounded
registration/exit waiter. Service handshakes now return a process-scoped128-bit incarnation,
generated from a Foundation UUID once per service process. The value is correlation metadata,
not a credential; requirement-checked XPC identity and independent Foundation PID remain
mandatory. Zero or changed incarnations reject with original immutable receipts. The expanded
scalar reply requires the client and service from the same reviewed revision; no ABI stability
or Apple production-pin change is implied.

Before creating channels, the executor authenticates and retains the first peer, registers
its process-exit source, then awaits a fresh matching handshake. This brackets kernel event
registration with a still-live authenticated incarnation. Matching PID alone is insufficient.
The local macOS SDK dispatch/source.h registration contract places the registration handler
after the underlying kevent registration. The source observes the registered process instance;
the second handshake prevents accepting a source registered after the original service was
replaced. Duplicate old RPC callbacks retain their original tokens and cannot satisfy the
fresh barrier. A failed barrier never permits job channels or authenticated exit output.

Only actual .exit event flags can emit exit observation. Unexpected flags retain their raw
value. Capture monotonic uptime at each registration/exit/timer callback before actor delivery.
Checked absolute phase deadlines reject events at or beyond their original end even when the
timer callback is delayed. Early timer wakeups reschedule only the rounded remaining interval,
without moving that end. Overflow/reversed source-relative clock values retain typed metadata.
A source exit captured before a later wait starts can still be pending on the actor; it must not
be rejected solely for predating that wait's start. Event capture order and actor delivery order
are separate. The actor preserves registration, registered, waiting, observed-exit and retired-with-
original-cause states. Registration and exit budgets are independently explicit1...10000ms.
Each phase retains one waiter; stale timers/duplicate/late callbacks cannot re-open it.
Source/timer owners are resumed before escaping, use weak callback ownership and cancel locally.
There is no signal delivery, reaping, force-stop, new connection, native cancellation or active
channel close. Scheduling is not real-time and OS setup/termination cannot be promised bounded.

The executor's retire() remains an idle invalidation **request**. retireAndObserve() additionally
awaits the pre-registered source and rechecks original PID/incarnation/generation. Invalid exit
policy rejects before invalidation. It requires the completed setup barrier and owned phase
joins; original source/control/cleanup causes are retained. Expiry does not prove native drain,
process disappearance or permission to replace a still-live lifetime. Active native work can
still hold one bounded slot; full #123/M8 containment, release trust/resource/deadline policy,
Apple adoption and all feature quarantines remain open.

## Verification and limits

Missing incarnation APIs, observer APIs, unexpected-event API and executor integration APIs were
verified compile-red before implementation. Original-cause regressions reproduced three failures
where generic retired hid timeout/event causes; state now retains/rethrows the original error.
A failed-barrier regression reproduced two failures where retained initial peer metadata could
be promoted to authenticated exit evidence; an explicit successful-barrier state now prevents it.

43 focused tests pass:13 handshake,14 observer and16 preview/source tests. Negative lifecycle,
overlap, changed identity, unexpected event, deadline, premature exit and registration failure
cases use synthetic callbacks/timers/transports only. A public executor construction test compiles
local trust configuration and creates an unresumed Foundation object; it never resumes or contacts
a service. No real failed-peer, native cancellation, signal, crash, malformed native, allocation-
failure or pressure/exhaustion probe occurs.

Matched LLVM scoped summaries: observer204/204 reported line entries,79/83 regions(95.18%),
28/28 functions; executor258/264 line entries(97.73%),110/117 regions(94.02%),21/23 functions.
Native process-source adapter reports zero in that fake profile; native job factory/copy paths
are not exercised by fakes. This is neither whole-file XPC transport nor whole-core/branch/native
coverage. Do not combine profiles from different executables. No C++ parser/owner change or
new sanitizer claim occurs.

Two virtual-time regressions reproduced late registration/exit acceptance while timer delivery
was withheld. Captured monotonic deadlines now reject both at the exact boundary; just-before,
early-wakeup, pre-wait pending exit, backward-clock and checked-overflow cases pass without
wall-clock sleeps or OS deadline probes. An initial cross-callback ordering test was corrected:
actor delivery does not impose a total order on captured events. The policy uses phase/source
bounds rather than comparing every event to the last actor callback.

The pre-clock-hardening actual public-product healthy Capsule run passed2.31s after a fresh guarded CMake build:
registered exit event after matching incarnation barrier, source/geometry copies, cache reuse,
reset/rebuild and current-source publication all passed. Independent controller observation
confirmed servicePID2503 disappeared after hostPID2475 requested normal invalidation.
The controller retains its independent PID-disappearance check; it does not supply the in-process
observer's event or infer exit from invalidation. The pre-clock full gate passed234/234 CTest51.39s including79 public XCTest cases, with23
changed source hashes unchanged. Those results do not substitute for the final gate after
monotonic hardening; final rerun evidence is recorded after validation.

## Final delivery validation

After monotonic hardening, the fresh guarded build and234/234 CTest passed51.84s,
including85 importing public XCTest cases and the actual healthy public-product registered
service exit/controller PID-disappearance path. All23 changed source hashes remained
unchanged during that gate. Strict changed-Swift formatting and changed-Python syntax passed.
The43-case instrumented report above remains scoped to application orchestration and synthetic
boundaries; no native deadline, cancellation, failed-peer or termination probe was added.
