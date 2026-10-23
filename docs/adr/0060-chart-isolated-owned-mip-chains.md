# 0060: Owned atlas mip chains with explicit island ownership

Status: Locally implemented portable stage for core #59. Public review, native
ownership/metadata, Swift/rendering adoption and independent export acceptance
remain required. Core #41 and full M4 acceptance remain open.

## Decision

Keep `SourceTextureBaker::bake()` and its LOD-zero contract unchanged. Add opt-in
`bakeMipChain()` returning the existing base result once, additional owned maps,
one common sampling cap, retained bytes and cumulative texel work. No dependency,
C ABI, production engine pin or image decoder changes are needed.

Imported triangles retain their individual packed chart owners. The procedural
region is a container, not one UV island: callers borrow the original `UvAtlas`
through `SourceTextureMipBakeOptions::legacyAtlas` during the call. Transform its
layout rectangles through the outer region, including transposition. Match known
procedural triangle UVs to the same source atlas and require a single island to
contain each triangle. Layout/triangle matching comparisons consume the work
budget. Missing procedural layout metadata keeps the result at LOD zero, rather
than claiming the whole procedural region is isolated.

At every proposed level, floor-halve every map's actual dimensions. Require an
interior texel center in each island for every map, and disjoint one-texel collars
at the smallest map dimension. Stop before either condition fails. Checking only
the smallest map's centers is insufficient: different odd dimensions have
different center lattices. The requested level count includes the base and is
bounded to 1..14; stop when any enabled map reaches dimension one.

Average box footprints by exact pixel-overlap area, including odd edge texels.
Read only the previous level's matching island-interior samples. Reserve complete
destination collars before assigning gaps, then dilate only within that owner.
For the first procedural reduction, also verify that each base pixel's original
nearest source-image sample lies inside the original island. Otherwise odd-sized
resampling can place unrelated source padding inside the transformed rectangle.
Such samples cannot seed lower levels. No retained source coverage produces a
typed bake failure rather than a fabricated material.

A normalized chart point's bilinear footprint fits within its reserved collar.
Both levels of a permitted fractional LOD therefore remain owned by that same
chart; interpolation between them cannot introduce another owner. Color and
emission RGB decode sRGB before averaging, then encode once. Alpha is independent
and straight, including hidden RGB under zero alpha. Scalars average linearly.
Normals decode to signed vectors and normalize after averaging, with a neutral
tangent normal for an exactly opposing sum. Base normals already carry the
existing procedural transposition convention. Triangle policies remain in their
original order without alpha thresholding or geometry/UV changes.

## Bounds and remaining acceptance

The existing base dimension/material/chart limits apply. Count internal
procedural islands and imported charts together. Plan total retained bytes before
allocating additional levels, then inspect actual vector/pixel capacities. Charge
the wrapper root once, its base dynamic buffers, additional image records and
pixels. Temporary overlap/ownership/coverage/filter storage, inputs and allocator
overhead are excluded; this is not an RSS limit.

The configured texel-work quota applies cumulatively to base generation, matching,
ownership, filtering and dilation. The base default of 250 million remains in
effect; callers can configure up to the existing one-billion maximum for larger
chains. No large-model throughput, peak-memory or default-resolution benchmark is
claimed here. Cancellation remains cooperative. Allocation errors propagate to
the caller; no deliberate failure injection or resource-exhaustion test is used.

Native adoption must generate chains while the original procedural atlas and
decoded images still live, retain all levels within the native owner quota and
expose exact dimensions/counts/caps. Swift must copy those buffers before releasing
the owner. Rendering must honor the partial-chain cap. Exporters unable to retain
these limits need explicit base-only behavior or another validated representation.
No current native sampling cap is raised by this portable stage.

## Verification

Test-first failures established the missing API, unresolved procedural ownership,
unmatched island admission, transposed odd-resampling padding leakage, differing
map center lattices and the packer-rounding boundary. Each now passes. Bounded
fixtures cover diagonal high-contrast/transparent charts, bilinear corners and
fractional LOD transitions, mixed imported/procedural ownership, straight alpha
and hidden RGB, odd edge-area weighting, scalar/emission semantics, normalized
and opposing normals, absent maps, exact/minus-one retained/work quotas,
cancellation and ordinary malformed metadata. Base maps remain byte-identical to
the original API. No snapshot writer, forbidden fixture or crash probe is used.

Final implementation validation on October 23, 2026:

- CMake Debug: all 50 ordinary contracts pass, 14.46 seconds.
- ASan/UBSan with coverage instrumentation: all 50 pass, 70.73 seconds; the focused
  mip contract passes again. No sanitizer findings.
- Single-binary focused coverage for `bakeMipChain`, its lambdas and `filterMip`:
  307/308 lines (99.68%), 212/213 regions and 169/182 branch outcomes. The remaining
  line is the defensive nonpositive-area guard. This is not whole-engine or full
  baker coverage, nor a claim to satisfy inherited 100% core coverage.
- SwiftPM builds; inherited UUID seed-conversion warnings remain. Formatting,
  whitespace and source quarantine checks pass. The instrumented mip test links
  libc++, libSystem and the sanitizer runtime, with no Qt or Apple UI framework.

Validation reuses `/private/tmp/dust3d-validator-safe-build` serially. Instrumented
compiler products are cleaned afterward, and CMake flags reset before reuse.
No UI changes or app screenshot apply to this core-only stage.
