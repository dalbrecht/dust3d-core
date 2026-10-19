# ADR 0019: Parallel imported corner UV and model ownership

Status: Implementation under core issue #48, following #43 and #47.

## Context

Position-key UV maps cannot distinguish coincident source triangles with different
texture coordinates. Per-vertex reconstruction also collapses corner seams.
Generated mirror component IDs do not directly identify the original GLB material.
Removing the native imported UV/bake rejection without preserving these facts
would silently render incorrect surfaces.

## Decision

Add optional parallel triangle-corner UV records to decoded imported models,
part caches, mesh states, final Objects and leaf/group previews. Preserve the
inherited map for C++ compatibility, using it only when a caller supplies no
parallel input. The new channel is authoritative for decoded GLB corners.
Untextured triangles are explicitly unmapped. Coincident UV triangles can now be
represented without decoder rejection; all existing bounded GLB validation remains.

Carry a separate parallel resolved model key alongside original component keys.
This key matches the core's resolved asset/material identity, preserving case and
allowing mirrors to select the source GLB despite their derived component IDs.
It is not a replacement for original document instance bindings.

Use the Boolean combiner's exact source triangle identity to transfer attributes.
Retained corners copy original UVs; clipped corners use bounded source-triangle
barycentric interpolation. Generalize the existing corner-color transfer without
changing color arithmetic. Preserve coordinates outside [0,1] for sampler policy.
When recombination creates a bridge with no source triangle, leave its UV and model
key absent/empty rather than inventing a texture mapping. Copies, combination
caches, uncombined surfaces and preview expansion retain the parallel alignment.
Mirroring reverses UV corner order together with triangle winding.

Include all new owned capacities in decoded-import and retained-cache budgets.
These remain logical ownership budgets; temporary geometry/allocator overhead is
not a process RSS cap. No new dependency, C ABI structure or advertised UV/bake
capability is added. Existing native imported UV/bake rejection stays intact.

## Verification and remaining work

Independent synthetic GLB/archive tests cover coincident UVs, known source corners,
deformation, cold/warm caches, original and mirrored uncombined components,
leaf/group previews and inherited map-only callers. Mixed procedural/imported
union/subtraction fixtures exercise hard and smooth joins and compare genuinely
clipped UVs with a separate projected-coordinate oracle. Boolean tests require
actual generated bridges and verify that their attributes stay unmapped. Budget
contracts check reserved UV/model-key capacity and exact/one-byte-short limits.

Full Debug and ASan/UBSan contracts, formatting, Qt-free boundaries and SwiftPM
source compilation are required before review. Report measured coverage separately;
this does not establish full inherited-core coverage or Qt/render parity.

Subsequent stages must expose these facts through the UV/bake boundary, connect
imported factors/images/samplers, adopt PBR/alpha and rendered file previews, and
verify textured exports and broader Qt compatibility. Aggregate #41 remains open.
