# Ordinal source UV atlas verification

Scope: core #54 and ADR 0022. This stage packs source-aware UV metadata; it does
not enable native imported baking or change an Apple dependency pin.

## Automated evidence

The contract initially failed on the missing new header. After implementation,
all 42 headless CTest contracts passed in Debug and separately under ASan/UBSan.
The supported local macOS sanitizer invocation uses `ASAN_OPTIONS=detect_leaks=0`
and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`; Linux hosted CI retains
leak detection. Hosted checks and exact-head automated review remain merge gates.

SwiftPM built the static product, manifest lint passed, all native/test C++ files
and the new domain files passed root-config clang-format, and diff whitespace and
Qt-boundary scans passed. Existing inherited compiler warnings and SwiftPM's native
build-system deprecation remain; no whole-repository warning-free claim is made.

Clang instrumentation of `source_uv_atlas.cc` reports 230/230 executable lines,
12/12 functions, 239/239 regions and 189/214 branches. These measurements apply only
to the new implementation. Defensive combinations inside compound checks are not
all exercised; this is not 100% branch or inherited-engine coverage.

## Behavior exercised

- Coincident imported triangles preserve separate charts, byte-exact model/component
  identities and independent owner lifetimes after caller metadata changes.
- Authored negative/repeated UVs remain unchanged. Textureless imports keep unknown
  authored mapping while gaining known output UVs. Output corner transforms are
  checked independently, including chart transposition.
- Packed import charts and the legacy region do not overlap. All packed coordinates
  are finite and normalized. Legacy-only UVs and texture size remain unchanged;
  unmapped procedural triangles and empty geometry remain explicitly unknown/empty.
- Exact retained quota succeeds; one byte less fails. Chart-count limits, cancellation,
  malformed parallel arrays, invalid indices, degeneracy, nonfinite values, invalid
  legacy UVs/options and extreme geometry/packing ranges produce typed failures.
- The existing synthetic MIT ImportedSurface.ds3 fixture is decoded by readDocument
  and generated with MeshGenerator. All four real generated triangles retain exact
  source identities/corner UVs and separate non-overlapping packed charts. No fixture
  or archive bytes are rewritten.

## Reproduction and limits

Run README CMake configure/build/CTest commands and repeat with
`-DDUST3D_ENABLE_SANITIZERS=ON`; use the supported sanitizer environment above on
macOS. Run `swift build --build-system native -j 4`, manifest lint and root-config
clang-format on the new source/header/tests plus existing native/test files.

Retained payload excludes inputs, allocator bookkeeping and temporary packing
workspace. Packing attempts are bounded, but an inherited heuristic is not
interruptible internally and no cancellation latency/RSS limit is claimed.
Per-triangle chart density and large-model packing performance require further
measurement. Native adoption still must preserve final float-filtered triangle
alignment, rasterize authored UVs through the CPU sampler, compose procedural maps
through the returned region, preserve material/alpha/scalar semantics and validate
textured exports by independent re-import. Core #41 and Apple #143 remain open.
