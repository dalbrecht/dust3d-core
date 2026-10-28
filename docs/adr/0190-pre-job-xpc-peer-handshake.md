# ADR 0190: Bind authenticated XPC identity before job channels

## Decision

Add a scalar-only handshake to the exact-revision shared XPC protocol and async client.
The service claims the requirement-checked connection candidate under the existing
ownership lock, without creating a native session, acquiring a job lease or consuming a
request ID. Same-owner repetition succeeds; foreign/retired ownership retains its original
admission status. Owner invalidation still orders normal cleanup/retirement after admitted
work. Handshake acceptance is ownership only, never native-idle or job acceptance.

The client captures its positive fixed generation and independently observes Foundation's
OS processIdentifier on the correlated callback. Require matching positive reported PID,
verified sandbox self-check and accepted ownership; reject/retire invalid lifetimes with
an immutable original handshake receipt. Return the owned PID/generation/observation so a
future executor can retain it after control failure. Existing job methods remain compatible
and do not implicitly perform a handshake; the real executor must explicitly call it before
channel creation. Repeated handshake uses RPC tokens, not the caller's job request sequence.

QA hosts now use the real client handshake before their existing job-submission closure,
and the healthy verifier requires this evidence. Fixture files/channels may be prepared
outside that closure; this is not evidence that all QA fixture allocation occurs after
handshake. Future production execution must use the stronger ordering. Job/control/EOF/
current-source admission, native drain and actual process disappearance remain independent.
No production signing/trust or Apple route/pin change is implied.

## Verification

Verified missing client/scheduler APIs red. Ten new fake client cases cover stable/repeated
identity then first job, original rejection fields, missing independent peer, fixed generation,
changed peer, duplicate/late callbacks, overlap, caller cancellation, original remote errors,
interruption and missing proxy. A pure recording-scheduler case proves zero lease/generation
consumption, first job ID1 acceptance, foreign-owner rejection and retirement ordering.
No actual failed-peer, overlap, cancellation, malformed native or fault probes occur.

Selected fake tests29 pass. LLVM function-filtered client handshake report:68/70 reported
line entries97.14%,17/18 regions94.44%,all5 functions executed; the two unforced lines are
weak-self actor disappearance. Service claim functions:14/14 reported line entries,6/6 regions,
all2 functions executed. Nested function line entries overlap; these are not unique physical
line counts or whole-file/core/branch coverage claims. Matched executable/profile retained.
Real service handler coverage is not inferred from that fake report.

Final fresh ordinary build and233/233 CTest pass46.81 seconds, including51 importing public
XCTest cases and every existing healthy file/socket/base/UV/session/procedural XPC profile.
Actual healthy handshakes precede job submission and retain the same independently observed
peer; ordinary cache/reset/EOF/normal-process-retirement gates still pass. Actual-product and
unsigned-bundle gates pass. Strict formatting/diff and34 frozen inputs match; normal build
flags restored after profiling, one shared build root. No C++ parser/native-owner change or new sanitizer claim.

One earlier selected build stopped before tests because SwiftPM reported ServiceAdmission
modified during compilation. Its bytes matched HEAD; after reading/freeze, content and mtimes
matched during the subsequent successful selected run. Cause remains unproven; the failed run
was not counted. A test-only trailing-closure lint issue was corrected before final gates.
No source metadata was forcibly restored or compiler check bypassed.

## Remaining work

The descriptor executor must join writer completion, actual semantic EOF, authenticated
control receipt, current-source admission and owned cleanup. Control error never proves
native/process drain; retained handshake PID alone is not process-exit evidence. Streaming
preview/session submission still needs explicit stream-mode plumbing rather than the current
file-backed defaults. Deadline/remote-work supervision, actual signed embedding/release trust,
Apple adoption, all quarantines and M4–M8 acceptance remain open. Tracks #294/#270/#123.
