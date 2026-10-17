# ADR 0014: Portable owned CPU texture baking

Status: Proposed (core #37; implementation and comparison gates in progress).

## Context

UV metadata alone cannot render the reference application's images, seam gradients,
material maps or world-space wrap details. The reference orchestration depends on
Qt images, painters and a process-global image registry. The core must retain
those algorithms' observable behavior without linking Qt into its products.

## Decision

Provide a C++17 `TextureBaker` alongside `UvAtlasBuilder`. Callers pass const
original geometry/atlas values and explicitly resolved material scalars, borrowed
RGBA8 image views and value surface settings/folds. The synchronous call owns its
result pixels and tangent records; it retains no input pointers. Image views
carry dimensions, stride and byte count. Decoding stays outside this library.

Color and emission images contain source-encoded straight-alpha RGBA8. Normal and
scalar maps contain linear RGBA8 data. Tangents carry direction, handedness and a
known marker in original triangle order; unknown UVs remain unknown. Images are
sampled without multiplying their colors by solid component colors. Seam
sampling retains the discrete gradient source and UV-axis transposition.

World-space surface details use the inherited SurfacePattern/ClothFolds kernels
with welded chart normals and increasing-U/decreasing-V tangent frames. Raster
rows execute sequentially, preserving triangle priority and propagating errors
without worker-thread exception or partial-launch hazards. Row distance scratch
is proportional to output width. A start/end row-event sweep maintains active
triangle ordinals in priority order, avoiding a complete triangle rescan per
row; events require two records per triangle. Candidate visits are exposed as
a diagnostic count rather than asserted to bound whole-call work. Deterministic four-neighbor dilation uses an
explicit coverage mask, so transparent texels are valid coverage.

Validate inputs before unsafe arithmetic, pointer reads or inherited numeric
conversions. Cancellation is cooperative. Logical payload accounting includes
output pixels and tangent records, excluding input storage, allocator overhead
and procedural/raster workspace. It does not establish a process memory or time
limit. Dimensions are bounded to 8192 and the output budget to 256 MiB.

## Consequences and remaining gates

No runtime dependency is added. Native products stay Qt-free. The optional Qt
harness remains a separate comparison tool. Interior reference comparisons measure at most one byte of color error and
exact image/scalar/normal samples in the current analytic fixtures. Source alpha
is preserved: the reference semitransparent-image probe increases alpha 128 to
192 or 240 through layered painting; this API deliberately retains 128. Gutter
and broader asset comparisons remain open. See texture-baker-verification.md.

This stage does not expose a C bake owner, adopt pixels in Swift, decode assets,
implement GPU PBR/transparency/AO, import GLB or add textured export. It does not
repair missing reference UV mappings. Those remain separate reviewed stages in
the full editor plan. Full M4 acceptance requires their integration evidence.
