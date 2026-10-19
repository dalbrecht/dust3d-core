# 0021: Bounded CPU color sampling for imported baking

Status: Accepted for the sampling foundation; imported atlas integration remains pending.

## Context

Imported rendering now preserves authored UVs and glTF sampler settings on Apple
platforms. The portable baker still rejects embedded models. Reusing its legacy
encoded-color interpolation for imports would produce different colors and discard
wrapping/filter intent. Neither Qt image types nor Metal can become core dependencies.

## Decision

Add an owned `ColorTextureSampler` in the UV domain. Callers provide decoded sRGB
straight-alpha RGBA8; decoding file formats remains outside this class. Construction
validates dimensions, checked readable spans, enum values and retained storage quota
before reading source pixels. It copies packed rows and retains no cancellation or
source pointers. Sampling is const, finite-input checked and independent of a GPU.

Implement repeat, mirrored repeat and clamp wrapping on both axes, nearest/linear
magnification and all six glTF minification policies with caller-supplied LOD.
Normalize large coordinates before integer conversion. Decode RGB before filtering;
alpha remains linear and straight, including RGB under zero alpha. Mip reduction
weights the full source footprint so odd trailing rows and columns contribute, then
quantizes back to sRGB RGBA8. This deterministic policy does not promise driver mip
bit equality. Construction cancellation is checked at row and mip boundaries.

A 64 MiB default, 256 MiB maximum logical quota includes root, descriptor capacity and
pixel capacity. It excludes input, temporary workspace and allocator overhead, and
is not an RSS bound. Preflight dimensions and complete planned storage precede pixel
allocation; actual retained capacities are checked too. No third-party dependency,
C ABI change or Apple pin update is needed for this stage.

## Consequences and next integration

Existing procedural bake behavior is unchanged. Source sampling UVs must remain
separate from packed atlas UVs. The next baking stages must resolve imported material
and vertex factors exactly once in linear space, select an explicit footprint/LOD,
rasterize aligned source triangles into the atlas, preserve alpha policy, publish
owned results, and verify textured export by independent re-import. Core #41 and
Apple #143 remain open. This helper alone does not enable imported-model baking.

Contract tests use hand-computed colors and tiny padded buffers, exercise all filters
and wraps, odd mip dimensions, owner independence, typed failures, overflow metadata,
quotas and cancellation. Full CTest and ASan/UBSan results belong in the PR evidence;
no whole-core coverage or full material parity claim follows from these contracts.
