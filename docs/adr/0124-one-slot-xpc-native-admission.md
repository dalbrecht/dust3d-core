# ADR 0124: One-slot native admission before XPC queueing

Status: prepared mitigation integration for #175; no production adoption.

## Decision

Add immutable, framework-free admission values with one active lease and no
pending native queue. Acquiring an idle state issues a monotonic nonwrapping
lease; active work yields busy, retired/exhausted state yields closed. Matching
completion releases only its lease. Stale completion leaves current work intact.
Retirement seals admission without interrupting accepted work. These leases are
process-local routing values, not authentication tokens or wire identities.

The infrastructure scheduler holds state transitions and executor submission
under the same lock. This closes the gap in which retirement could otherwise
queue stop before a previously admitted operation had been enqueued. The FIFO
executor never invokes inline or waits for execution. Normal retirement queues
one stop after every accepted submission; repeated retirement is idempotent.
Rejected submissions never enter the executor. A default serial DispatchQueue
still serializes all native entry, while recording-executor tests require no
native work or concurrency/load experiment.

The XPC fixture service acquires admission before calling the runtime or parsing
its body. It returns separate busy/closed control status without native execution,
queue retention or mutation of possibly aliased channel capabilities. Native
status/context fields are not meaningful on an admission decline. Accepted work
preserves its native result and output-finalization errno, then reports matching
lease completion; an unexpected stale completion is a separate status. The host
requires accepted admission in addition to its existing writer/EOF/semantic,
sandbox, actual peer PID and request/operation checks.

This bounds only application-native queued/running invocations. It does not bound
NSXPC marshaling, framework buffers, connections, kernel queues, process RSS or
an individual native algorithm. The fixture assumes one owning connection;
production per-connection ownership/retirement routing remains an explicit gate.
No ProcessOutcome, current-document policy or publication authorization is
invented. Sandbox grants, development peer policy, production pin and quarantines
remain unchanged.

## Verification

Pure transition tests initially failed on missing admission types, then pass with
old-state immutability, one slot, release/reuse, stale/idle completion, retired
active draining and idempotence. UInt64 maximum is checked as a small numeric
value, not through repeated work or resource exhaustion. A recording-executor
fixture initially failed on the missing scheduler, then verifies accepted-only
queueing, busy/closed exclusion, ordered stop after work, one stop and reusable
capacity. It records at most two deferred actions and performs no native job.

Measured pure state coverage is 30/30 lines and 22/22 regions. Scheduler coverage
is 55/57 lines and 15/17 regions under the recording fixture; default Dispatch
adapter code is exercised by healthy integration but is not profiled there.
Swift reports no branch-outcome metrics. These numbers do not establish whole-core
coverage or dynamic race/load correctness. Strict Swift6 concurrency/warnings
and formatting pass; no external dependency or portable C++ implementation changes.

Fresh source quarantine guard and registered-source audit include 139 ordinary
contracts. A clean build and pure admission/channel/entitlement preflight pass.
All four healthy file/socket base/UV XPC cases pass in 2.14 seconds and require the
new accepted-admission receipt. They retain distinct actual peer processes and
240 vertices/80 paired UV triangles where applicable. All 139 ordinary CTest
contracts pass in 22.51 seconds. All gated implementation/test/build hashes match
after documentation-only README/plan/ADR/changelog updates.

No real native overlap, busy-peer rejection, connection retirement fault, signal,
broken-pipe, allocation-failure, crash, denial or resource-exhaustion probe was run.
Previous scoped pure sanitizer evidence applies to identical portable runtime
source, with no new native/XPC sanitizer claim. Temporary bundles are removed only
after host exit and service PID disappearance, not independently observed launchd
exit status. Generated products are cleaned after terminal gates; source/caches,
checkpoints, protected apps and OS-managed containers are preserved.

## Serial delivery verification

Replayed onto accepted core #202, preserving the aggregate UV text budget,
checked UV fixture indexing and portable decoded-budget initializer corrections.
The source quarantine guard and inventory audit pass with 140 ordinary tests,
one more than the prepared inventory because the accepted pure quota regression
is retained. All four XPC cases retain their shared development-host resource lock.
A fresh clean build in the single shared root passes strict Swift 6 checks.

Normal and optimized sandbox policy suites each pass four tests. Pure admission,
recorded scheduling and channel metadata/result composition pass before native
work, followed by all 19 pure C/C++ preflight contracts. Four healthy file/socket
base/UV cases pass in 2.16 seconds and verify accepted admission. Socket base
returns 240 vertices (host14968/service14970); socket UV returns 240 vertices and
80 triangles (host14980/service14981), with actual peer identity and joined
writer/EOF/control evidence. Owned temporary bundles are removed after host exit
and service PID disappearance; service exit status is not independently observed.

All 140 ordinary CTests pass in 22.39 seconds. All 267 audited source hashes and
four fresh Swift fixture binary hashes remain unchanged. Formatting, Python syntax
and whitespace checks pass. No portable runtime/parser/ownership or SwiftPM input
changed; prior scoped evidence applies to identical code, with no new sanitizer
or SwiftPM execution claimed. No native busy-peer, overlap, retirement, cancellation
or fault probe was run. Production pins, quarantines, protected apps, OS-managed
containers and global SVG remain unchanged. Full editor acceptance remains open.

## Remaining acceptance

Implement per-connection ownership, cancellation/deadlines and failure diagnostic
transport; bind authenticated transaction/current-document publication policy;
review effective resource/sandbox/release trust and adopt through reviewed Apple
dependency integration. Production #67 block, all quarantines and full M4/M8/editor
acceptance remain unchanged.
