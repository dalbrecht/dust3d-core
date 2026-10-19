# Native source-aware baking verification

Scope: core #58 and ADR 0024. This is an opt-in C owner; Apple dependency/SDK/render
adoption, safe atlas minification (#59) and textured export/re-import remain pending.

## Local gates and measured coverage

The initial contract failed on the missing source-bake C types/functions before
implementation. The final CMake build and all **46 CTest contracts** pass in Debug
and separately under ASan/UBSan. macOS uses `ASAN_OPTIONS=detect_leaks=0` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`; Linux CI retains leak detection.
The suite includes a separately compiled pure-C consumer of the new public API.

SwiftPM builds the static product. A standalone client imports Dust3DEngine,
initializes both sized option structures, invokes the C load/null-accessor/destroy
boundary, and passes with Swift 6, complete strict concurrency and warnings as errors.
This smoke client verifies import/link/runtime interoperability; it does not replace
the upcoming Apple SDK copying tests. Manifest lint, root-config C++ formatting,
whitespace and Qt-boundary scans pass. Inherited compiler warnings and the existing
SwiftPM native-build-system deprecation remain; no whole-core warning-free claim.

Clang instrumentation of `native/Engine/SourceBakedMesh.cc`, running the actual API,
composition and pure-C contracts in one process, measures **411/431 lines (95.36%)**,
26/26 functions, 369/401 regions and 261/318 branches. Test entrypoints are renamed
only in the temporary coverage driver; product/test behavior and fixtures are unchanged.
A preliminary merge of separate executable profiles produced harness hash warnings;
the reported result is the single-process rerun without those warnings.

Residual coverage includes internal consistency/finite-frame guards, unsupported
sampler defenses behind validated GLB parsing, some quota-exhaustion combinations,
warning whitespace and fallback exception handlers. This is not 100% changed-file
line/branch or inherited-engine coverage. Broader instrumentation/fuzzing remains
core #1 / M8 work. Exact-head automated review and all hosted checks remain merge gates.

## Behavior established

- Real synthetic MIT ImportedSurface.ds3 produces four float-retained triangle
  records, coherent geometry, packed UVs, checked tangent frames, exact model/
  component catalogs and actual material alpha/cutoff/sidedness policies.
- Authored metallic .125, roughness .625 and linear emissive (.1,.2,.3) survive native
  resolution. Accepted glTF wrap/filter values and absent filter defaults execute.
  Textureless models require no decoded image. Removing source COLOR_0 independently
  verifies encoded component #8fc5dc plus material/gray-texture factors gives RGB
  (32,69,96) and straight alpha 64, distinct from linear vertex-color handling.
- Caller image/identity/inspection storage can be released; prior outputs survive
  subsequent failed loads. Invalid view selectors and null handles are safe.
- A mirrored document yields eight aligned triangles and original/generated
  membership. A separate tiny component collapsing only at float export leaves
  exactly the four valid triangles, with no stale UV/material/tangent records.
- A mixed procedural/imported document exercises both paths. Procedural-only Capsule
  composition preserves the existing encoded color pixels. A direct generation-
  observer contract verifies red/blue patches, a seam gradient, a broken-chart cap
  with unknown tangent, actual fold normals and metallic/roughness/emissive maps.
- Exact aggregate quota succeeds; one byte less fails. Malformed small RGBA spans,
  non-NUL identity lengths, duplicate/unresolved models, option/image/work/chart
  limits and pre-cancellation return typed causes without a partial owner.
- Deterministic allocation failures at initial owner and color-map allocation stay
  inside C, retain `std::bad_alloc::what()` and do not poison a subsequent bake.
- A regression first proved that a decoded component image override on an import
  could disappear silently. The adapter now explicitly rejects that unsupported
  combination instead of declaring a successful complete bake.

Fixtures are read only. The native image boundary intentionally uses synthetic
decoded pixels; PNG/JPEG decoding remains the caller's responsibility, not a claim
of decoder integration in this C++ stage. No new dependency or license change occurs.

## Reproduction and limits

Use README CMake configure/build/CTest commands; repeat with
`-DDUST3D_ENABLE_SANITIZERS=ON` and the supported platform environment above. Run
`swift build --build-system native -j 4`, manifest lint and root-config clang-format.
For scoped coverage, instrument the new adapter translation unit with
`-fprofile-instr-generate -fcoverage-mapping` and run the API, composition and pure-C
contracts under a single driver with distinct entrypoint names; merge its profile
with llvm-profdata and report this translation unit using llvm-cov. CMake/CTest
continues to run the contracts as separate normal executables.

Output quotas include retained roots, geometry/source/catalog/triangle/map capacities,
not decoder/generator/raster workspace or allocator overhead. They are not RSS/time
limits. Cooperative cancellation does not imply a hard latency bound. Unknown
procedural mapping/policy remains explicit. Consumers must honor maximum sampling
LOD zero until #59 establishes safe chart-isolated atlas minification. Primary
ordinary/source load and existing UV/bake contracts remain unchanged; optional bake
failure must preserve the primary document/geometry and original diagnostic in the
future Apple integration. Full Qt/M4/editor parity is not claimed.
