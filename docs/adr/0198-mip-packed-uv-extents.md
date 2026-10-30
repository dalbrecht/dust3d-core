# ADR 0198: Use packed chart extents for opt-in mip UVs

Status: proposed; tracks [core #59](https://github.com/dalbrecht/dust3d-core/issues/59).

The inherited chart packer truncates padded rectangle sizes to integer units,
but the UV mapper uses the original unquantized part dimensions. A mapped
corner can therefore extend beyond its reported chart, including by more than
the existing ownership-check tolerance. Expanding that tolerance could admit
pixels belonging to a neighboring chart.

For opt-in mip preparation, map normalized UVs using the already packed layout
extents, including after rotation. The allocation, layout, padding, overlap
checks and ownership tolerance are unchanged. Default UV atlas callers retain
the inherited unquantized mapping. The native source-bake mip path explicitly
enables this option; the base-only loader does not. This changes the opt-in
mapping by the packer's quantization discrepancy rather than asserting that a
UV outside its owning rectangle is safe.

Pure tests use noninteger, unrotated and rotated charts, verify interior
disjointness and exact corner containment, and compare the default mapping and
packed placement with the inherited formula. A synthetic two-component atlas
flows through `UvAtlasBuilder`, `SourceUvAtlasBuilder` and the source mip baker:
the default layout reproduces the unresolved-owner condition and the opt-in
layout proves ownership. No imported document or GLB generation is needed.

This correction alone does not guarantee useful mip levels: the actual padding,
sampling footprints and per-level overlap checks still govern the LOD cap. Real
mixed-document and GPU validation remain open, and all existing quarantines
remain in force.
