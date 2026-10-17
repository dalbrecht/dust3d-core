# Portable configured stitching contracts

Core issue #9 promotes the native editor's configured-surface evidence into
portable C ABI tests. Tests independently serialize synthetic XML and `.ds3`
containers, invoke dust3d_mesh_load, copy borrowed vertices before releasing the
owner, and validate source counts, nonempty triangles, finite positions/colors,
bounded colors and unit normals. No Swift, Qt or display server is involved.

The source is `dalbrecht/dust3d-swiftui` at immutable commit
`789b789626869503d64c2d14f87259b57db693b6` (Apple PR #97 merge):

- [StitchingFixture.swift](https://github.com/dalbrecht/dust3d-swiftui/blob/789b789626869503d64c2d14f87259b57db693b6/Tests/Dust3DEditorTests/Support/StitchingFixture.swift)
- [ComponentStitchingStoreTests.swift](https://github.com/dalbrecht/dust3d-swiftui/blob/789b789626869503d64c2d14f87259b57db693b6/Tests/Dust3DEditorTests/ComponentStitchingStoreTests.swift)
- [PartLoopInteriorStoreTests.swift](https://github.com/dalbrecht/dust3d-swiftui/blob/789b789626869503d64c2d14f87259b57db693b6/Tests/Dust3DEditorTests/PartLoopInteriorStoreTests.swift)

The synthetic fixture geometry is independently reimplemented as C++ builders in
`tests/core/stitching_tests.cc` under this repository's MIT license, without user
documents, external assets or generated binary fixtures. Intentional parity
updates must identify the replacement immutable source revision and record any
changes to the expected behaviors and tolerances below.

- Lines: three curved semicircular profiles at different depths. Front/back/side
  closure and segment count must change output; segment endpoints 0, 1 and 100
  must remain finite. Side closure joins each spline's own endpoints, so
  straight-profile finite output is now covered by the angle regression correction
  described in [bounded angles](angle-normal-verification.md); zero-area triangle
  quality remains separately tracked in core #14.
- Loops: concentric closed diamond loops. Back closure, depth and sharpness must
  change output, and supported scalar endpoints remain finite.
- Fill: isolated loop fill adds surface triangles beside a separate uncombined
  model. Connected-loop fill retains existing surface positions. Refilling must
  restore the same copied triangle geometry within 1e-6 coordinate tolerance. The prior
  lone-loop empty-result contract continues to cover NO_GEOMETRY without a model.

A controlled test-only fixture mutation changed the front-closure case back to
the baseline value. The contract failed with "Line setting did not affect
geometry". The source was restored and rebuilt before final verification; no
mutated fixture or binary is committed. This checks that metadata-only output
cannot satisfy the geometry-change assertion.

Local verification on 2026-10-17: all fifteen Debug and fifteen ASan/UBSan CTest
contracts pass, as do SwiftPM source build, new C++ formatting, manifest lint and
Qt-boundary checks. macOS local sanitizer runs use detect_leaks=0 because leak
detection is unsupported there; existing Linux CI retains leak detection.

The existing CI matrix automatically executes these registered CTest scenarios.
No geometry algorithm, dependency, public API or CI workflow changes. The Apple
production pin can remain at the same reviewed engine implementation. These
contracts do not prove full inherited coverage, manifoldness, agreed Qt
differential tolerances or native UI/GPU acceptance.

Review regression coverage canonicalizes triangles and their corners, then matches
positions within tolerance independent of emission order. Synthetic comparison
cases check reordered triangles/corners, below/above-tolerance movement, changed
connectivity with the same vertex multiset, and different counts. The new reorder
case failed against the old index-based comparison before the correction.
