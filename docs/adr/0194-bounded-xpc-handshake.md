# ADR 0194: Bound pre-job XPC handshake ownership

## Decision

Require an explicit validated 1...10000ms budget on every public connection handshake and both
preview executor handshakes. There is no unbounded overload or implicit default. Validate policy
and idle/generation ownership before reading the clock or consuming a token. Checked absolute
monotonic nanoseconds begin before connection setup. An overflow retains a nil token and retires
the lifetime without dispatch. Registration and active native work remain separate policies.

Capture scalar replies/errors/timer observations under one lock before scheduling actor delivery.
Store one selected terminal result, one global connection loss and one pending wake. A timely
captured reply wins over later timer/actor delay; exact-end and late callbacks cannot succeed.
Early wakes only rearm the original remaining interval. Clock reversal and overflow preserve
original typed metadata. Global invalidation/interruption retires the lifetime even after a reply
has been selected; stale per-RPC proxy errors cannot retire a later owner.

The transport owns the checked continuation and resumed timer. Expiry clears that RPC token,
completes its continuation exactly once, cancels its timer and invalidates the connection. The
first correlated deadline failure remains observable on later handshake attempts. A task-group
race around an unbounded continuation would retain ownership, so the deadline lives inside the
transport. Destruction resolves any pending handshake rather than leaking its continuation.

The executor validates its explicit handshake budget before transport construction and applies it
to both initial peer identity and fresh PID/incarnation barrier after exit-source registration.
Any failure retains original preparation/cleanup causes and prevents job/channel construction.
Existing PID, sandbox, admission and incarnation checks remain mandatory. No native job RPC is
interrupted or given cancellation/deadline semantics by this change.

## Verification and limits

TDD established the missing bounded API and executor policy. Pure policy/capture tests cover
checked overflow, reversed clocks, exact end, late/timely ordering, early wake and coalescing.
Fake transport/timer tests cover continuation ownership, late replies, overlap preflight, global
loss after timely selection and remaining-time rearming. Executor fakes verify both explicit
budgets and original deadline failure from either handshake before the job factory. Existing
healthy licensed Capsule integration uses an explicit trusted 5000ms handshake budget.

No actual service timeout, remote cancellation, malformed-native-input, crash, signal, resource
exhaustion or denied-peer probe is introduced. Healthy integration is not evidence of hard native
termination, real-time delivery, signed release trust or full editor parity. The production Apple
pin/routes and all existing quarantines remain unchanged. The full local gate passed 236/236 CTest checks in 46.28 seconds, including the actual healthy
public product consumer. The focused XPC/executor suite passed 87 tests. All 35 frozen source
hashes remained unchanged across the build/gate. Swift 6 strict concurrency and warning-as-error
product compilation and formatter checks passed.

A separately compiled fake-only XPC XCTest bundle passed 69 tests without linking the native
engine. LLVM mapped coverage: HandshakeDeadline 26/26 lines,11/11 regions; HandshakeCapture 144/144
lines,56/57 regions; HandshakeTiming 55/55 lines,17/18 regions; Connection 712/733 lines,159/180
regions and 54/55 functions. Counts are LLVM mapped line totals, which can overlap closures;
they are not physical source-line counts or combined integration profiles. The remaining
adapter/lifetime paths are not claimed covered; no unsafe probe was added to force them.
The standalone coverage harness required Xcode's XCTest Swift overlay and a proper local test
bundle before it ran successfully. These scoped measurements do not imply inherited engine
coverage or full editor acceptance.
