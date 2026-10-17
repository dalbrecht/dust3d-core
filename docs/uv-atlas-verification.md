# Portable UV atlas verification

Core issue #29; ADR 0011. This stage extracts metadata, not texture pixels or a
renderer. Existing C and Swift ABI behavior remains unchanged.

## Failing-first evidence and contracts

The initial contract build failed because the portable API did not exist. Atlas
sanitizer execution then exposed inherited integer occupancy overflow, isolated
and fixed separately in reviewed PR #31. A further failing-first regression
showed that an exact ordinary-chart record could beat a cyclic seam record.
Canonical same-winding keys now preserve seam priority and actual corner order.

Contracts exercise analytic charts, proportional image-less areas, image aspect
and identity, both gradient seam sides, missing colors and gray endpoints,
broken-triangle charts, existing and propagated owners, cyclic/reversed winding,
owned lifetime, unchanged inputs, excess ownership suffixes, absent resolved
components and unmatched retained charts. Boundary tests cover exact payload
budgets, invalid dimensions/identities/options, all RGBA channels, nonfinite local
UVs, packed-coordinate overflow, malformed triangles and both position-key bounds.

Generated fixtures use the real mesh generator under the engine lock:

| Fixture | Charts | Known UV triangles | Gradient descriptors |
| --- | ---: | ---: | ---: |
| Capsule | 1 | 80 / 80 | 0 |
| Sword | 18 | 320 / 320 | 14 |
| RoundedCrossing | 2 | 306 / 434 | 0 |

RoundedCrossing's 128 unmapped triangles remain marked unknown. No complete UV
coverage, unique material ownership or Qt pixel parity is claimed. Generated
fixture visual inputs use resolved colors; image decoding is not exercised.

## Gates

Complete Debug build/CTest passes 31/31 (38.70 seconds). SwiftPM source build
and both strict Swift 6/warnings-as-errors session/source consumers pass. Full
ASan/UBSan build/CTest passes 31/31 with halt-on-undefined-behavior enabled.

Focused LLVM line coverage for uv_atlas_builder.cc is 255/258 (98.84%); all 12
functions execute, with 98.60% region and 95.83% branch coverage. The three missed
lines guard inconsistent layout count, invalid texture size and nonfinite layout
bounds returned by the packer. No production fault switches were added to force
these internal invariants. This report instruments the new builder, not the
whole inherited engine.
New API/test formatting, native adapter formatting, Swift manifest lint, source
Qt-boundary checks and whitespace validation pass. Existing inherited compiler
warnings are outside this feature. Focused LLVM coverage is recorded separately
from inherited engine coverage; no full-core coverage target is claimed.

## Remaining acceptance

No raster baking, C ABI/source-key adapter, Swift consumption, PBR/transparency,
image decoding, GLB import or texture export is implemented here. Logical payload
accounting excludes allocator/capacity/packing workspace and cannot guarantee
process-memory or time limits. Independent Qt chart/render comparisons, visual
texture acceptance and malformed asset decoding remain later stage gates.
