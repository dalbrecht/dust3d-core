# ADR 0167: Authenticated ordered session reset

Status: proposed for reviewed delivery. Tracks #251.

## Decision

Expose resetSession with immutable document/generation/cache scope and a host-issued
request identity on the existing requirement-checked XPC connection. Admit it through
the same scheduler as generation/close; never reset concurrently with accepted work.
Use the lazy coordinator's existing reset: reject uninitialized/closed/foreign scope,
never create a native session just to reset it, and preserve original typed diagnostics.
The scheduler consumes its own monotonic control identity; native generation retains
its independent replay sequence. Reset clears cache without replacing connection scope.

## Verification and limits

The regression first failed to compile on the missing resetSession API. The healthy
Capsule connection now generates twice, observes real cache reuse, resets, generates
again with real rebuild/nonreuse observations and equal copied geometry, then closes.
Every control response checks original status/error flags, captured identity and actual
OS peer PID. The controller requires the explicit rebuild marker and ordinary process
retirement. No fault/cancellation/malformed-native-input scenario was added.

Fresh CMake build, strict Swift6 concurrency/warnings-as-errors, format lint, source
quarantine guard and 210/210 ordinary CTest contracts pass (34.58s). Five changed
source hashes remain unchanged through validation. SwiftPM product sources did not
change; the preceding product build remains applicable. Reset RPC failure/control
paths are not fully covered; earlier fake coordinator contracts cover scope/lifecycle
errors without native failures. No whole-runtime/core coverage claim is made.
Apple adoption, output/range acceptance #279/#280 and distribution remain open.
Production pin48b0ce2d and all quarantines remain unchanged. Completed clean exact-head
automated review and all CI remain merge gates.
