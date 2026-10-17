# Portable CPU texture baker verification

Tracking: core #37. C++ implementation; not yet an Apple renderer feature.

## Contracts and ownership

`TextureBaker::bake` accepts const original geometry/UV metadata, explicit RGBA8
image views and resolved material/pattern/fold values. Returned maps and tangent
records own their storage. The synchronous call retains no source pointer.

The native contract executable exercises solid/image/seam/material outputs,
actual Spots color variation and nonflat fold normals, output lifetime across
later calls, exact/insufficient payload budgets, cancellation, malformed geometry,
UV alignment, image stride and procedural settings. Real Capsule, Sword and
RoundedCrossing fixtures exercise deterministic bakes and normalized, aligned
tangents. Unknown UV records remain unknown; baking does not repair missing UVs.

Failing-first regressions reproduced missing spatial pattern evaluation, accepted
invalid settings, continuous rather than discrete seam interpolation, unsafe
noise-lattice coordinates, and an actual UBSan signed-grid conversion in inherited
ClothFolds for finite coordinates near 1e12. Preflight bounds now reject that grid
before construction and check neighbour-query coordinates. A conservative
cumulative cell-insertion quota bounds inherited grid construction work separately
from output bytes. This is not a process-memory or time guarantee.

Default square map sizes are color 4096, normal 2048 and material 1024, with
maximum dimension 8192. Zero output budget selects 256 MiB; explicit budgets cannot
exceed it. The spatial insertion bound defaults to 1,000,000 and accepts 1 through
16,000,000. The output quota counts pixels/tangent records; allocator overhead,
input storage, chart preparation and raster/dilation workspace are excluded.

## Measured Qt comparison

The optional standalone reference harness adds `portable-` scenarios. Both paths
receive the same analytic geometry and resolved source metadata. Original corner
UVs must agree within 1e-10 before comparing matching-resolution image samples.
Each triangle has 21 interior barycentric probes. Each RGBA channel permits at
most two bytes of rounding difference; RGB beneath zero alpha is ignored because
straight-alpha and Qt storage policies differ there. Alpha is always compared.

Measured with Qt 6.12.0 on this Mac:

| Scenario/map | Probes | Maximum byte error |
| --- | ---: | ---: |
| Solid color | 84 | 1 |
| Asymmetric image color (including transposed chart) | 84 | 0 |
| Seam color | 105 | 1 |
| Material color | 84 | 0 |
| Metallic, roughness, emission (each) | 84 | 0 |
| Procedural color | 84 | 1 |
| Procedural normal | 84 | 0 |
| Transparent color | 84 | 0 |

The six comparisons passed together in 59.51 seconds. This measures sampled
interiors, not every texel, gutter or image format. The independent 2x3/3x2 image
fixture also verifies source-pixel center orientation. No local PNG baseline is
committed. The optional Qt project is excluded from native CMake/SwiftPM products
and default CI; it needs a separate developer Qt installation.

## Deliberate alpha policy

A separate `image-alpha` observation uses the same images with source alpha 128.
Portable output retains alpha 128 at every output pixel. The preserved reference
returned alpha 192 or 240 at sampled centers, depending on chart; its image
painting builds padded and exact layers using source-over operations. The probe
records these values without blessing that opacity increase as correct.

The portable API preserves straight-alpha source opacity rather than reproducing
that compositing artifact. Unobservable RGB under zero alpha is preserved by the
portable coverage mask. Consumers must use the documented straight-alpha blend
convention. This is an intentional difference, not a full pixel-parity claim.

## Remaining integration and release gates

This API does not yet cross the C/Swift ownership boundary, decode Apple images,
render PBR/transparency/AO, import embedded GLB or export textured models. Broader
asymmetric dimensions, discontinuity/gutter coverage, all pattern types and real
serialized cloth fixtures need expansion. Full Qt file/generator, native GPU and
M3/M4 acceptance remain separate. Inherited-core coverage is not established by
these focused contracts or by Swift smoke tests. See ADR 0014.

## Scoped coverage measurement

An LLVM-instrumented `texture_baker.cc` object was linked ahead of the ordinary
native archives into the real contract executable, which ran all its analytic
and generated-fixture cases. LLVM reports 670/695 executable lines (96.40%),
504/530 regions (95.09%) and 408/484 branches (84.30%). This measures this source
file only, not inherited procedural kernels, all headers, the C ABI or the
complete core. The domain's aspirational 100% target is not met by this report;
remaining degenerate/fallback and numeric-failure paths stay visible rather than
adding unreachable code to improve the number. Core issue #1 tracks broader
coverage and compatibility work.

## Native gate results for this stage

- Full Debug build/CTest: 33/33 passed, 52.05 seconds.
- Full ASan/UBSan build/CTest: 33/33 passed, 196.10 seconds under concurrent local
  workload; expanded invalid-input baker contracts subsequently passed in 2.80
  seconds under the same sanitizer configuration.
- SwiftPM source build and Swift 6 session/source/UV smoke consumers passed.
- New/core boundary C++ formatting, strict Swift manifest lint and diff checks
  passed. Native source scan found no Qt types/includes; `otool -L` on the baker
  executable lists only libc++ and libSystem.

Timings are observations on this developer machine, not performance budgets.
The optional Qt libraries themselves are not sanitizer-instrumented. Native core
and compiled reference code can be instrumented separately as described in the
reference harness README. No native UI changed in this C++ stage.

The final complete optional reference suite passed 18/18 in 172.19 seconds,
including the six portable comparisons, alpha observation, CLI rejection and
actual native archive/executable incremental relink regression.

## Review regression: tangent-stage cancellation

Automated review identified a cancellation gap after input validation and during
multi-million-triangle tangent generation. An internal, prevalidated stage is
isolated for a deterministic contract, avoiding sleep/latency assumptions or
production fault switches. The test first reproduced successful tangent
completion despite a cancelled token. The stage now checks before allocating
and every 256 triangles, preserving typed cancellation and owned output behavior.
Uncancelled and empty-stage cases are also exercised. This is cooperative
cancellation, not an operating-system scheduling or whole-call latency guarantee.

After the review fix, full Debug 33/33 passed in 44.20 seconds and ASan/UBSan
33/33 passed in 102.21 seconds. Swift 6 source/session/UV consumers and formatting
passed again. The full-resolution portable/reference procedural comparison
passed in 5.66 seconds, with unchanged color/normal samples.

## Review regression: dense procedural row candidates

A second automated-review finding identified a full triangle-list scan for each
image row. A deterministic 32x32-cell planar chart (2,048 triangles, 256-square
color map) first recorded 524,288 triangle-row visits. The row-event sweep now
records 22,336, while independent world-space color samples still agree across
the dense triangle boundaries. `rasterTriangleVisits` exposes that operation
count for diagnostics; it is not a whole-call work or latency limit.

Each triangle contributes a start/end event, with conservative bounds retaining
the original exact pixel-center predicate. Active triangle ordinals stay sorted,
preserving first-triangle precedence and gutter-distance comparisons. Workspace
is proportional to triangles, rows and the active set rather than duplicating
triangles into every row. Scheduling and candidate loops check cancellation in
batches. Triangles that actually span every row still require that work; no
universal linear-time or peak-memory guarantee is claimed.

After row scheduling, full Debug 33/33 passed in 28.13 seconds and ASan/UBSan
33/33 passed in 86.25 seconds. Swift 6 consumers passed. All six full-resolution
portable/reference comparisons passed in 24.23 seconds with unchanged measured
errors (color at most one byte, image/scalar/normal samples exact).

## Review regression: inactive surface preparation

Ordinary solid, image and material-only bakes now return from procedural
preparation before creating a vertex-position map. Preparation requires a
matching image-free, nonempty chart with an active pattern or nonempty folds
and positive wrinkle strength. The `surfaceVertexIndexEntries` diagnostic records
zero for absent, disabled, unmatched, image and empty-chart inputs; an active
analytic chart records its four unique positions. The regression first failed
because the ordinary path indexed all four vertices despite having no details.
Long owned validation and position-index loops also poll cancellation in batches.

A related serializer compatibility regression exercises actual ClothFolds
serialize/deserialize: normal components are written to three decimals and
therefore valid saved unit normals no longer have exact unit length. Validation
now accepts finite, nonzero normal lengths; the inherited fold constructor
normalizes its owned copy. Zero/nonfinite/overflowing normals remain rejected,
and caller inputs remain unchanged. The test first reproduced the exact-unit
rejection with diagonal normals rounded to 0.707.

After inactive-path and serialized-normal fixes, full Debug 33/33 passed in
31.11 seconds, ASan/UBSan 33/33 in 84.35 seconds, and all six Qt comparisons in
29.27 seconds. Swift 6 consumers, formatting, manifest lint, source/link Qt
checks and diff checks passed. Timings remain observations rather than budgets.
