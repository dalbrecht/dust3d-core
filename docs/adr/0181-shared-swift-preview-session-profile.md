# ADR 0181: Shared Swift preview session profile

Status: proposed for reviewed delivery. Tracks #270 and Apple #279/#280.

## Decision

Add an opt-in Swift backend with explicit legacy and segmented-preview job/receipt
cases. Both delegate to the existing connection-local C session. The existing generic
coordinator holds its lock through the complete native call, reset, close and destruction;
there is one cache owner and one replay history, not a second preview session per peer.
Captured identity, connection generation, original native controls and independent input/
output chunk policies are passed to the accepted C serving entry. Keep preview receipts
in their original C type rather than mapping their status numbers into legacy job status.

Preserve the old adapter and active XPC service route. This backend is the prerequisite
for an explicit XPC selector with separately typed RPC receipts and independent peer/
process/current-source publication admission. It is not a production Swift SDK, app backend
switch, deadline or cancellation implementation. Apple pin48b0ce2d and quarantines remain.

Expose the accepted preview header in the CMake-generated consumer module map as well
as the canonical runtime module map. Do not hand-edit generated files.

## Evidence and limitations

The strict Swift smoke first failed to compile because the mixed backend was absent.
The existing licensed tiny Capsule then exercises legacy request1, segmented request2
with actual cache reuse and identical copied geometry/source identity, explicit reset,
legacy request3 with actual rebuild, close/destruction and Sendable actor transfer.
Both result variants are pattern matched before accessing the original receipts. No new
native fixture, malformed-native-input or fault/pressure probe is introduced.

Fresh full CMake build and227/227 CTest pass (37.46s). The actual SwiftPM runtime build
(0.17s cached) and freshly linked strict Swift6 mixed-profile consumer pass; strict
formatting and diff checks pass. The generated module-map omission was found and fixed.
An earlier chained command incorrectly ran the previous consumer after compilation failed;
that stale result is excluded. The later fail-fast fresh build and full gate are the evidence.

No C++ ownership/parser implementation changed, and no new sanitizer claim is made.
Coverage percentages for the new Swift adapter are not measured; lifecycle error branches
remain covered only by the existing generic fake-backend coordinator tests, not a claim of
full native error coverage. Real XPC preview delivery and independent admission, public SDK
packaging, native-budget acceptance and baked/imported profiles remain separate work.
M4–M8/full editor parity and release acceptance are incomplete.
