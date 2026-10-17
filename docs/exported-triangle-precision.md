# Exported triangle precision verification

Core #14, ADR 0003. This stage closes the zero-area export defect investigated
while implementing the Apple wrapping controller.

The inherited double mesh and its float C ABI buffers were inspected separately.
For the radius-.2 single-sphere Cloth fixture with .225 opening at 64 faces, the
float top-opening mesh contained 14 zero-area triangles of 480, and bottom had 10
of 416. The inherited double meshes had one exact zero-area triangle each; most
export collapses therefore require checking the actual float coordinates.
These counts are investigation evidence rather than ordering/count goldens.

The geometry source is the same independent MIT fixture pinned in
[angle-normal verification](angle-normal-verification.md) to Apple commit
`6886b105fd0ea1a9bca86550d56e4be020d519a5`. No user documents or external assets.
Top/bottom .225 openings at 64 and 256 faces, plus .1 at 64, now require positive
exported area. Tests weld identical exported positions for semantic edge checks:
maximum two triangles per edge, opposed winding on shared edges, exactly two
body/garment components, and one degree-two boundary loop for the inner hem.
They do not compare vertex emission order or impose exact triangle counts.

A separate synthetic Model node of radius .001 centered at the origin proves
small representable geometry is retained without an arbitrary area epsilon.
The same node at (1000000,1000000,1000000), with origin zero, previously returned
OK with 48 vertices all collapsed at exported precision. The regression failed
before implementation and now requires GENERATION_FAILED plus the specific
precision diagnostic. A subsequent representable load must clear that diagnostic.
The existing straight-profile fixture also requires positive exported area and
retains its nine-node/three-part source counts.

All eighteen Debug and eighteen ASan/UBSan contracts pass locally on 2026-10-17.
SwiftPM source build, native/test formatting, manifest lint and Qt-boundary checks
also pass. Local macOS leak detection is unsupported; existing Linux CI retains
it. C ABI finite-value and bounds checks, buffer ownership, serialization and
successful-empty status remain unchanged. These tests do not establish inherited
coverage, general manifoldness, native UI/GPU acceptance or Qt parity tolerances.
