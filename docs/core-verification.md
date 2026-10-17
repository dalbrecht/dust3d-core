# Initial portable-core verification

Verified October 14, 2026 with Apple Clang 21 and CMake 4.4.4 on Apple Silicon.

- Full standalone CMake Debug build passed with no Qt, Swift or UI frameworks.
- All seven CTest contracts passed: null handles, invalid archive diagnostics,
  early input-size limit, Capsule and Sword geometry/colors/normals, retained
  mesh ownership across another load, and a pure-C header/link consumer.
- A separate full ASan/UBSan build passed the same seven contracts with
  `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`.
- New adapter/test C++ formatting passed the inherited `.clang-format` rules.
- Original geometry implementation and third-party license files were retained.
  Inherited `sprintf` and signed one-bit field warnings remain visible.

This does not establish full C++ line/branch coverage, all-feature geometry parity,
fuzzing coverage, ABI stability, leak-free execution on every platform, or hard
resource limits in geometry algorithms. Linux/macOS hosted CI is configured
separately; its results must be checked before release.

Follow-up: [core testing roadmap](https://github.com/dalbrecht/dust3d-core/issues/1).

## Boolean-boundary regression verification — October 16, 2026

Core issue #3 was reproduced in a new portable CTest before changing engine code:
the rounded/subdivided crossing returned generation failure with repeated-halfedge
and polygon-ring diagnostics. A degree-four intersection vertex belonged to two
loops; vertex-only traversal prevented the second loop from closing. A stronger
closed-edge check then exposed a sliver between original and cut coordinates
about 2.2e-8 apart across a truncated position-grid boundary.

The repair consumes boundary edges, splits repeated-vertex walks into closed loops,
shares original/cut vertex indices and checks neighboring weld cells at the existing
1e-5 resolution. Collapsed triangles/edges are removed after mapping. The global
PositionKey and C ABI remain unchanged. The original affected file has the same
Git blob in the maintained base and upstream-sync; this is a reusable inherited
engine fix, not an Apple-specific fork of the algorithm.

Final local checks passed with Apple Clang 21.0.0 and CMake 4.4.4:

- Complete Debug CMake build and all ten CTest contracts.
- Separate full ASan/UBSan build and all ten contracts with halt-on-error enabled.
- SwiftPM static source product build.
- Formatting for the modified inherited C++ files and all adapter/contract sources.
- Whitespace verification using `core.whitespace=cr-at-eol`, preserving inherited CRLF files.

Five synthetic MIT fixtures check complete finite colored meshes, normalized
normals, noncollapsed edges paired with opposite winding at 1e-5 tolerance, and
connected-surface counts. Union, translated union and hard union remain connected;
a centered difference and a 4e-5 separated pair retain two surfaces. Fixture
provenance/geometry is recorded in tests/core/fixtures/README.md; the design is in
[ADR 0001](adr/0001-boolean-intersection-loops.md).

macOS ASan ran with leak detection disabled; no leak-free claim is made. Hosted
Linux/macOS and Linux leak-enabled sanitizer results must pass before merge.
Qt GUI differential verification, complete vertex-manifold guarantees, broad
performance budgets and full inherited C++ coverage remain unproven.

Automated review identified a zero-bucket boundary case: truncated PositionKey
maps -9e-6 and +9e-6 to the same key despite their 1.8e-5 separation. A new direct
SolidMesh boolean test failed before the correction because triangles disappeared
from a valid thin box. Buckets now retain multiple distinct canonical indices and
every candidate, including same-key candidates, must pass the coordinate-distance
check. Three axis variants preserve all 24 triangles and signed union volume
0.100018 within 1e-12. This raises the suite to ten CTest entries.

## Legacy decimal-separator boundary regression

Apple cut-face PR #84 exposed that inherited String numeric parsing accepts comma
decimals while the native validator's raw std::stod/full-consumption check rejected
them before generation. A failing-first C ABI fixture reproduced rejection at
originX. Validation now normalizes both decimal separators to the active C locale
before the existing strict parse. Snapshot values, C ABI layout/ownership and
inherited algorithms are unchanged.

The synthetic in-memory contract compares dot/comma actual mesh counts, positions,
normals and colors across canvas, node, part and component numeric fields. It
also rejects trailing junk, duplicate separators, nonfinite values, overflow/
underflow, excessive magnitude and negative comma-decimal node radius. The final
Debug CMake/CTest and separate ASan/UBSan builds each pass all eleven entries;
SwiftPM and adapter/test formatting pass. Existing inherited UUID integer-conversion
warnings remain in SwiftPM; no warnings were suppressed. macOS sanitizer testing
uses detect_leaks=0, so Linux leak-enabled hosted checks remain required before
merge. Full inherited coverage and Qt GUI differential acceptance are not claimed.

## Complete wrapping numeric boundary (core #11)

The wrapping audit for Apple issue #98 found ten consumed numeric attributes
missing from native validation: `wrapSmoothness`, `wrapDrape`, `wrapDrapeLength`,
`wrapOpenTop`, `wrapOpenBottom`, `wrapThickness`, `wrapWeightRadius`,
`wrapPatternScale`, `wrapBelly` and `wrapWrinkleSize`. The existing checks for
`wrapOffset`, `wrapFaces` and `wrapWrinkles` remain unchanged.

A new rejection case first failed with "Invalid numeric fixture was accepted".
All ten fields now use the existing validator before generation. The numeric
contract checks malformed, nonfinite, overflow, underflow and excessive-magnitude
values for each key, and accepts empty/default, zero, dot/comma decimal, negative
finite and magnitude-boundary values. Attributes are attached to an inert
part-linked component so these cases isolate boundary parsing from wrapping
algorithms; they do not claim wrapping geometry acceptance or impose UI ranges.
Finite values retain the inherited generator's clamping and defaults.

All fifteen Debug and fifteen ASan/UBSan contracts pass locally on 2026-10-17,
along with SwiftPM source compilation and formatting. Local macOS sanitizers do
not enable unsupported leak detection; Linux CI retains its existing leak gate.
No API, dependency or inherited geometry algorithm changes.
