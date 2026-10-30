# ADR 0197: Base-only fallback for unproven mip ownership

Status: proposed; tracks [core #59](https://github.com/dalbrecht/dust3d-core/issues/59)
and [Apple #180](https://github.com/dalbrecht/dust3d-swiftui/issues/180).

When a known procedural triangle's normalized source UVs match the packed atlas
but no single procedural island contains all its corners, retain the ordinary
base bake and expose no additional mip levels with maximum sampling LOD zero.
The atlas is not sufficient proof that a lower-resolution sample stays within
the original surface. Do not guess a chart, lower validation, or claim the
fallback provides isolated minification. The result retains the specific
ownership reason and the native geometry warning reports it to existing callers;
growth of the owned warning is charged to the retained-output quota. Continue validating every triangle,
including after an unowned triangle is found: missing metadata, nonfinite or
out-of-range UVs, and inconsistent packed coordinates remain invalid.
Independent quota, cancellation, chart and overlap checks remain in force.

Pure synthetic regressions cover the previously rejected ownership case, a
mixed procedural/imported atlas, unchanged base maps, and the existing malformed
and mismatched UV cases. A separate native warning unit verifies preservation of
an existing diagnostic and quota accounting without opening a document. These tests do not invoke imported-model generation or
alter the GLB quarantine. The real mixed-document failure supplied the trigger,
but no claim is made that this change has been verified against that document.
The actual cause of its missing island attribution, safe mixed mip levels,
GPU minification and textured export/re-import remain open. A reviewed core
dependency update and separate downstream validation are required before the
Apple app can use this behavior.

Before exposing a positive LOD for procedural islands, the opt-in baker also
checks whether each chart's base-level ownership can be filled from source texels
strictly within that chart. It reconstructs the base collars from those owned
samples, then generates lower levels. If an owned chart cannot be filled, it
keeps the original base and returns LOD zero with a distinct diagnostic. The
base-only loader and explicit single-level requests retain their original bytes.
Work for ownership and coverage traversal uses the existing cumulative work
limit; retained level quotas are checked before isolation. Temporary ownership
and queue buffers remain workspace, not a hard process-memory guarantee.

The independent poisoned-gap test samples procedural chart corners at every
admitted level and fractional trilinear transition, including the base. It covers
straight alpha and hidden RGB, emission, metallic, roughness and tangent normals,
and checks exact and one-short work/output quotas. This is bounded synthetic
evidence; real mixed-document and GPU validation remain outstanding.

For opt-in mip preparation, both atlas stages request extra chart padding based
on the smallest enabled output map. The procedural stage first packs for the
guaranteed color and material maps. If the normal dimension would require more
padding, it validates inputs and uses the texture baker's own fold eligibility
and geometry witness on that atlas. When a normal image is predicted, it releases
the initial atlas and repacks once before the sole pixel bake. The source atlas
uses the actual baked normal presence. This is a packing heuristic, not a promise
that any mip level is safe: the ownership, footprint and collar proofs still determine the
maximum LOD. If the padded atlas cannot be packed, a typed packing failure alone
triggers an unpadded base-only retry and its original cause remains in the native
warning. Invalid input, cancellation and quota failures are not retried. If the
base retry also fails, both causes and the base failure type are retained. The
legacy base-only request keeps its existing packing behavior.

A synthetic 25-chart, 32-pixel case verifies that excessive optional padding
still leaves a usable base atlas. Another in-memory mixed-atlas case establishes
positive LOD and independent bilinear and trilinear checks against poisoned gaps.
Neither case runs real imported-model generation or validates GPU rendering.

Normal planning adds one validation and geometry witness and, when needed, one
additional procedural atlas pack. The witness is bounded by admitted input sizes
and uses at most 64 linear probes before building a vertex index; each atlas pack
retains its existing payload limit and 100-attempt cap. The first atlas is released
before repacking. These are bounded extra costs, but the raster-sample quota does
not account for geometry sorting or packing, and no aggregate CPU or hard memory
containment is claimed. Performance and containment remain release work.
