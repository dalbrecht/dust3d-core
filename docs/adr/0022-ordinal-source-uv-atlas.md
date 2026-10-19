# 0022: Ordinal source UV atlas layout

Status: Proposed implementation for core #54; native bake adoption remains pending.

## Context

The inherited atlas accepts normalized component charts keyed by triangle positions.
Imported authored UVs can repeat outside that range. Coincident imported components
can share positions while requiring different colors, model identities or UV seams.
Clamping authored UVs or merging by position would lose those distinctions.

## Decision

Add `SourceUvAtlasBuilder`, leaving `UvAtlasBuilder` and its procedural pixel policy
unchanged. Each imported original triangle receives an independent geometry-derived
chart addressed by its ordinal. Retain exact model/component strings and optional
authored UVs separately from normalized packed UVs. Textureless imports remain
explicitly source-unmapped while still gaining packed output coordinates.

Pack an existing legacy atlas as one region and return its transform. Legacy-only
calls return unchanged known UVs and texture size; unknown procedural mappings stay
unknown. Mixed calls allocate geometry-scaled imported charts and one square legacy
region. Preserve corner ordering and expose chart transposition explicitly. Reuse
the existing portable ChartPacker, without new dependencies or changes to inherited
packing heuristics. Check finite arithmetic before converting chart sizes to float.

Validate aligned arrays, indices, finite geometry/source UVs, normalized known legacy
UVs and representable nondegenerate imported area. Preflight the output-record and
identity lengths plus chart count; account actual returned vector/string capacities.
The default logical retained quota is 64 MiB, capped at 256 MiB. It excludes temporary
packing workspace, allocator overhead and caller inputs. Default chart count is
50,000, with supported explicit limits from 1 through 100,000. Packing tries at most
100 scales. Cooperative cancellation checks occur through validation and between
attempts; they do not interrupt an inherited packing heuristic. No time/RSS bound is
claimed. Add a typed cancellation case to the C++ UV error enum; no C ABI changes.

## Tradeoffs and remaining integration

Per-triangle charts favor correct source ownership over atlas utilization. Their
padding and rectangular waste can reduce density; large models need measured packing
and sampling budgets before production acceptance. Degenerate/unrepresentable input
fails explicitly rather than returning fabricated UVs. Native adoption must reconcile
original ordinals with its final float-filtered triangle order, preserving existing
surface alignment rules.

This stage does not change the document loader's imported UV/bake rejection. The
baker must next rasterize these packed triangles using the independent authored UVs,
CPU sampler, linear vertex/material factors and explicit footprint/LOD. It must
compose the legacy bake through the returned region transform, retain alpha/scalar
policy, publish owned native results and verify textured exports by re-import. Core
#41 and Apple #143 remain open; no Apple dependency update is needed for this layout
stage alone.

## Verification

Numerical tests independently check transforms, coincident ownership, authored repeat
UVs, textureless sources, non-overlap, normalized output, exact quotas, caller lifetime,
legacy-only stability and malformed/numerically extreme inputs. The real synthetic
MIT ImportedSurface.ds3 fixture also runs through document decoding and MeshGenerator
before layout checks. CMake/CTest, sanitizers, SwiftPM and scoped coverage results must
be recorded before publication. No UI screenshot applies to this portable library.
