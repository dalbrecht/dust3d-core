# ADR 0120: Isolated XPC static mesh host verification

Status: development integration evidence for #171; no production adoption.

## Decision

Add a macOS-only CMake integration fixture, not a shipping app or core dependency
on Apple frameworks. Fresh strict Swift6 test binaries form a private host.app
with an embedded XPC service. The Python controller stages both only under the
single shared build root, explicitly ad-hoc signs service then host, and verifies
the sealed bundles. Fixed test identifiers are reused; no installed app, launchd
registration, credential, GUI control or production bundle is modified.

The controller provisions both exact CDHashes in a private 0600 sidecar outside
the sealed bundles, inside its 0700 temporary directory. The Swift fixture bounds
and validates this regular, same-user, non-symlink configuration and compiles the
requirement before using the Foundation setter. The host binds the service
connection; the service binds each incoming connection before activation and
checks same-user identity. This is explicitly DEVELOPMENT configuration, not a
production trust-distribution/root-of-trust scheme. Release signing/configuration
and compromised-peer behavior remain separate acceptance gates.

The local SDK permits connection-level requirements for these connections but
restricts listener-level requirements to anonymous/named Mach listeners; the
fixture never applies the latter to the service listener. Apple's
[Creating XPC Services](https://developer.apple.com/library/archive/documentation/MacOSX/Conceptual/BPSystemStartup/Chapters/CreatingXPCServices.html)
describes the private embedded service and NSXPC interface model. The current
Foundation SDK headers also establish FileHandle's NSSecureCoding support and
connection-requirement availability. The fixture uses exact FileHandle arguments
and scalar reply fields, with no custom object graph or message archive.

One healthy licensed Capsule request is written to an owned regular file and
passed through duplicated FileHandles. The service reuses `dust3d_worker_serve`
on a single serial queue shared across its connections. Its NSXPC handle copies
live through that invocation and ARC releases them afterward; the parent's own
handles stay valid. Control receipts carry native status/op/request/error/context
scalars and actual service PID. No arbitrary body Data object travels over XPC.

After receipt, the parent requires the actual distinct service PID reported by
NSXPC, expected operation/request and complete-write status. It reads the reply
file in sizes capped by the shared C collector, observes actual EOF, decodes via
the semantic C owner and copies the 240 vertices. It never calls native generation
in the parent or fabricates a ProcessOutcome for PendingReply. The host prints
fixture evidence only; no app state or publication admission changes.

The host invalidates its connection on scope exit. The service schedules its
normal exit behind in-flight serial work. The controller observes host exit zero
and service PID disappearance before deleting its own temporary bundle directory.
Service exit STATUS is not independently observed through launchd; PID absence
must not be reported as an observed zero process exit. Timeout/failure artifacts
and live processes are preserved without a kill/terminate path or human request.

## Verification

The requested CMake fixture target initially did not exist (red). New Swift
sources pass strict Swift6 concurrency/warnings and formatting checks. Fresh
source quarantine guard and registered-source audit include 130 ordinary tests;
all relevant runtime/test/build source hashes remain unchanged throughout gates.
The clean build passes. The focused isolated job passes in 0.69 seconds: host
PID38533, service PID38548, both peer observations38548, 240 vertices. The service
PID subsequently disappears and owned bundles are removed. All 130 ordinary
CTest contracts then pass in 21.81 seconds, including the isolated fixture again
in 0.32 seconds. These are two healthy acceptance runs, not crash/liveness probes.

No runtime/parser/C++ ownership implementation changes were made. Existing
scoped pure sanitizer evidence from ADR0119 remains applicable to unchanged
code; no full-engine or XPC sanitizer/coverage claim is added. No allocation-
failure, malformed native archive, crash, signal, broken-pipe or resource-
exhaustion probes, stale binaries or protected apps were used. SDK/source/docs
and gate logs are retained; production pins and every quarantine remain unchanged.

## Remaining acceptance

This verifies a real separate engine process, development peer binding and the
bounded descriptor/C semantic path on macOS arm64. It does not verify live pipes,
backpressure/cancellation/deadlines, bounded service scheduling, failure receipt
transport, authoritative authenticated transaction/publication admission, sandbox,
resource/stdio policy, release trust, notarization or other platform runtime parity.
The service is not claimed sandboxed merely because it is XPC. Those gates and
reviewed Apple SDK/editor adoption remain required for #123/M8 and full parity.


## Serial delivery verification

Replayed after #198 with accepted aggregate text admission, checked pure metadata
fixture access and the GCC-portable typed quota fixture preserved. Current Swift
host/service/common/controller sources match the inspected prepared stage. The
current audited macOS inventory contains 131 ordinary contracts, including the
accepted pure text-quota regression. A fresh clean CMake build regenerates both
strict Swift 6 binaries; source and binary hashes remain unchanged through gates.
Seventeen pure preflight checks pass before native/IPC work.

The focused healthy Capsule XPC fixture passes in 0.48 seconds: parent PID62175,
service PID62177, and the actual connection peer PID62177 agree; 240 vertices are
copied through the shared C path. The controller observes host exit zero and
service PID disappearance before removing its own temporary bundles. Service
exit status is not independently observed. The full 131-contract gate then passes
in 31.36 seconds, including another normal XPC run. No timeout, forced shutdown,
failed peer, native cancellation, malformed native input, crash, signal,
allocation-failure or pressure probe is introduced.

Strict Swift formatting, Python syntax, whitespace and recorded source/binary
hashes pass. Runtime/manifest source is unchanged from accepted #198; its scoped
pure sanitizer/SwiftPM evidence applies to identical code, without a new engine,
XPC sanitizer or coverage claim. Existing duplicate resource-library warnings
remain. Development signatures/pins are fixture evidence, not release trust or
sandbox/resource containment. Production pin, protected apps, global SVG and
all quarantines remain unchanged. Full M4/M8/editor acceptance remains open.
