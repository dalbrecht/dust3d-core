# 0126: Withdraw queued work before native entry

Issue: #177. Status: local implementation; public review pending.

## Decision

The service scheduler accepts an explicit cancellation callback with every job.
Only the owning connection and matching active lease can withdraw queued work.
Cancellation and the transition to native entry share the admission lock: once
entry wins, cancellation reports alreadyStarted and never pretends to interrupt
native execution. Repeated cancellation is idempotent; stale/foreign requests
cannot alter current work. The cancelled job retains admission until the FIFO
executor runs its cancellation callback instead of its native operation, then
releases the slot. Normal retirement remains behind that action.

No cancellation command is exposed over XPC in this stage. The service supplies
a distinct cancelledBeforeStart scalar receipt callback for later integration;
the healthy fixture never invokes it. This is a scheduling foundation, not
end-to-end cancellation or a bound on transport/captured-input memory. Running
native work, deadlines, reconnect policy, descriptor teardown and authenticated
current-source publication require subsequent integration and review.

## Verification and limits

A regression first failed to compile for the missing cancellation API. Recorded
execution verifies foreign, duplicate, pending and stale cancellation, retained
busy admission, retirement ordering and already-started work. No native work
runs in those tests. The first full build stopped at strict Swift closure-call
lint; corrected syntax passes strict lint and the fresh completed build.
Four current policy/metadata tests pass (0.56 seconds). All 139 audited ordinary
CTest cases pass (22.51 seconds), including the existing healthy isolated XPC
base/UV file/socket cases. Source hashes match the build and test inputs.

The scheduler pure recording profile measures 162/164 lines and 49/53 regions;
two default Dispatch adapter lines are outside that run. Swift reports zero
branch outcomes, so no branch-coverage claim follows. No C++ runtime or parser
change and no new native sanitizer claim. No native cancellation, rejected-peer,
real overlap, signal, crash, allocation-failure or pressure probe ran. Pins and
quarantines remain unchanged; full editor/M8 acceptance remains open.

## Serial delivery verification

Replayed onto accepted core #204 with the accepted aggregate UV text budget,
checked UV fixture indexing and portable decoded-budget initializer fixes retained.
The quarantine guard and source inventory audit pass with 140 ordinary tests,
including the accepted pure quota regression. All four XPC cases retain their
shared development-host resource lock. A fresh clean build uses the single shared
root and passes strict Swift 6 compilation and formatting.

Normal and optimized sandbox policy suites each pass four tests. Pure admission,
ownership, recorded queued cancellation and channel metadata checks pass before
native work, followed by 19 pure C/C++ contracts. Four healthy file/socket base/UV
XPC jobs pass in 2.21 seconds. Socket base returns 240 vertices
(host27064/service27066); socket UV returns 240 vertices and 80 triangles
(host27075/service27076). Both join writer completion, actual EOF decoding and
control receipt; all four verify accepted admission and actual peer identity.
Owned temporary bundles are removed after host exit and service PID disappearance;
service exit status is not independently observed.

All 140 ordinary CTests pass in 22.77 seconds. All 268 source hashes and four
fresh Swift fixture binary hashes remain unchanged. Historical scoped pure
coverage above applies to identical implementation bytes; no new coverage,
sanitizer or SwiftPM execution is claimed. Portable runtime/parser/C++ ownership
and SwiftPM inputs are unchanged. No native cancellation, rejected-peer, retirement,
overlap or fault probes were run. Production pins, quarantines, protected apps,
global SVG and OS-managed containers remain unchanged. XPC cancellation routing,
deadlines, application adoption and full editor acceptance remain open.
