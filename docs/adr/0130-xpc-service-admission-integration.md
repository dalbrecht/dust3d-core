# 0130: Real XPC host service admission

Issue: #181. Status: local implementation; serial public review pending.

## Decision

The healthy isolated host now creates service-specific C collectors before job
submission and invokes service_admit after actual EOF plus the control receipt.
File collectors stay on the main host thread. Socket collectors are created,
framed, admitted, copied and destroyed on the reply-reader queue; a preparation
event confirms their creation before service invocation. No opaque C owner crosses
Sendable queues. Only copied vertices/UV arrays and scalar identity values cross.

WorkerXPCPeer owns NSXPCConnection on an actor, including requirement installation
before activation, proxy access, PID observation and invalidation. Strict Swift6
first rejected a non-Sendable connection capture; actor ownership fixes it without
an unchecked wrapper around Foundation. The callback transfers scalars to the
actor, which reads the real connection.processIdentifier separately from reported
receipt PID. The C gate correlates both with the captured generation and request
identity. Framing/admission failures retain source/code/truncation/original cause.

WorkerOutcome retains one first-result completion behind a lock and DispatchGroup.
Both parent and reader can observe it, before or after completion, without a
consuming semaphore wake. Duplicate finish cannot overwrite the original cause;
observation timeout neither retires nor restarts work. XPC invalidation runs on
the actor and joins the existing pure operation/cleanup result composition. This
is invalidation acknowledgement, not observed service exit status.

## Evidence

The missing outcome regression first fails to compile. Pure tests verify repeated
reads, two bounded readers, first-result wins, original NSError identity and a
late completion after observation timeout. Strict Swift6 concurrency/warnings and
formatting pass. The thin outcome profile measures25/31 lines and11/17 regions;
error-description presentation and the defensive missing-result invariant are not
fully exercised. Swift reports no branch outcomes. No coverage target or platform
failure-injection claim is made. C++ implementation is unchanged from #180, so no
new native sanitizer run/claim follows.

Eight current pure policy/C/outcome checks pass0.84s. Four freshly built healthy
XPC pilots pass1.73s. Every pilot prints its C-admission acceptance marker after
actual EOF and OS-peer correlation; the controller requires that marker, verifies
host exit0 and observes the owned service PID disappear before deleting bundles.
Capsule results retain240 vertices and80 UV triangles. All140 source-audited
ordinary CTest cases pass24.52s, with matching source hashes. No UI/app process is
controlled. Core CMake registration adds only the pure outcome test.

## Serial delivery verification

Replayed onto accepted core #208 with the accepted UV aggregate text budget,
checked UV fixture indexing and portable decoded-budget initializer fixes retained.
The quarantine guard and source audit pass with 141 ordinary registrations,
including the accepted pure quota regression and the new pure outcome test.
All four XPC cases retain their shared development-host resource lock. A fresh
clean build in the single shared root passes strict Swift 6 compilation/lint.

Normal and optimized policy suites each pass four tests. Pure outcome/admission/
channel checks pass, followed by 19 pure C/C++ contracts. All four healthy
file/socket base/UV XPC cases pass in 2.26 seconds. Every case prints the required
C-admission marker after actual EOF and OS-peer observation. Socket base returns
240 vertices (host4744/service4750); socket UV returns 240 vertices and 80 triangles
(host4759/service4760). Both join writer, EOF decoding and control receipt.
Owned cleanup follows host exit and service PID disappearance; service exit
status is not independently observed.

All 141 ordinary CTests pass in 24.51 seconds. All 271 source hashes and five fresh
Swift fixture binary hashes remain unchanged. Python syntax and whitespace checks
pass. C++ runtime/parser/ownership and SwiftPM inputs are unchanged from #208;
its focused sanitizer/build evidence applies to identical code, with no new
sanitizer, SwiftPM or coverage execution claimed here. No real cancellation,
foreign-peer, retirement, overlap, fault or pressure probes were run. Production
pins, quarantines, protected apps, global SVG and OS-managed containers remain
unchanged. The immutable single-source fixture does not establish editor adoption
or the remaining gates below.

## Remaining gates

The fixture has one immutable source and connection (generation1), not an editor
broker with mutable current document, replacement/cancellation or reconnect policy.
The shared monotonic deadline bounds outcome observations, not blocking descriptor
I/O or whole-job CPU/RSS. Authenticated current-source app publication, failure/
cancellation/descriptor teardown, deadlines, resource/release trust and reviewed
Apple adoption remain open. PID disappearance is not reaped service exit status.
No real rejected-peer/multi-peer/overlap/native cancellation, crash, signal,
allocation-failure or pressure probe ran. Production pin and every quarantine
remain unchanged. Full editor/M8 acceptance is incomplete.
