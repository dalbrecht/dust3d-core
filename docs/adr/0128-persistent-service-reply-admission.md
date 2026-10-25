# 0128: Persistent service reply admission

Issue: #179. Status: local portable implementation; public review pending.

## Decision

Keep PendingReply's observed successful child-exit requirement. Persistent XPC
services need a separate PendingServiceReply rather than an invented ProcessOutcome.
The new gate captures the original validated request and a nonzero caller-selected
connection generation. Completion retires first, then requires the authoritative
active document/request/revision, matching connection generation, positive actual
OS-observed peer PID, receipt PID correlation, actual response EOF and matching
receipt operation/request. Nonzero runtime/admission/errno/stage/source/detail/
truncation/finalization values reject with their original numeric diagnostics.

Only then does a shared decoder validate exact frame identity and bounded mesh,
paired UV or typed failure semantics. Returned data owns its buffers independently
of collected bytes. A frame-written typed failure is a completed transaction,
not successful geometry. Cancellation/replacement/closure can retire the gate
before any completion. Every rejection remains retired; retry cannot publish.

This gate does not authenticate peers or manufacture OS evidence. The transport
caller must bind its code-signing-enforced connection, actual process identity,
collector EOF and current application source. Receipt fields cannot substitute
for these observations. It neither observes service exit nor collects/stops/reaps
processes or publishes app state. Currently it belongs to the CMake protocol
library, with no new C ABI or SwiftPM runtime exposure. Real C/XPC/Apple broker
integration, deadlines, cleanup/resource policy and release trust remain open.

## Evidence and limitations

The new regression first failed to compile for the missing service types. Pure
bounded values then cover original failure bytes, base/UV ownership, quotas,
foreign peer/generation, absent EOF, all three stale identity fields, receipt
operation/request mismatch, each diagnostic field, original numeric causes,
retirement/replay, invalid session and malformed frame. No OS peer/process or
native failure is produced in those tests. The old child-process admission tests
continue to require real ProcessOutcome input.

Focused ordinary ASan/UBSan passes. An initial sanitizer startup stopped because
macOS does not support detect_leaks=1; supported ASan/UBSan settings then pass.
No leak-check claim follows. WorkerAdmission.cc measures62/62 lines,71/71 regions,
7/7 functions and48/48 branch outcomes in the focused pure-value profile; this is
not whole-engine coverage. Root clang-format and diff checks pass. Fresh build,
five current pure policy/admission checks0.68s and all139 audited ordinary CTest
cases22.56s pass with matching source hashes. Existing healthy XPC jobs remain
unchanged and do not exercise this new gate yet.

No native cancellation, rejected/multiple-peer, overlap, crash, signal,
allocation-failure or resource-exhaustion probe ran. Production pin and all
quarantines remain unchanged. Full editor/M8 acceptance is incomplete.

## Serial delivery verification

Replayed onto accepted core #206 with the accepted UV aggregate text budget,
checked UV fixture indexing and portable decoded-budget initializer fixes retained.
The quarantine guard and audit pass with 140 ordinary registrations, including
the accepted pure quota regression. All four XPC cases keep their shared
development-host resource lock. A fresh clean build in the single shared root
passes; root clang-format and whitespace checks pass for the changed C++ files.

Normal and optimized policy suites each pass four tests. Pure state/channel
checks and 19 pure C/C++ contracts pass. A freshly compiled focused admission
test linked only to the pure protocol/mesh/UV/failure codecs passes ASan/UBSan
with detect_leaks=0; no leak-check or native-engine sanitizer claim follows.
Four unchanged healthy file/socket base/UV XPC regressions pass in 1.91 seconds.
Socket base returns 240 vertices (host38650/service38651); socket UV returns
240 vertices and 80 triangles (host38661/service38664). Both join writer, actual
EOF decoding and control receipt. These jobs do not exercise the new gate yet.
Owned cleanup follows host exit and service PID disappearance; service exit
status is not independently observed.

All 140 ordinary CTests pass in 23.93 seconds. All 268 source hashes and five
fresh fixture/sanitizer binary hashes remain unchanged. The earlier focused
coverage is historical scoped evidence, not a new measurement. No SwiftPM input
changed or new SwiftPM run is claimed. Pure malformed-frame and diagnostic-value
tests do not invoke native input parsing or OS failures. No native cancellation,
foreign-peer, retirement, overlap, fault or pressure probe was run. Production
pins, quarantines, protected apps, global SVG and OS-managed containers remain
unchanged. C/Swift/XPC/app integration and full acceptance remain open.
