# 0127: Route queued cancellation by caller request identity

Issue: #178. Status: local implementation; serial public review pending.

## Decision

Carry a nonzero UInt64 caller request identity in the private development XPC
serve method. Accepted identities strictly increase for the owning connection's
service lifetime. The scheduler retains only the last accepted and current
identity, not an unbounded history. Rejected submissions do not advance either
identity or native lease. Zero/reuse/backward identities reject; UInt64 exhaustion
cannot wrap, so a new service lifetime is required after the final identifier.

A scalar cancelQueued method routes the supplied identity through the same
connection-owned scheduler. A stale or foreign request cannot affect a later job.
Cancellation and native entry remain atomic under the existing lock. Completion
clears current routing; monotonic history remains. Wire cancellation outcomes are
UInt32: cancelled0, alreadyCancelled1, alreadyStarted2, stale3 and notOwner4.
These are distinct from job-admission status values. The cancelled job callback
carries the caller identity plus cancelledBeforeStart admission status, rather
than asserting that native input or geometry was evaluated.

A caller identifier is correlation data, not authentication or document-source
proof. Native success still reports the parsed request identity separately; the
healthy host verifies it matches its expected request. Authenticated/current-source
publication, remote cancellation acceptance, channel teardown for cancellation,
deadlines, running-native cancellation and release policy remain open.

## Verification

The missing-routing regression first fails to compile. Pure recorded tests then
cover zero/reused identities, old-vs-current cancellation, foreign routing,
queued withdrawal, clearing after completion, already-started work and maximum
identity without wrap. Strict Swift6 concurrency/warnings and formatting pass.
Four current policy/metadata cases pass0.58s. All139 source-audited ordinary CTest
cases pass22.62s, including healthy isolated XPC base/UV file/socket serve calls
using the new signature. Source hashes match the freshly built/tested inputs.

The scheduler pure profile measures188/190 lines and63/67 regions; default
Dispatch adapter lines are outside that run. Swift reports no branch outcomes.
The remote cancel declaration/handler compiles but is not invoked in native
integration. No cancellation, real multi-peer/denial/overlap, crash, allocation,
signal or resource-exhaustion probe ran. No C++ parser/runtime change or new
sanitizer claim. Production pin and all quarantines remain unchanged; full editor
and M8 acceptance are incomplete.

## Serial delivery verification

Replayed onto accepted core #205, preserving the accepted UV aggregate text
budget, checked UV fixture indexing and portable decoded-budget initializer fixes.
The quarantine guard and source audit pass with 140 ordinary registrations,
including the accepted pure quota regression. All four XPC cases retain their
shared development-host resource lock. A fresh clean build in the single shared
root passes strict Swift 6 compilation and formatting.

Normal and optimized policy suites each pass four tests. Pure identity routing,
queued cancellation, admission, ownership and channel metadata checks pass before
native work, followed by 19 pure C/C++ contracts. Four healthy file/socket base/UV
XPC jobs pass in 2.23 seconds using the new serve signature. Socket base returns
240 vertices (host32517/service32519); socket UV returns 240 vertices and
80 triangles (host32532/service32533). Both join writer completion, actual EOF
decoding and control receipt; all cases verify accepted admission and actual
peer identity. Owned cleanup follows host exit and service PID disappearance;
service exit status is not independently observed.

All 140 ordinary CTests pass in 22.68 seconds. All 268 source hashes and four fresh
Swift fixture binary hashes remain unchanged. Historical scoped pure coverage
above applies to identical implementation bytes; no new coverage, sanitizer or
SwiftPM execution is claimed. Portable runtime/parser/C++ ownership and SwiftPM
inputs are unchanged. No remote/native cancellation, foreign-peer, retirement,
overlap or fault probe was run. Production pins, quarantines, protected apps,
global SVG and OS-managed containers remain unchanged. Cancellation channel
teardown, deadlines, release policy, app adoption and full acceptance remain open.
