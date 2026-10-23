# ADR 0066: Single-use parent response admission

Status: local implementation for #113; public review and hosted CI pending.

## Decision

Add PendingReply to the portable protocol library. One noncopyable/nonmovable
pending owner captures a validated request header. A serialized parent supplies
its authoritative current document/request/revision identity, observed terminal
process outcome and complete collected reply. Completion retires the pending
owner before any parsing or allocation. Cancellation, replacement and document
closure can explicitly retire it. Repeated completion never publishes.

Publication requires observed normal exit zero, the same still-active identity,
exact framing, matching response identity and successful mesh/failure semantic
validation under consumer limits. Every rejection remains retired, including codec
or allocation exceptions. Return values own their buffers. Original typed codec
errors propagate; process rejection preserves observed normal/exit values in the
error. Opaque native failure causes stay data and are not logged or interpreted.

The state owner is deliberately not thread-safe: transport/application orchestration
must serialize revision changes and completion admission. This is not a queue,
collector, process observer or sandbox. A caller must not infer exit from received
bytes or publish until actual process termination and output collection complete.
Wire and decoded payload budgets are logical, not a combined memory/RSS cap.

## Verification and boundaries

The new contract first failed because the admission API did not exist. Value cases
exercise independent failure ownership, success geometry, all three stale identity
fields, duplicate/cancelled completions, ordinary nonzero exit observations,
malformed/truncated/wrong-identity wire data, mesh/failure semantic rejection and
consumer output quotas. Non-normal outcome is only a policy value; no process is
crashed or signalled. The existing fifteen controlled subprocess scenarios now
use the admission API for actual mesh/failure replies after CMake verifies the
observed process status. Capsule, Sword and seam-report responses still compare
bit-for-bit with ordinary C ABI output.

Full normal CMake/CTest passed 59/59 in 26.22 seconds; the focused admission and
subprocess contracts also passed after adding stronger typed-error/ownership
assertions. A clean ASan/UBSan build passed 58/58 in 103.00 seconds. Instrumented
admission checks measured 25/25 lines, 22/22 regions, 3/3 functions and 12/12 branch
outcomes in WorkerAdmission.cc. This is file-scoped admission logic coverage,
not inherited-engine or production containment coverage. Formatting and the
source quarantine guard pass. All products reuse the shared build root.

Actual pipe collection/back-pressure, queue bounds, deadline/cancellation,
resource and filesystem isolation, Swift app integration, richer schemas and full
corpus acceptance remain future work. No production pin change or quarantine
removal is included. This does not close M7/M8 or establish full editor parity.
