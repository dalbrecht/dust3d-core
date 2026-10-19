# 0024: Owned native source-aware baking

Status: Proposed implementation for core #58; Apple SDK/render adoption remains pending.

## Context

Portable source sampling, layout and rasterization now exist. Existing native UV and
bake APIs intentionally reject embedded models; changing their input/output contracts
would silently change established clients. The new API must preserve source identity,
material policy, float-retained triangle alignment and caller-buffer lifetimes.

## Decision

Add an opt-in sized `Dust3DSourceBakeOptions` and opaque `Dust3DSourceBakedMesh`. The
caller supplies decoded sRGB straight RGBA8 images keyed by exact material model IDs,
plus existing procedural bake options/images. The core resolves actual glTF factors,
filter/wrap settings and source color encoding. Required unresolved images fail with
the original typed diagnostic. Decoding remains outside the portable engine; the core
can validate storage/identity but cannot verify that caller pixels match encoded bytes.

Reuse the serialized generation pipeline with a named source-bake observer. Retain
original ordinals only for triangles that survive the shared float export validation.
Source metadata/catalogs finish before baking, then the observer constructs float-
retained geometry and independent source layout data in exactly that final order.
No second generator/export implementation or Qt bridge is introduced.

Build a procedural-only subset using original double positions, component UVs,
seam gradients and broken-chart fallback records for retained procedural triangles.
Resolve visuals and bake through the existing shared native resolvers/domain baker.
Compose that base with imported charts through SourceTextureBaker. This preserves
legacy color policy, scalar/emissive/normal data and explicit unknown mappings.
Direct imported component color-image overrides are currently unsupported and reject;
never silently discard them in a successful bake.

Return packed UVs, checked tangents, model/component indices and membership, alpha/
cutoff/sidedness policy and borrowed map views under one owner. Known mapped vertices
use neutral modulation because output pixels already contain source factors. Unknown
procedural vertices retain their ordinary source color. Clear only the obsolete
imported-preview material warning after successful composition; preserve other text.

The owner retains no generated Object/Snapshot, caller image descriptor/buffer,
imports reference or cancellation token. Root/catalog/map/triangle ownership remains
independent of subsequent calls. The matching destroy function releases all storage;
null and invalid-selector accessors return explicit empty/unknown values.

## Bounds and failure containment

The default aggregate retained quota is 256 MiB, also its maximum. It includes roots,
geometry/source/catalog capacities, native triangle records and final map capacities.
Sampler and source-atlas quotas default to 64 MiB, capped at 256 MiB independently.
Sampler accounting sums owned image/mip payloads, excluding lookup workspace. The
nested legacy options retain independent UV/procedural bake workspace ceilings.
Working geometry, decoder/generator/lookup/raster workspace and allocator bookkeeping
are excluded; this is not an RSS bound.

Preflight mandatory final map bytes before rasterization and check actual capacities
before publication. Default chart count is 50,000 (maximum 100,000); texel-work default
is 250 million (maximum one billion). Up to 10,000 decoded model-image views are accepted.
Validate exact non-NUL identity spans, dimensions/row spans, duplicate identities and
options before reads/allocations. Reuse cooperative cancellation at generation phase,
source-copy, atlas/sampler and raster checkpoints, with a final pre-publication check.
No whole-call cancellation latency guarantee is made. Catch all exceptions at C and
preserve typed causes, including allocation failures, without partial owners.

Output reports `maximum_sampling_lod=0`. Native and Apple consumers must propagate
and enforce it; globally generated combined-atlas mips are unsafe until required M4
issue #59 establishes chart-safe minification. Source-image derivative mip sampling
remains supported independently.

## Verification and remaining work

Contracts cover real imported/procedural archives, mixed source geometry, mirrored
membership, float-collapsed filtering, textureless models, encoded fallback versus
linear vertex colors, accepted sampler variants, exact quotas, malformed small views,
pre-cancellation, ownership after caller release/subsequent failures, and injected
allocation failures at owner/map creation. An analytic observer contract exercises
colored patches, a seam gradient, broken-chart cap, fold normals and PBR composition.
A pure-C consumer and strict Swift 6 smoke client verify the public module boundary.

Full CTest/ASan/UBSan/SwiftPM and measured changed-file coverage belong in the PR record.
No whole-engine coverage, Qt differential parity, binary ABI stability or finished M4
claim follows. Apple dependency/SDK copying, decoding/runtime integration, rendered
acceptance and independent textured export/re-import remain. Core #41 and Apple #143
stay open; #59 remains required before enabling combined-atlas mip sampling.
