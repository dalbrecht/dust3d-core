# Indexed mesh verification

Core issue #17, ADR 0004. The first build failed because the indexed owner/API and
internal builder did not exist; tests were written before implementation.

Seven portable scenarios extend the existing CTest matrix:

- Null source/owners: descriptive rejection, optional diagnostic output, safe null
  destruction, null borrowed pointers and zero counts.
- Reconstruction: a real independently serialized `.ds3` planar-loop mesh is
  converted; every index is in bounds and reconstructs every expanded position,
  normal and color value. Triangle count, source counts and warning survive.
- Ownership: borrowed indexed vertices/indices/warning remain valid after source
  destruction and unrelated generation/conversion/destruction.
- Budget: exact payload succeeds, one byte below vertex payload and insufficient
  index payload fail with LIMIT_EXCEEDED, omitted errors remain safe, and source
  data remains usable after failures.
- Attribute seams: a pure builder fixture preserves distinct normal/color values
  at shared positions, reuses identical complete vertices, and accepts equal
  signed-zero coordinate values without losing numeric attributes.
- Invalid attributes: pure builder inputs with NaN/Inf position, normal or color
  reject before ordered-key lookup; empty/incomplete triangles reject atomically.
- Concurrent reads: four workers perform fifty immutable conversions each while
  unrelated generation occurs. This is representative stress, not a TSan proof.

The C consumer compiles and calls the new API without C++ or Qt headers. Existing
expanded geometry and error/empty/precision contracts continue to run unchanged.

## Fixture provenance

The planar-loop fixture is an independent MIT synthetic variant of
[the core empty-result fixture at immutable commit
2776de6933ae5f511e632afee6ffbd0c33dc0b75](https://github.com/dalbrecht/dust3d-core/blob/2776de6933ae5f511e632afee6ffbd0c33dc0b75/tests/core/empty_result_tests.cc).
It adds a fixed color and a procedural-pattern warning attribute to verify
metadata copying; no texture is rendered or external asset loaded. Numeric
reconstruction is compared against the actual same-input expanded mesh, rather
than a golden emission order. Repeated conversion of that immutable input must
retain stable indices.

The attribute-seam fixture is newly authored MIT test data: two perpendicular red
triangles, a coincident blue triangle and a repeated red triangle. Internal builder
unit tests deliberately operate on these complete buffer values separately from
the real-document public API integration tests. No user documents are involved.

## Validation scope

All twenty-five Debug and twenty-five ASan/UBSan contracts pass locally on
2026-10-17, as does the SwiftPM source build. macOS local leak detection is
unsupported; existing Linux CI retains it. Native/test formatting, manifest lint
and Qt-boundary checks also pass.

This does not establish inherited C++ coverage, global race freedom, renderer/GPU
acceptance, per-vertex source identity or general topology parity. Allocation-failure
stress and the defensive UInt32 limit remain thin-boundary limits; the current
loader already bounds valid expanded input below that index limit. Do not invent
unreachable data just to claim coverage. Payload limits exclude the expanded
input, temporary lookup/allocator storage and consumer copies; large-model timing
and peak memory require later measurement before renderer defaults change.
