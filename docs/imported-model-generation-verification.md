# Embedded-model geometry verification

Tracked by core #41. The committed ownership/header correction passed its gates.
Further review corrections are in progress below; the working tree is not merge-ready.
This stage does not establish full M4 or application import support.

The first end-to-end C API test failed with the existing imported-GLB rejection.
The corrected reader binds a self-contained, closed tetrahedron from an actual
`.ds3` archive to `ImportedModel` generation. Four output triangles remain finite,
and changing `deformWidth` from 1 to 2 doubles the generated X extent.

The contract exercises:

- Required missing/malformed assets, original GLB diagnostics, invalid UUID syntax,
  brace aliases, case preservation and ambiguous alias rejection.
- Opaque unreferenced assets and known-role retained metadata. A failing regression
  reproduced an unknown imported role falling back to procedural geometry; it now
  returns an explicit unsupported-target diagnostic.
- Unchanged source archive bytes, independent mesh/indexed/sourced owners, source
  corner alignment, input overwrite and later load/session destruction lifetime.
- Unapplied material warnings, coexisting image warnings, and explicit UV/bake
  rejection. The encoded image marker is not a pixel-decoding test.
- Disabled, tiny and retained cache budgets; actual component reuse; changed GLB
  bytes under the same UUID matching fresh generation; required-error recovery.
- A real multi-model archive exceeding the 64 MiB decoded-owner quota before
  generation. No production test/failure switches or reduced quota are used.

Earlier committed-head results (`df70d4a`):

- Full Debug build and **37/37 CTest entries passed**, 23.46 seconds.
- The ASan/UBSan build passes the direct spine and expanded imported-model
  contracts, 1.99 seconds, with UBSan halt-on-error enabled. Other local instrumented
  contracts were not rerun; hosted sanitizer CI still runs the full suite.
- SwiftPM source product builds; strict Swift/C++ formatting and Qt boundary scan pass.
- Resolver-only LLVM coverage: **83/83 lines and 5/5 functions (100%)**,
  **71/82 branches (86.59%)**. This is not whole-reader/engine or 100% domain coverage.
- Shared asset references generate both distinct parts; encoded-image accounting and
  coexisting compatibility warnings are exercised without claiming pixel decoding.

Evidence logs are retained under `/private/tmp/dust3d-imported-model-*`.
The independent direct-generator bent-spine probe and SHA256 manifest are in
`/private/tmp/dust3d-imported-spine-probe`, recorded in issue #41. That probe
bypasses native archive loading and is not end-to-end app acceptance.

Reproduce with the README's CMake/CTest gate. For the relevant instrumented path,
configure a separate build with `DUST3D_ENABLE_SANITIZERS=ON`, build
`dust3d_imported_model_tests`, and run `ctest -R contract_imported_models` with
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. Hosted Linux sanitizer CI also
runs the complete suite with leak detection; no local Linux/32-bit result is claimed.

Next acceptance includes imported materials/pixel decoding, aggregate transferred
asset ownership, C/Swift integration and exact Apple pin, native open/edit/undo/save,
Quick Look/thumbnail rendering, export re-import and broader upstream comparisons.


## Review correction: independent zero-width scaling

Automated finding 4308286986 identified `0/0` in inherited spine deformation when
`deformWidth` is zero. Both a direct portable deformer test and an actual imported
`.ds3` load failed before the fix; the latter reported nonfinite generated geometry.
Width and thickness now multiply the radius/source scale independently. This small
core fix preserves the inherited layout and can be extracted for upstream review.

The direct contract covers zero/positive/negative width, zero/positive thickness,
cut rotation, zero radius and a bent spine. The C API regression requires finite
flattened faces with remaining thickness. The full corrected Debug build passes all 37 contracts (24.54 seconds), relevant
ASan/UBSan contracts pass (2.28 seconds), and SwiftPM/formatting/Qt boundary checks
pass. Logs are retained under `/private/tmp/dust3d-core-pr43-zero-width-*`.
The resolver-only coverage measurement above is unchanged source from the initial
head; no whole-engine coverage claim is made. Fresh exact-head review and all
hosted CI checks remain required before merge.


## Review correction: transformed imported normals

Finding 4308327427 identified frame-only normals under rotated/nonuniform vertex
deformation. Both direct expected-vector tests and a public C API perpendicularity
test failed before the fix. Normals now use the inverse transpose of the local
vertex-deformation Jacobian, including longitudinal and radius-gradient terms.
The generator supplies the full source point so tapered cross-sections retain
their shear contribution. The legacy source-Y overload evaluates the centre.

The direct contract covers rotation, inverse nonuniform scales, reflected scales,
thin scales, taper and finite-difference tangents on a bent spine. Singular
transforms return an unavailable normal; the generator retains geometry-derived
normals rather than overriding them with zero or nonfinite data. Actual GLB
fixtures with explicit and generated normals verify perpendicular output under
rotation/scaling; the zero-width archive verifies the singular fallback. Final
corrected Debug build passes all 37 contracts (23.46 seconds); both relevant
ASan/UBSan contracts pass (1.99 seconds). SwiftPM, formatting and Qt boundary
checks pass. Logs are under `/private/tmp/dust3d-core-pr43-normal-*`.
Fresh exact-head automated review and all hosted CI checks remain required.
No whole-engine coverage claim is made.


## Review correction: coincident attribute ownership

Finding 4308411184 was reproduced by
`testCoincidentImportedAttributeIsolation` in the public imported-model contract.
Two embedded tetrahedra occupy exactly the same space but belong to separate
parts: a combined red part with source +X normals and an uncombined green part
with source +Z normals. The expected 24 expanded corners retain twelve corners
from each part, including their independently transformed normals. The original
implementation failed with “Coincident parts overwrote each other's imported
attributes”. The corrected regression passes, including a second variant without
COLOR_0 that retains the owning blue component color. Cached loads and a nested
group preview preserve the same independent attributes.

The correction carries exact source triangle indices from Boolean cutting through
union/difference selection. Cached MeshState copies retain component ownership;
recombination maps retained faces by input index and gives new bridges no invented
owner. Ownership tracking is activated by imported documents; ordinary documents
do not retain per-face UUID buffers. The extra cached capacity and UUID strings
are included by the allocation-free payload visitor and its capacity regression.

Imported attributes remain in their owning part cache, rather than being merged
into global position maps. Final native output uses corner colors, so shared
Boolean vertices can retain different attributes on different faces. Group
previews expand corners when imports are present, and the imported smoothing
pass keys positions by owning component. Unidentified generated bridge faces keep
computed fallback attributes. This does not establish interpolated GLB materials
or complete attribute transfer onto new Boolean/seam geometry.

The portable Boolean regression verifies source triangle containment and winding
for union, difference and intersection; MeshCombiner forwarding; recombination
retained-face identity; and cached MeshState copies with/without recombination.
Each new source-identity API first failed to compile in its regression before its
implementation was added. No preexisting position-derived vertex source hint is
used as authoritative triangle ownership.

Final complete Debug build and CTest pass **37/37** (129.82 seconds), and the
complete linked ASan/UBSan build and CTest pass **37/37** (263.00 seconds), with
UBSan halt-on-error enabled. No local leak-detector or Linux result is claimed.
SwiftPM builds (69.81 seconds); strict formatting, changed inherited source ranges,
Qt boundary scan and whitespace checks pass. Parallel compilation occurred during
the Debug test run; these timings are validation evidence, not performance parity.
The long sanitizer session contract was sampled while live and was computing in
the wrapping remesher, then passed within its existing 180-second timeout (167.71
seconds). Logs are under `/private/tmp/dust3d-core-pr43-scope-*`.
Resolver-only coverage above is unchanged; no whole-engine coverage claim is made.
Fresh exact-head review and all hosted checks are still required before merge.


## Portability correction: explicit array definitions

The first hosted Linux build of the ownership correction failed compiling the
expanded Boolean contract: MeshState/MeshRecombiner used std::array through
transitive includes that supplied a complete definition on libc++ but not
libstdc++. Both owning headers now include `<array>` directly. The same Boolean
contract continues to instantiate those types without an earlier test-file array
include, so the Linux matrix exercises the original failure path.

The corrected full Debug build and CTest pass **37/37** (71.27 seconds); the full
ASan/UBSan build and CTest pass **37/37** (190.63 seconds). SwiftPM builds (55.96
seconds), strict formatting, Qt boundary and whitespace checks pass. These are
local macOS results; fresh Linux/macOS hosted checks and exact-head review remain
required. Logs are under `/private/tmp/dust3d-core-pr43-array-*`.


## Review corrections: source hints, color seams and node scales

Finding 4308646690 is reproduced through the sourced C API: known corner hints
from the second coincident imported part resolve to the first part. The regression
failed with “Coincident imported source hint refers to another part”. Imported
component-specific position-to-node associations now remain on the generated
Object, and each exported triangle index is supplied to the source resolver. A
known imported owner never falls back to another component's global node/weight
map. Generated nearest-node identities absent from original membership remain
explicitly unknown; no new original identity is invented. Fresh and cached sourced
loads, coincident global weights and missing scoped associations are covered.

Finding 4308646686 is reproduced by a valid nonindexed tetrahedron with four face
colors and duplicated seam positions. The public API failed with “Imported color
seam collapsed distinct coincident corners”. Imported colors are now indexed by
source vertex at leaf generation and carried as optional triangle-corner arrays
through MeshState/cache copies, Boolean source triangles and recombination. Cut
triangles interpolate colors from their exact originating face using bounded,
scaled barycentric weights; new seam bridges have no invented source colors.
Native output and leaf/group previews retain all four colors. Independent clipped
box gradients exercise union/difference, recombination and cached copies; corner
buffer capacity is included in allocation-free cache accounting.

Finding 4308676923 identified ignored per-node directional scales. A standalone
portable regression failed its expected vertex vector, and the actual .ds3/C API
regression failed with “Imported node width modifiers were ignored”. Width and
thickness now interpolate independently along each segment and multiply the
part/radius scales. Normal Jacobians include both node-scale derivatives and the
radius-gradient product terms. Expected endpoint/midpoint vectors, independent
normal vectors, combined radius/scale gradients, bent/rotated finite differences
and actual archive extents/normals cover the correction.

An additional accepted-identity regression used distinct braced/unbraced component
keys that collapse to one inherited Uuid. It failed with “Another part overwrote
the red import normal”. MeshState now retains original opaque component keys,
including cached/recombined output. Normal/color fallback, imported smoothing and
source associations use those keys; legacy Uuid metadata is derived separately.
Public coincident-geometry and portable clipped-gradient regressions exercise
these aliases without rejecting the document or choosing an arbitrary owner.

The final complete Debug build and CTest pass **37/37** (62.11 seconds); the
complete ASan/UBSan build and CTest pass **37/37** (169.30 seconds), with UBSan
halt-on-error. SwiftPM builds (21.79 seconds). Strict formatting, inherited changed
ranges, Qt boundary and whitespace checks pass. Logs are under
`/private/tmp/dust3d-core-pr43-final-opaque-*`. No local leak-detector/Linux result
or performance-parity claim is made. The resolver-only earlier coverage numbers
are not whole-engine or freshly instrumented coverage for these changes.
Fresh exact-head hosted review and all CI checks remain required before merge.
Imported materials, new seam material interpolation and full M4 remain open.

## Review corrections: quantization bounds and shared-part fallback colors

Finding 4308854378 exposed an unchecked floating-point-to-long conversion after
accepted radius/part/node scales multiplied into coordinates near 10^18. Before
the fix, a direct quantization regression accepted unsafe inputs and an actual
imported `.ds3` did not return the expected rejection. PositionKey now checks the
scaled value against exact, exclusive signed-long bounds before conversion and
throws PositionQuantizationError. This generic fix is kept in its own commit for
upstream reuse. Tests cover each coordinate axis, NaN/infinities, finite overflow,
near-boundary ordering and unchanged truncation; the archive contract covers
expanded/sourced errors and a session recovering after rejection. The check adapts
to the platform's long width; a local 32-bit build has not been performed.

Finding 4308854381 reproduced two components referencing the same uncolored GLB
part inheriting the last component's color. Imported attribute owners now retain
the component's explicit/default fallback color separately from the shared part
cache. Regressions cover red/blue and default/blue instances, a later color edit,
fresh/session loads, and leaf/group previews. Explicit source vertex colors retain
their existing precedence. The regression failed with the original implementation
before this correction.

The full final Debug build and CTest pass **38/38** (28.10 seconds), and the
complete ASan/UBSan build and CTest pass **38/38** (74.06 seconds), with UBSan
halt-on-error enabled. SwiftPM builds (12.50 seconds); strict native/test formatting,
changed inherited ranges, manifest formatting, Qt boundary and whitespace checks
pass. These are local macOS results; no local Linux/leak/32-bit result or full
inherited coverage claim is made. Logs are under
`/private/tmp/dust3d-core-pr43-bounds-*`. The branch also incorporates reviewed
Node 24 CI from master (#45). Fresh exact-head review and hosted CI remain
required before merge. This remains a geometry-preview stage; material transfer,
Apple pin/adoption and M4 acceptance are not complete.
