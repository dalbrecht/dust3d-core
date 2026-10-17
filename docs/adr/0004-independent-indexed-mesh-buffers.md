# 0004 — Independent indexed mesh buffers

Status: Accepted for core conversion; Swift/renderer integration follows separately.

## Context

The native adapter exposes expanded triangles. M3 needs indexed geometry while
preserving the separate viewer and existing clients. Indexing only positions
would incorrectly merge hard-normal or color seams. The new representation also
needs clear ownership when the expanded source is released.

## Decision

Add an opt-in `dust3d_mesh_create_indexed` conversion and an opaque immutable
`Dust3DIndexedMesh` owner. Unique vertices retain the existing Dust3DVertex layout;
indices are UInt32. Complete finite position/normal/color values form the sharing
key, and vertices are assigned in first-appearance order. Equal numeric signed
zero values may share; reconstruction preserves attribute values, not zero sign
bits. Source counts and compatibility warning are copied. Every returned buffer
belongs to the indexed handle, with no dependency on the source owner after the
conversion returns.

Keep generation and the expanded API intact. Conversion reads immutable buffers
without accessing inherited engine globals. Move the existing bounded diagnostic
writer into the internal adapter namespace for both entry points; preserve all
existing load statuses and serialization.

Take an explicit exported-payload byte budget: zero selects 256 MiB, with exact
vertex/index payload accounting. Check index arithmetic, unique-vertex capacity
and UInt32 conversion bounds before publishing ownership. Reject incomplete or
nonfinite input rather than allowing invalid ordered-key comparisons. All C ABI
exceptions are caught and converted into typed diagnostics.

## Alternatives and limits

Direct indexed generation could reduce the expanded intermediate, but couples
this correctness foundation to a larger generator refactor. Keep that optimization
for measured performance work after reconstruction/lifetime contracts exist.
Payload budget is not a whole-process bound: source buffers, temporary tree lookup
storage, allocator overhead and later Swift copies require separate accounting.

This API carries the currently exposed position/normal/color attributes and
source counts, not per-vertex source IDs, UVs, rig weights or materials. Those
remain additive stages. Existing public layouts/status numbers are unchanged;
this prototype does not establish distribution ABI stability or full M3 parity.
