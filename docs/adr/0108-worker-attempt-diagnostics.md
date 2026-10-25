# ADR 0108: Preserve bounded partial worker diagnostics

Status: Local implementation validated; serial public review pending.
Tracking: [core #123](https://github.com/dalbrecht/dust3d-core/issues/123).

## Decision

Add optional caller-owned WorkerSupervision.attemptReport output. Reset it before
preflight; callers retain storage until return/throw, read it afterward on the
calling thread and never share it across active operations. A private RAII capture
moves the collector's already-bounded reply and opaque diagnostic vectors, with
the truncation flag, when an exception unwinds the collector scope. It does not
allocate new payload storage, decode/log bytes, wrap or replace the original
exception. Its optional outcome exists only when the collector observed terminal
process state. Exceptional cleanup observations remain separate in cleanupReport.

WorkerAttemptReport is diagnostic-only and is not a CollectedJob or an admissible
completion. Never publish or parse its partial bytes as a successful worker reply.
Normal completion leaves it empty; preflight rejection resets prior stale output
without entering collection. Intentional retirement dismisses partial capture
because WorkerStopped already owns its collected context. Later resource-admission
errors retain their existing ResourceJobError context. The report contains only
bytes read before the error, not unread channel data or complete worker output.

Declare capture before the child owner: exceptions close channels, perform child
cleanup, then move still-owned collector buffers into report storage. The observed-
exit flag outlives capture and its constructor requires an lvalue, preventing a
temporary bool from becoming a dangling observation. Interrupted/uninterruptible
OS cleanup can still delay reaching this report. No retry, forced drain or new
termination behavior is added. Existing calls without output storage keep their
prior error types and buffer ownership. No production caller adoption is claimed.

## Verification

The first value-only test failed on the missing API. Fresh strict-warning tests
pass for original runtime-error propagation, independent binary buffers/truncation,
observed versus unobserved terminal state, normal exit, stale report reset,
dismissed retirement and omitted storage. Compile-time checks require nonthrowing
report movement/destruction and reject temporary outcome-state borrowing. Existing
ordinary transport/pre-start tests verify empty/reset reports with no new failing
child workload. Source inspection retained the conditional retirement branch;
normal licensed supervised completion passes the full suite.

The source quarantine guard and registered-source audit pass; only the new
value-only test registration is added. Fresh CMake build and 112/112 ordinary
CTest contracts pass in 20.20 seconds. Scoped fresh ASan/UBSan tests of only the
capture policy pass. Scoped LLVM coverage measures WorkerAttempt.cc at 18/18 lines,
17/17 regions and 10/10 branch outcomes. Changed C++ formatting/diff checks pass.
The unchanged SwiftPM product builds incrementally in 0.09 seconds; worker targets
are CMake development infrastructure, so this is a compatibility build check, not
SwiftPM coverage of the worker. Logs and tested source hashes are retained.

## Limits

Actual transport-error/cleanup OS paths remain unforced; value-only exception
unwinding is not an OS failure, signal event or proof of containment. No crash,
signal-delivery, deliberately broken-pipe, allocation-failure or resource-exhaustion
probe runs. Strong sandbox/memory/descendant/parent-death limits and trusted helper
packaging remain open. Preserve core #67/#69, #83, #84, #95 and Apple #199
quarantines and the Apple production core pin. #123/M8 and full editor parity
remain incomplete.
