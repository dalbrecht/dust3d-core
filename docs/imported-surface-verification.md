# Imported surface source verification

Tracked by #50. The public C contract is exercised by
`tests/core/imported_surface_tests.cc`, using the MIT synthetic
`tests/core/fixtures/ImportedSurface.ds3` and the existing Capsule fixture.
Fixture construction/provenance is recorded in the fixture README.

## Behavioral evidence

- Real tetrahedron source UV/RGBA corners align with exported geometry; authored
  alpha is not multiplied by material alpha or replaced by preview opacity.
- Explicit linear encoding for glTF COLOR_0; explicit sRGB encoding and exact
  midtone RGB for document fallback when COLOR_0 is absent.
- UV repeat coordinates (-2, 3) survive unchanged at three corresponding corners.
  A NaN UV rejects with the original typed decoder diagnostic.
- Mirrored original/generated component catalogs preserve reflected positions,
  corner winding, model identities, source UVs and colors.
- A preceding double-valid triangle group collapses after float conversion;
  surviving records still refer to the correct later triangles, and discarded
  capacity remains charged to retained ownership.
- Procedural geometry reports no imported model/UV rather than inventing facts.
- Input overwrite, subsequent successful loads and subsequent rejected loads do
  not invalidate an existing owner's views.
- Exact payload budget fits; one byte less rejects. Invalid options, oversized
  budget, invalid input, null queries and out-of-range catalogs are checked.
- Fault injection into the last 32 real load allocations verifies RAII cleanup,
  typed allocation errors and preserved causes. A separate C consumer compiles
  and links the public declarations and verifies null/error behavior.

## Limits and remaining acceptance

These contracts do not prove full imported material rendering or arbitrary
Boolean provenance. Per-corner transfer through clipping and cache reuse has
additional internal coverage documented in imported-corner-uv-verification.md;
end-to-end material sampling, mixed Boolean surface acceptance, packed atlas
integration, baking and Apple adoption remain subsequent work. Source output is
not a substitute for packed UV metadata. No pixel baseline is generated locally.

The final Debug suite passed 40/40 contracts in 26.03 seconds. SwiftPM builds
with the new source boundary and encoding tag. The final macOS ASan/UBSan suite also passed 40/40 contracts. macOS runs disable leak detection; Linux CI must
supply the supported ASan/LSan gate.

Scoped LLVM instrumentation of ImportedSurface.cc with the public surface
contract measured 142/152 lines (93.42%), 85/98 branches (86.73%) and 18/18
functions executed. Uncovered lines are defensive generated-channel/model/UV/RGBA
invariant failures, final retained-accounting overflow and generic exception
fallbacks; malformed archive UVs reject earlier in the decoder. These remain
verification gaps, not a claim that all boundary paths or inherited core logic
have 100% coverage. Fault injection covers allocation errors with real loads.
