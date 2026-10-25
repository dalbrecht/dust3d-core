# ADR 0122: Sandbox-entitled healthy XPC fixture

Status: mitigation integration evidence for #173; no production adoption.

## Decision

Explicitly sign the private development XPC service with two entitlements:
App Sandbox=true and read-only access to exactly its controller peer-hash file.
The latter is a development temporary exception outside the sealed bundles,
inside the controller's private 0700 directory; the file remains 0600. No network,
broad directory, write, inherit or get-task-allow grant is added. The existing
host is unchanged in signing scope. This is not a release trust-distribution plan.

The controller inspects the embedded signature with explicit codesign XML output
and rejects any mismatch, extra key, broadened path or non-Boolean sandbox flag
before launch. The service independently queries its own SecTask App Sandbox
entitlement before reading peer configuration or accepting native work. Original
CFError domain/code/cause is retained on failure. Its scalar receipt carries this
startup verification; the host requires it alongside actual XPC peer PID and
native request/operation/status context. The existing peer hash requirements,
serial native queue, passed FileHandles and bounded C encoding/collection/semantic
copies remain in place.

Apple's [temporary exception reference](https://developer.apple.com/library/archive/documentation/Miscellaneous/Reference/EntitlementKeyReference/Chapters/AppSandboxTemporaryExceptionEntitlements.html)
describes exact absolute file paths and read-only grants. The
[App Sandbox entitlement](https://developer.apple.com/documentation/bundleresources/entitlements/com.apple.security.app-sandbox)
requests the platform sandbox; public SecTask SDK declarations define the runtime
entitlement query. The local codesign manual documents --xml for machine-readable
entitlement extraction. No private sandbox APIs or permission-denial attempts
are used.

This stage verifies exact signed configuration, runtime self-entitlement evidence
and healthy descriptor jobs. It does not independently audit all effective
permissions or prove behavior under a compromised service. No filesystem/network
denial, crash, signal, allocation-failure, broken-pipe or resource-exhaustion probe
is introduced. Production #67 block, dependency pin and every quarantine remain.

## Verification

Pure policy tests initially failed because the policy functions were absent.
They now pass in normal and optimized Python: minimal exact grants, independent
returned values, rejected extra/broadened paths/flags and invalid configuration
paths. A subsequent extraction regression requires explicit --xml and rejects
non-plist metadata. Four pure tests pass in each mode.

The clean audited build registers 135 ordinary contracts. Swift6 strict
concurrency/warnings and formatting pass. One automatic approval rejected the
initial native-check request after interpreting the historical TDD red log as a
current failed policy gate. The current policy gate was explicitly rerun green
before retrying the same check; no workaround or validation bypass was used.

The next attempt stopped before app launch: codesign's default abstract [Dict]
display is not a plist. Read-only inspection established that the signed grants
were correct. A regression first failed on the missing --xml flag, then passed
with the documented output option. Read-only parsing of the staged signature
also passed exact policy validation. Only the Python metadata reader/tests changed;
all compiled native/Swift input hashes remained identical, and the build was
rechecked. No service or native job ran in the prelaunch format failures.

The corrected focused healthy base/UV pair passes in 2.12 seconds: base 240 vertices
(host54138/service54139), UV 240 vertices/80 triangles (host54151/service54152).
Both runs verify embedded grants and the service's runtime sandbox entitlement.
The full 135 ordinary contracts then pass in 22.55 seconds. Service PIDs disappear
before owned temporary bundle removal; launchd service exit status is still not
independently observed. The controller never forces process termination.

No runtime/parser/C++ ownership implementation changed, so existing scoped pure
sanitizer evidence applies to identical source; no new XPC sanitizer, framework
failure-path coverage or comprehensive permission-enforcement claim follows.
All gated implementation/test/build inputs remain unchanged after documentation.
The prelaunch-only failed artifacts are removed only after retaining metadata,
logs and proof that the controller never reached launch. OS-managed containers
are not deleted or otherwise inspected. Generated build products are cleaned;
shared caches, source, stashes, checkpoints and protected QA apps remain intact.

## Remaining acceptance

Replace development peer provisioning for release; review effective sandbox and
resource policy independently; implement live pipes/backpressure, bounded service
admission, cancellation/deadlines, failure diagnostic transport and authoritative
transaction/publication admission. Reviewed Apple adoption and full M4/M8/editor
acceptance remain open. A runtime entitlement receipt is not a blanket security
certification and never substitutes for process exit or producer provenance.


## Serial delivery verification

Replayed after core #200 with accepted aggregate text admission, checked pure
metadata fixture access and GCC-portable quota fixture preserved. Current Swift,
controller and policy test source match the inspected corrected prepared stage,
including explicit codesign XML extraction. The historical Swift README hash
difference is saved-stage documentation only. The current audited macOS inventory
contains 136 ordinary contracts, adding the accepted pure text-quota regression.

Before any native/IPC run, all four current policy tests pass in both normal and
optimized Python, then nineteen pure C/C++ preflight contracts pass. A fresh clean
CMake build regenerates strict Swift 6 host/service binaries. The healthy XPC
pair passes in 0.98 seconds: UV host99887/service99888 returns 240 vertices and
80 triangles, and base host99897/service99898 returns 240 vertices. Each run
verifies exact embedded grants, service runtime App Sandbox entitlement and
actual peer PID. The controller observes host exit zero and service PID
disappearance before removing owned temporary bundles; service exit status is
not independently observed. All 136 contracts pass in 21.63 seconds, including
the XPC pair again. No new approval rejection, metadata prelaunch failure or
validation bypass occurs in these runs.

Source/binary hashes, strict Swift formatting, Python syntax and whitespace pass.
C++ runtime/parser/ownership and SwiftPM manifest inputs are unchanged from
accepted #200; prior scoped pure sanitizer/packaging evidence applies to identical
code, without a new engine/XPC sanitizer or coverage claim. Existing duplicate
resource-library warnings remain. No filesystem/network denial, failed-peer,
native cancellation, malformed native input, crash, signal, allocation-failure
or pressure probe is introduced. OS-managed containers and protected QA apps
are untouched by controller cleanup. This proves exact configuration and healthy
self-entitlement evidence, not comprehensive effective permission enforcement.
Production pin, #67 block, release/resource gates and all quarantines remain.
