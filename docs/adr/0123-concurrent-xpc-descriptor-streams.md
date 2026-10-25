# ADR 0123: Concurrent healthy XPC descriptor streams

Status: development integration for #174; no production adoption.

## Decision

Keep the regular-file reference cases and add healthy base/UV jobs over anonymous
Unix stream-socket pairs. The host creates separate request/reply channels,
configures close-on-exec and per-socket SO_NOSIGPIPE, and reads those settings back
before any writes or descriptor transfer. It closes the unused reader write
direction. No global signal disposition, network binding, permission grant or
sandbox entitlement changes. Apple's [socket option reference](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/setsockopt.2.html)
documents the per-socket option; it is configured/inspected here without any
closed-peer or signal-delivery test. This is not evidence of tested signal-fault
containment.

The parent writes bounded 4096-byte request chunks while a separate worker drains
the reply through the shared C header-first collector and semantic owner. The
service still serializes native entry and passes only scalar control fields.
After all runtime writes return, it orderly shuts down the output write direction
so EOF does not depend on NSXPC's argument-retention timing or other descriptor
aliases. It reports finalization errno separately, preserving the native result.
No body Data object travels over XPC; FileHandles transfer the already configured
channel capabilities under the same exact sandbox and development peer checks.

The parent requires a successful writer result, actual EOF plus valid correlated
semantic data, and the connection's expected scalar receipt/actual distinct peer
PID. These may arrive in either order; all waits share one monotonic deadline.
The read task creates/destroys C owners locally and returns only immutable
Sendable vertex/UV arrays and counts. No opaque owner crosses queues. A callback
still cannot fabricate ProcessOutcome or authorize editor publication.

Owned read/write tasks close their endpoint before delivering their result,
including read rejection. This prevents an unread endpoint from remaining open
until the overall deadline when the other side is writing. Pure result composition
preserves the original operation error, close error or both independently. The
controller still preserves live processes/artifacts on timeout and never forces
termination. Cancellation, hostile-peer behavior and end-to-end failure recovery
are not established by these healthy cases.

## Verification

The channel fixture initially failed because the factory was absent. A subsequent
channel-only check found ENOTCONN in a redundant unused read-side shutdown; that
call was removed before any data/engine job. The remaining unused write-direction
closure and both channel options pass. The fixture performs no failed-peer write.
Strict Swift6 concurrency/warnings and formatting pass. A generic Error conversion
and later unused binding were corrected at compile time before native execution.

Fresh source quarantine guard/audit includes 138 ordinary registrations. All four
XPC cases share the same resource lock while recycling fixed development bundle
IDs. Exact sandbox policy/extraction tests and channel metadata pass before jobs.
The clean build and focused healthy file/socket base/UV cases pass (2.22 seconds).
Socket base returns 240 vertices (host62001/service62002); socket UV returns
240 vertices/80 triangles (host62014/service62015), with actual peer and runtime
sandbox receipts and joined writer/EOF/control evidence. Initial full inventory
passes in 22.51 seconds.

Code review then found that the read endpoint was closed only after the main wait.
A pure value-composition test first failed on the missing resolver, then verified
all four success/rejection combinations with typed errors, without OS failure
injection. Ownership wrappers now close both read/write endpoints before outcome
publication. Only three Swift fixture files changed; portable runtime inputs were
unchanged. The affected Swift targets were rebuilt; final preflight and all 138
ordinary contracts pass in 16.23 seconds. All gated input hashes remain equal
before documentation-only README/plan/ADR/changelog updates.

No runtime/parser/C++ ownership implementation changed. Existing scoped pure
sanitizer evidence remains applicable to identical source; no new XPC sanitizer,
exceptional I/O coverage, buffer-pressure or comprehensive security claim follows.
Per-socket suppression is not exercised by a broken-peer test. No malformed native
archive, allocation-failure, signal, crash, resource-exhaustion or denial probe,
stale binary or protected app is used. Temporary bundles are removed after host
exit and service PID disappearance; launchd service exit status is not independently
observed. Shared caches, all source, stashes and checkpoints remain intact.

## Serial delivery verification

Replayed this stage onto accepted core #201 while retaining the aggregate UV text
budget, checked UV fixture indexing and portable decoded-budget initializer fixes
from #190, #191 and #198. A fresh clean build uses the single shared build root.
The quarantine guard and source inventory audit pass with 139 ordinary tests,
including the accepted pure text-budget regression. Both normal and optimized
sandbox policy suites pass all four cases; 19 pure contracts and the channel
metadata/result-composition check pass before healthy native jobs.

All four healthy XPC cases pass in 2.01 seconds. Socket base returns 240 vertices
(host7286/service7290); socket UV returns 240 vertices/80 triangles
(host7300/service7301). Both join writer completion, actual EOF semantic decoding
and the control receipt. File base/UV cases also pass, with actual peer identity
and sandbox self-entitlement evidence. Full CTest passes all 139 tests in
22.22 seconds. Source hashes and all three freshly built Swift fixture binary
hashes remain unchanged across validation. Owned temporary bundles are cleaned up
after host exit and service PID disappearance; service exit status is not observed.

Portable runtime/parser/ownership and SwiftPM inputs are unchanged by this stage;
prior scoped sanitizer/build evidence applies to identical inputs, with no new
sanitizer or SwiftPM execution claimed here. No failure, pressure, cancellation,
signal, allocation or malformed-native-input probes were added. No protected app,
global SVG or OS-managed container was touched. Production pins and quarantines
remain unchanged. This is development integration evidence, not editor acceptance.

## Remaining acceptance

Implement bounded service admission, cancellation/deadlines and failure diagnostics,
then authoritative transaction/current-document publication policy. Review release
trust, effective sandbox/resource constraints and Apple adoption separately.
All production pins, #67 block and quarantines remain unchanged; full M4/M8/editor
acceptance is incomplete.
