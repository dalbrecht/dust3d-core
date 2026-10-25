# ADR 0107: Preserve independent owned-worker cleanup diagnostics

Status: Local implementation validated; serial public review pending.
Tracking: [core #123](https://github.com/dalbrecht/dust3d-core/issues/123).

## Problem and decision

The owned-child destructor previously ignored errors from exceptional force-stop
and blocking reap. The original transport exception propagated, but a caller
could not distinguish a cleanup failure from successful child collection. Preserve
that primary exception type and add an optional caller-owned WorkerCleanupReport
to WorkerSupervision. Reset the report before preflight. The caller keeps it alive
until return/throw, reads it afterward on the calling thread, and never shares it
between concurrent operations. This is an additive development C++ API, not a C
ABI or binary compatibility promise.

WorkerCleanup is an allocation-free, noexcept policy over a narrow process-call
interface. No owned child means no calls. A supervised exceptional owner requests
force-stop once and attempts reap even when that stop fails; the legacy owner
only reaps. Interrupted wait observations retry reap only. Separate stop/wait
causes and an actually observed outcome survive independently. An invalid wait
observation reports a typed failure rather than fabricating a collected child.
The Unix adapter retains errno immediately and normalizes ESRCH on a stop as an
already-absent target. Existing ECHILD ownership retirement remains unchanged.

The child destructor invokes this policy after channel destruction and copies
its fixed-size report without throwing or wrapping the primary exception. A
normal collected child and pre-start rejection require no exceptional cleanup;
the reset report remains empty. No document data or child diagnostic bytes are
logged by this policy. Callers must surface the primary error and any separate
cleanup report safely. Existing calls that omit report storage retain their
prior behavior; production callers have not been migrated by this stage.

## Verification

The first recording-only contract failed on the missing header. Fresh strict-
warning contracts then passed, covering absent owners, legacy/supervised ordering,
interrupted observations, independent error codes, outcome preservation, invalid
wait state and primary exception propagation during RAII cleanup. Recording
implementations perform no process operations and deliver no signals. Existing
ordinary transport tests check reset reports for pre-start cancellation and normal
licensed completion; no additional failing-child workload was added.

The source quarantine guard passes. Registered-source audit confirms the existing
ordinary registrations are unchanged except the recording-only cleanup test;
111/111 freshly built Debug CTest contracts pass in 24.40 seconds. The core
SwiftPM dust3d-export product builds in 33.17 seconds. Changed C++ formatting and
diff checks pass. A fresh scoped strict-warning ASan/UBSan executable of only the
recording policy/tests passes. Scoped LLVM coverage measures WorkerCleanup.cc at
28/28 lines, 13/13 regions and 14/14 branch outcomes. This is not sanitizer or
coverage evidence for the actual Unix stop/reap paths or the whole engine.

## Limits

No active stop, signal-delivery, crash, broken-pipe, allocation-failure or resource-
exhaustion probe was run. Actual exceptional OS paths remain unforced. Reap in
uninterruptible kernel state can still block; reporting is not bounded cleanup,
strong memory/descendant/filesystem/network containment or trusted packaging.
Partial buffers on transport errors are unchanged. Preserve core #67/#69 GLB,
#83 writer, #84 faults, #95 animation and Apple #199 rig-pose quarantines, and the
Apple production core pin. #123/M8 and full editor parity remain incomplete.
