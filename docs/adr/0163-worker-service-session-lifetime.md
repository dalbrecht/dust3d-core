# ADR 0163: Lazy Swift connection-session ownership

Status: proposed for reviewed delivery. Tracks #251.

## Decision

Add a reusable synchronous coordinator to the Swift worker prototype. An immutable
scope contains the host-selected document, positive connection generation and bounded
cache allowance. The caller must establish authentication before supplying it.
The first accepted scope binds permanently, even if native creation fails; later
calls cannot change document, lifetime or cache policy. Creation is lazy, so an idle
connection does not allocate a native cache. Jobs/receipts must be Sendable; the
opaque backend handle stays behind OSAllocatedUnfairLock, including all backend
calls, reset, close and final destruction. No unchecked Sendable conformance is
added to the coordinator. Backend methods cannot reenter the coordinator.

Reset requires a live initialized matching scope and retains native replay policy.
Close seals admission before releasing state, is idempotent, and destroys the handle
even when backend close reports an original error. Backend creation/serving/reset/
close errors propagate without generic replacement. Deinitialization provides final
destruction if explicit close was omitted; it does not fabricate successful close.
Close waits for synchronous work, never interrupts generation; call off the UI thread.

The native adapter invokes existing C session APIs and copies boundary diagnostics
into owned Swift values, preserving status, source, signed code, truncation and opaque
bytes. The existing ordinary session consumer now exercises this adapter/coordinator
against real Capsule data. No new dependency is added. These files remain worker
prototype support, not a new public Swift product or activated Apple backend.

## Validation

The fake-only contract began red on missing coordinator types. It verifies lazy
creation, same-scope reuse/reset, immutable document/generation/cache, closed and
uninitialized rejection, retained binding after a synthetic creation error, original
synthetic serving/close causes, destruction after close error and deinit cleanup.
Fakes never call native code. A separate synthetic C receipt verifies non-UTF8 cause
copying without inducing native failure. Swift6 strict concurrency, warnings-as-errors
and strict formatting pass.

Full CMake build and208/208 ordinary CTest contracts pass (34.19s), including the four
existing healthy stateless XPC fixtures. The ordinary retained-session consumer and
new native coordinator also link/run against the actual SwiftPM archive; the product
build passes (0.13s). Five source/build/test hashes match final validation. Guard and
manifest lint pass. No C++ implementation/wire changes or new native failure probes.

Matching Swift profiles cover the generic coordinator87/87 lines,39/39 regions and
13/13 functions with fakes. The thin native adapter covers32/34 lines (94.12%),11/14
regions and8/8 functions with ordinary data plus synthetic diagnostic values. Actual
native create/reset/close failure guard regions remain unforced. Swift coverage
emits no branch outcomes here; region coverage is not branch coverage. These counts
are not whole-core/runtime coverage or an assertion that infrastructure adoption
targets are met. No new sanitizer claim is made; inherited native libraries remain
uninstrumented in this scoped evidence. No new malformed native input, crash/fault/
signal/native-cancellation, pressure or exhaustion probes, new XPC scenarios or app
control were performed.

## Remaining integration

StaticWorkerObject still uses its existing stateless route. Wiring the coordinator
to authenticated per-connection RPC metadata and ordered retirement, platform image
decoding, full authored/procedural/source-baked output and Apple range acceptance
remain under #251/#279/#280. Existing stateless XPC success is not session-operation
authentication evidence. Production pin/backend and quarantines remain unchanged.
Completed clean exact-head review and ALL CI remain required before merge.
