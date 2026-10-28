# ADR 0182: Segmented preview XPC endpoint

Status: proposed for reviewed delivery. Tracks #270 and Apple #279/#280.

## Boundary and ownership

Add an explicit `servePreview` selector to the development XPC service protocol.
Capture document/request/revision/connection identity, original indexed/source native
controls and independent request/reply transfer/chunk policies as scalar RPC arguments.
Use exact UInt32 chunk widths rather than truncating a UInt64 conversion. The service
uses the accepted mixed backend: legacy and preview jobs share one connection-owned
session/cache/replay state under the existing coordinator lock and admitted scheduler.
Retain both borrowed FileHandles through queued work; finalize an output stream only
after the native call returns. Existing legacy selectors retain their result domains.

The preview callback has its own status domain. It preserves original detail source/code,
errno/stage, truncation and opaque cause. A separate boundaryStatus retains original C
session-creation decode classification, rather than treating it as preview/legacy status.
Swift lifecycle errors and scheduling admission remain distinct. The peer actor records
connection generation from its captured input and process ID from NSXPCConnection;
neither is accepted from the callback as an independent observation.

Add a collecting-thread, metadata-only, one-shot publication gate. The caller authenticates
the connection, decodes using captured identity/context/policies through actual EOF, then
supplies current source. The gate retires before checking current identity, EOF, observed
peer/lifetime, scalar control and successful finalization. Rejection retains the complete
original preview receipt. It neither queues archives nor reconstructs legacy frames.
The gate alone does not authenticate a connection, observe EOF or publish app state.

## Healthy integration

The existing small licensed Capsule session fixture now runs legacy request1, segmented
preview request2 on the same authenticated sandboxed peer, explicit reset3, then legacy4.
It verifies actual cache reuse and identical indexed positions, indices and source-corner
correspondence, then actual rebuild after reset. Preview archive/reply paths do not use
the legacy RequestQueue frame or collector. Original chunk/archive/semantic policies are
captured separately. Copy output only after C framing/semantic/actual EOF validation and
independent OS-peer/current immutable-fixture source admission; close cache before normal
connection retirement and observe service PID disappearance before fixture cleanup.
Original transfer errors retain status/domain/code/errno/stage/cause. Development CDHash
pins and owned ephemeral bundles remain test infrastructure, not production trust.

## Verification and remaining work

Missing preview RPC diagnostic API and missing pure admission APIs produced red compile
failures before implementation. A sandbox module-cache denial on the first pure-test
command was setup evidence only; the correctly configured retry recorded the API red.
Focused strict Swift builds and pure/healthy XPC tests passed, followed by fresh full build
and228/228 CTest (35.38s). Actual SwiftPM runtime cached build0.17s and freshly linked
strict Swift6 consumer pass. Strict formatter, diff check and13 frozen source/fixture hashes
pass. No source was edited while a validation process ran.

Pure admission cases cover invalid identity, current-source replacement, replay, retirement,
missing EOF, peer/generation mismatch and every receipt guard with synthetic values only.
Measured gate coverage:17/17 regions,7/7 functions,29/29 lines; Swift reports no branch
counters. Shared RPC mapping file:28/29 regions,2/2 functions,52/52 lines. Do not interpret
these scoped results as full native/service coverage. Error classification tests preserve
invalid UTF8 diagnostic bytes, each C boundary class and bounded Swift lifecycle causes.
No new C++ parser/owner implementation changed and no new sanitizer claim is made.

Service scope/channel rejection, transport interruption/failure, actual half-close failure
and scheduler rejection paths are not newly forced in native processes. No denied-peer,
malformed-native-input, crash/fault/allocation-failure/signal/broken-pipe/native-cancellation
or pressure/exhaustion probes were run. No protected app, OS container or global SVG changed.

Production metadata-only active/waiting scheduling (including replacement and publication
retirement), public Swift SDK/worker packaging and trust distribution, app backend adoption,
full native-range resource acceptance and baked/imported profiles remain open. The fixture
uses an immutable source and synchronous serial jobs; it does not prove application source
mutation or waiting-job behavior. Apple pin48b0ce2d and active app routes remain unchanged;
all quarantines and M4–M8/full parity limitations remain in force.
