# ADR 0121: Shared C worker request encoding

Status: prepared host integration for #172; no production adoption.

## Decision

Expose an immutable C request owner in the separate worker runtime product.
Callers supply an explicit copied document/request/revision identity, base/UV
operation, stable opaque archive span, supported wire limits and optional resolved
UV/image options. Validate nonempty readable input, operation, identity and body
wire budget before retaining body bytes. Base requests reject UV options. UV
requests retain native zero/default options, original UUID spellings and semantic
duplicate-alias rejection through the maintained codec. No archive parsing,
native generation, transport, authentication or logging occurs in this API.

Introduce `UvRequestView`: a stable borrowed archive and reference to bounded
image values. Both existing owned encoding and C request encoding use the same
validation and explicit-endian serialization. Wire/logical/count/options/UUID
checks precede encoded body allocation, without constructing an intermediate
owned archive. The C bridge may construct bounded image metadata workspace
(maximum 10000 descriptors with 36/38-byte UUID text) before final codec validation.
The full frame prepends the shared encoded header. Logical quotas exclude
capacity, temporary workspace, allocator overhead and RSS; no hard memory or
containment guarantee is implied.

C result receipts own original category/code/diagnostic bytes, including explicit
truncation. C++ exceptions remain caught. NULL receipt does not allocate an owner;
NULL-safe destroy releases exactly once. Full frame and exact expected header
are borrowed only until owner destruction, and Swift copies both first. Existing
C layouts/version-one fields remain unchanged; additive unreleased API is not
binary stability. Caller pointer/span validity remains an explicit C precondition.

The isolated Swift XPC fixture now calls this API instead of writing request wire
fields itself. Base mode uses request 42; UV mode uses request 43, texture 64 and a
1 MiB metadata policy. The shared collector still correlates document, request,
revision and operation against the encoded expected header. Separate C semantic
owners expose the base mesh or atomic geometry/UV pair. The UV fixture copies
all 80 triangle records and checks finite corners, known/association tags and
component references. Native-generation calls remain solely in the service.

Both development XPC tests use one CTest resource lock, preventing overlap while
reusing fixed test bundle identifiers. The existing fresh ad-hoc signing, private
controller peer hashes, observed peer PID, regular-file descriptors and owned
cleanup remain. No ProcessOutcome is fabricated and no editor publication occurs.
Development peer binding is not production trust distribution or sandboxing.

## Verification

The C11 fixture initially failed on missing request types/functions. It now
independently checks explicit header fields and opaque bytes. Pure C++ fixtures
verify base/UV copied lifetime after input/owner destruction, options/dimensions,
original mixed-case braced UUIDs, NULL/default options, typed boundary/quota errors,
semantic duplicate aliases and NULL getter contracts. Borrowed and owned UV
encoding agree; existing independent golden prefix and codec rejection contracts
remain intact. No malformed native archive or resource-exhaustion job is used.

Scoped ASan/UBSan pass for the final C bridge/borrowed encoder fixture and the
existing UV request codec fixture. Combined coverage has 39 matching shared
function profile hashes, including both fixture mains' equal control-flow hashes;
only shared runtime files are reported. UV codec coverage is 174/174 lines,
136/136 regions and 112/112 branch outcomes. C request bridge coverage is 83/96 lines,
80/86 regions and 48/52 branches: oversized-span/image-count and defensive runtime/
diagnostic guards remain unforced. No whole-engine coverage or forced failure
claim follows. Strict C11/C++ warnings, C++/Swift formatting and diff checks pass.

Fresh quarantine guard and registered-source audit include 133 ordinary tests,
with both XPC cases sharing the verified resource lock. A clean CMake build passes.
The focused base/UV XPC pair passes in 0.75 seconds: base 240 vertices (host 45856/service 45858),
UV 240 vertices/80 triangles (host 45867/service 45868). Actual XPC peer PIDs match the
receipts; owned bundles are removed after host exit and service PID disappearance.
The full 133 contracts pass in 22.28 seconds, including strict Swift6 and both XPC modes.
Service exit status is not independently observed through launchd.

SwiftPM runtime/helper builds pass in 1.51/1.36 seconds with disjoint explicit source sets
(fourteen runtime, five CLI). The runtime exports the C request owner, and current
helper ordinary base/resource and UV compatibility pass. The existing native-
build-system deprecation warning remains. All gated implementation/test/build
hashes match after documentation-only updates.

Actual live pipes, bounded service scheduling, cancellation/deadlines, failure
receipt diagnostics, authenticated transaction/publication policy, sandbox and
resource constraints, release trust and reviewed Apple adoption remain open.
All pins/quarantines remain unchanged; full M4/M8/editor parity is not achieved.


## Serial delivery verification

Replayed after core #199 with accepted aggregate text admission, checked pure
metadata fixture access and the GCC-portable quota fixture preserved. Current
request/borrowed-codec/C/C++/Swift/controller sources match the inspected saved
stage. Historical runtime/Swift README hash differences are saved-stage
documentation only. The current audited inventory contains 134 ordinary
contracts, adding the accepted pure text-quota test. Both XPC tests have the
same verified CTest resource lock while reusing the fixed QA identifiers.

Fresh clean CMake build, nineteen pure preflight checks and freshly compiled
pure request-owner/borrowed-encoder plus existing UV-codec ASan/UBSan pass.
No native generation or process work occurs in those sanitizer fixtures. The
previous coverage figures remain historical prepared-stage measurements;
defensive runtime/oversized metadata guards remain unforced and whole-engine
coverage is unclaimed. Fresh healthy packaged-helper UV dispatch/visual checks
pass. SwiftPM runtime/helper builds take 1.46/1.34 seconds with disjoint fourteen
runtime/five startup sources; export compatibility passes.

The focused healthy XPC pair passes in 0.86 seconds: UV host94203/service94204
copies 240 vertices and 80 triangles, and base host94212/service94213 copies
240 vertices. Actual connection peer PIDs match. Host exit zero and service PID
disappearance precede owned temporary-bundle cleanup; service exit status is
not independently observed. All 134 contracts pass in 20.90 seconds, including
the XPC pair again. Source/binary hashes, strict formatting, Python syntax and
whitespace pass. Existing native build-system deprecation and duplicate resource-
library warnings remain. No failed-peer, native cancellation, malformed native
input, crash, signal, allocation-failure or pressure pilot is introduced.
Production pin, release/sandbox/resource gates and all quarantines remain unchanged.
