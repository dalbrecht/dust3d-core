# 0131: Bounded persistent-service request queue

Issue: #182. Status: local implementation; serial public review pending.

## Decision

Extend the existing RequestQueue instead of introducing parallel source scheduling
state. beginService freezes a nonzero connection generation with the active
header before submission. The queue still retains one active header and one
latest waiting archive; begin transfers the archive to its transport owner.
The zero-generation sentinel remains reserved for child-process completion.
Invalid service configuration consumes no waiting work; a busy begin cannot
change the active generation.

completeService first requires matching active identity and service mode. Foreign
or wrong-mode completion keeps active work intact. Matching completion releases
the slot, then PendingServiceReply checks the queue's authoritative current
identity, captured generation, peer/receipt/EOF and owned semantic data. Stale,
cancelled, closed, malformed or rejected work cannot publish or replay. Waiting
latest work survives old/foreign cancellation and failed admission. abandon
releases matching collected work without publishing and clears service mode.
The existing child complete path still requires observed successful child exit;
service work cannot use it as a bypass.

The externally serialized caller still owns transport collection/cleanup and
child reaping where applicable. These methods do not start/stop jobs, authenticate
observations, cancel native execution or publish editor state. They currently
belong to the CMake protocol library; the C ABI and SwiftPM runtime do not expose
this queue. Real host/editor coordinator adoption, per-document fairness, source
projection policy, lifecycle/deadlines/resources and release trust remain open.
Metadata/history-only edits must not unnecessarily retire equivalent geometry.

## Verification

The new pure regression first failed to compile for missing service queue APIs.
Tests cover generation capture across busy begin, mode/foreign completion
rejection, latest waiting replacement, stale result suppression, changed service
lifetime, owned original failure bytes, successful completion/replay, cancel,
close, malformed-current admission, abandonment and recovery. Empty/foreign/old
cancellation cannot discard a newer source. These are bounded protocol/state
values, never actual native cancellation, peer failure or process observations.

Focused ASan/UBSan passes with supported macOS settings (no leak-check claim).
WorkerQueue.cc measures 87/87 lines, 72/72 regions, 11/11 functions and 48/48
branch outcomes. This is scoped queue coverage, not whole-core coverage.
Root clang-format and diff checks pass. Fresh build, nine current pure checks
(1.05 seconds) and all 140 source-audited ordinary CTest cases (22.63 seconds)
pass with matching source hashes. Existing healthy XPC jobs remain compatible;
they do not invoke the new queue API yet.

No native failure/cancellation/denial/multi-peer/overlap, crash, signal,
allocation-failure or pressure probe ran. Production pin and all quarantines
remain unchanged. Full editor/M8 acceptance is incomplete.

## Serial delivery verification

Replayed onto accepted core #209, preserving the accepted aggregate UV text
budget, checked UV fixture indexing and portable decoded-budget initializer fixes.
The quarantine guard and source audit pass with 141 ordinary registrations,
including the accepted pure quota regression. All four XPC cases retain their
shared development-host resource lock. A fresh clean build in the single shared
root and root clang-format checks pass.

Normal and optimized policy suites each pass four tests. Pure outcome/admission/
channel checks and 19 pure C/C++ contracts pass. A fresh focused queue test linked
only to pure protocol/mesh/UV/failure/admission code passes ASan/UBSan with
detect_leaks=0; no leak-check or native-engine sanitizer claim follows. Four
unchanged healthy XPC file/socket base/UV regressions pass in 2.15 seconds, each
requiring actual EOF, OS-peer observation and service C admission. Socket base
returns 240 vertices (host12309/service12314); socket UV returns 240 vertices and
80 triangles (host12324/service12325). These jobs do not adopt the new queue yet.
Owned cleanup follows host exit and service PID disappearance; service exit
status is not independently observed.

All 141 ordinary CTests pass in 22.19 seconds. All 271 source hashes and six fresh
fixture/sanitizer binary hashes remain unchanged. Earlier coverage remains scoped
historical evidence, not a new measurement. SwiftPM inputs are unchanged and do
not expose this queue; no new SwiftPM run is claimed. Pure cancellation/peer/
malformed-codec values do not invoke native or OS failures. No native cancellation,
foreign-peer, retirement, overlap, fault or pressure probe was run. Production pins,
quarantines, protected apps, global SVG and OS-managed containers remain unchanged.
C/Swift coordinator and editor adoption, lifecycle/resource/release policy and
full acceptance remain open.
