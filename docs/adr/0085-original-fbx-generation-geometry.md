# ADR 0085: Retain original generated geometry for static FBX

Status: Delivery validated locally; review and full M4/M7 acceptance pending (#99).

The preview pipeline destroys the generator Object after converting corners to
float render vertices. Export must not reconstruct double positions or shared
vertex identities from that buffer. Add an optional named GenerationOutputs hook
which creates the bounded geometry-selection owner from the original Object at
the end of successful generation. Default callers do not request or allocate it.

Copy all original faces in order, including any slivers omitted only by float
preview collapse. Retain positions, topology and available corner normals/UVs;
other Object metadata is excluded as in ADR0084. Returned source-face indices
refer to the generator Object, not preview indices. Original geometry selection
quotas apply independently of the preview. The additional temporary original-face
index list, existing source/generator/preview memory and allocator bookkeeping
are outside the geometry-owner quota; this is not a process-memory bound.

Preflight minimum logical geometry/quota before invoking the generator, then
admit actual counts before allocating source indices. Construct a temporary owner
and perform the existing cancellation check before replacing the caller output.
A rejected call preserves the previous geometry owner. The caller retains both
the borrowed output pointer and engine serialization lock throughout the call.
No new C API, native app adoption, image decoding, rig/animation or GLB path is
enabled. Production dependency pins and all quarantines remain unchanged.

## Initial local verification and remaining acceptance

The missing output fields were recorded red. The new ordinary contract uses
only the licensed Capsule fixture under engineMutex. It verifies80 source faces,
240 preview vertices, complete source-face order, actual values not representable
as float, independent ownership across preview destruction/another generation,
static encoding, quota rejection preserving the prior owner and successful
replacement. No fault injection, signals or exhausted-resource probes run.

Fresh Debug build and75/75 CTest contracts pass (16.47s). Source quarantine and
formatting guards pass. A fresh strict-warning scoped ASan/UBSan build instruments
the changed bridge, selection owner and test driver, linking fresh ordinary core
libraries; it passes. Inherited geometry is not instrumented by that scoped run.
SwiftPM builds (3.39s) with inherited UUID narrowing warnings.

A separate instrumented bridge/uninstrumented driver run executes every added
executable hook line. This single-fixture bridge measurement is112/182 lines,
78/156 regions and48/108 branch outcomes; it does not cover all inherited error,
asset or bake paths or establish whole-core coverage. ADR0084 retains the
separate selection owner's measured coverage and independent import evidence.
No new independent consumer claim is made for the generated Capsule scene here.

Static source-material bake/group alignment, owned C ABI, output publication,
asynchronous cancellation, native UI/CLI commands and independent generated-scene
acceptance remain required before full export parity. The geometry hook is an
integration prerequisite, not an exposed exporter or lossless document clone.

## Delivery revalidation

Prepared on tested face-selection delivery `20fe0117`, retaining reviewed
worker/writer contracts and the tested FBX prerequisites. Only the source-audited
ordinary Capsule geometry-retention contract is newly registered. Fresh ordinary
CMake build and 82/82 CTest contracts passed (17.38 seconds). Because this changes
the common generation pipeline, a fresh full ASan/UBSan rebuild and all 81
sanitizer-mode CTest contracts also passed (54.10 seconds). The additional ordinary
Swift consumer is deliberately excluded in sanitizer mode. Engine, core, remesher
and triangle-library sanitizer flags are retained with local provenance. Leak
sanitizer options were not enabled; no leak or full-path coverage claim is made.

SwiftPM built successfully (3.82 seconds), with manifest lint, formatting, source
quarantine and Qt-boundary checks passing. Earlier 75-case, scoped instrumentation
and bridge-coverage measurements above are historical and were not remeasured.
All tests use source-audited ordinary fixtures or pure bounded validation; no
allocation failure, signal, exhaustion, quarantined fixture or crash probe ran.
This hook still follows preview preparation: the subsequent original-export
preparation stage is required before a general exporter can avoid float-preview
admission entirely. No C ABI, native command, production pin or quarantine change
is introduced, and full M4/M7 acceptance remains unverified.
