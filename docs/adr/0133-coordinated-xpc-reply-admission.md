# 0133: Coordinate healthy XPC reply admission through the C queue

Issue: #188. Status: local implementation; serial public review pending.

## Decision

Use the #183 C coordinator in the existing development XPC host instead of
calling standalone service admission with a caller-supplied current identity.
WorkerServiceQueue is deliberately non-Sendable. Its opaque C queue, borrowed
collector and scoped decoded owner remain on the collecting thread. Only copied
request bytes/identity, scalar control observations and copied semantic arrays
cross concurrency boundaries. No unchecked Sendable wrapper is introduced.

For file jobs, the main collecting thread submits/begins before writing request
bytes or invoking the service. For socket jobs, the reader prepares the queue
and captures its nonzero connection generation before publishing a copied
WorkerServiceRequest through the retained multi-reader prepared event. The
writer and main thread both join that event; neither sends bytes/submits the
service before preparation. The request/control correlation uses the queue's
captured copied identity, not an identity reconstructed from reply bytes.

Framing borrows the collector only inside a synchronous closure on its owner
thread. After actual EOF and joined authenticated-connection generation/OS PID
and service control receipt, withDecodedReply admits through the queue-current
source gate. It copies values while an independent decoded owner is alive and
releases that owner even when a Swift consumer throws. The existing base/UV
summary, finite corners, peer, sandbox, channel cleanup and service-disappearance
checks remain. The controller additionally requires queue-admission evidence.
No new service method, signing grant, endpoint or native work is introduced.

The Swift boundary preserves operation, status, source, code, truncation and
owned opaque diagnostic bytes. Native errors and consumer errors are not wrapped
into generic failures. Missing private borrowed views have a separate typed
boundary diagnostic; no corruption is injected to force that invariant path.
Cancel/close retire publication only. Matching transport abandonment is available
after caller cleanup; it does not stop native execution or collect descriptors.

## Verification

The first pure Swift consumer failed on the missing owner. Strict Swift 6,
complete concurrency, warnings-as-errors and formatting pass. Its real C consumer
uses small independent wire fixtures without engine/I/O work. It covers request
copying, stale-current rejection, newer waiting work, independent original binary
cause bytes, idle/busy, retirement, closed submission, foreign/matching abandonment,
absent collectors, unsupported quota/zero-generation preservation and original
Swift consumer-error propagation. It creates no child process or failed peer.

The source quarantine guard passes. The 143-case inventory is the previously
audited 142 ordinary cases plus this pure Swift consumer. Existing C++/engine/
service inputs remain byte-identical; only host wiring, controller evidence and
new Swift/CMake inputs changed. Fresh builds and 12 pure preflight checks pass.
All four existing healthy base/UV file/socket pilots pass (1.96 seconds), with
240 vertices and 80 UV triangles for UV, actual EOF/OS peer and queue-admission
markers. Normal owned host/service cleanup completes. Full 143 ordinary CTest
passes (24.87 seconds). Current runtime/helper SwiftPM packaging passes without
helper execution; the native build-system deprecation warning remains.

Focused Swift owner coverage is 71/71 lines, 32/33 regions and 16/16 functions.
Swift reports no branch-outcome metrics; do not translate that to branch coverage.
The missing-view invariant region remains unforced. No new C++ source changed;
retained supported pure C++ sanitizer evidence applies to identical C coordinator
code, not Swift/XPC/framework behavior. No full-engine coverage, XPC sanitizer,
real failure-path or effective sandbox-containment claim follows.

## Serial delivery verification

Replayed onto accepted core #211, preserving the UV aggregate text budget,
checked UV fixture indexing and both decoded/coordinator explicit-array
portability corrections. The quarantine guard and source audit pass with 144
ordinary registrations, including the accepted pure quota regression and new
Swift queue consumer. All four XPC cases retain their shared resource lock.
A fresh clean build in the single shared root passes strict Swift 6/lint.

Normal and optimized policy suites each pass four tests. Pure Swift queue,
outcome/admission/channel checks pass, followed by 21 pure C/C++ contracts.
All four healthy file/socket base/UV XPC cases pass in 2.42 seconds and require
queue-authoritative admission evidence. Socket base returns 240 vertices
(host29457/service29458); socket UV returns 240 vertices and 80 triangles
(host29468/service29469). Both join writer completion, actual EOF decoding and
control receipt; all cases match actual peer identity. Owned cleanup follows
host exit and service PID disappearance, not independently observed service exit.

All 144 ordinary CTests pass in 25.06 seconds. All 278 source hashes and six fresh
Swift fixture binary hashes remain unchanged. Python syntax and whitespace checks
pass. C++ runtime/parser/ownership and SwiftPM inputs are unchanged from #211;
its focused pure sanitizer/build evidence applies to identical code, with no new
sanitizer, SwiftPM or coverage execution claimed here. Earlier Swift coverage is
historical scoped evidence only. No native cancellation, foreign-peer, retirement,
overlap, fault or pressure probe was run. Production pins, quarantines, protected
apps, global SVG and OS-managed containers remain unchanged. The remaining editor
and containment acceptance below is still open.

## Remaining acceptance

The fixture owns one connection and one native job. Pure source/cancel tests do
not prove real native cancellation, multi-peer ownership or multi-document fairness.
Observation deadlines do not bound blocking I/O, native CPU or RSS. Service PID
disappearance is not independently observed launchd exit status. Original real
transport failure/cleanup paths remain unforced. Release signing/trust, resource
policy, geometry-equivalent document revisions, per-document scheduling, reviewed
pin adoption and actual editor publication/open-edit-save-reopen acceptance remain
open. Full M4–M8/editor parity is incomplete.

Production pin41af7d4c and core #67/#69 GLB, #83 writer, #84 fault/allocation,
#95 animation and Apple #199 rig-pose quarantines remain unchanged. No crash,
signal, broken-pipe, allocation-failure, resource-exhaustion, real failed-peer,
native cancellation or native invalid-input probes run. No protected QA app,
global SVG, native Codex inspection or authentication warning is bypassed.
