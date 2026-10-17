# ADR 0011: Portable owned UV atlas metadata

Status: Accepted for implementation; core issue #29.

## Context

The reference UV orchestration lives in the Qt application and depends on color
parsing, global image lookup and image creation. The native engine needs its
chart algorithms before baking or exposing texture data to Swift. Reimplementing
packing in Swift would duplicate geometry behavior and complicate upstream sync.

## Decision

Add a C++17 `UvAtlasBuilder` in `dust3d/uv`, using the inherited UvMapPacker.
Callers supply resolved normalized RGBA colors and optional image identities and
dimensions. Results own layouts, gradient descriptors, triangle corner UVs,
explicit mapping masks and component UUIDs. Input Object data remains unchanged.
No Qt, Apple framework, image registry, random UUID or new dependency is needed.

Seam charts precede ordinary charts. Gradients use layout ordinals and endpoint
colors instead of allocating global images. Image-less charts receive area-based
sizes; broken-triangle solid charts retain their fallback. Same-winding cyclic
triangle keys are canonicalized and UVs rotated back to actual corners. This
intentionally repairs an exact-order lookup gap in the reference orchestration;
reverse winding is not treated as the same chart. Existing scan/edge priority
remains for propagated ownership. Neither known UVs nor propagated IDs establish
unique material ownership when source surfaces overlap.

Validate geometry, quantization, colors, dimensions, UVs and packed output before
publishing. Typed errors distinguish invalid input, output quota and packing
failure. A default 64 MiB logical payload budget, maximum 256 MiB, accounts for
retained elements; it does not cap allocator overhead, workspace or CPU time.
The numeric packer safety prerequisite landed separately in PR #31.

## Alternatives and consequences

Keeping Qt orchestration would violate the portable target boundary. Translating
it into Swift would split algorithm ownership. A new atlas library would add a
dependency and alter inherited packing; reuse minimizes that divergence.

This is a C++ source API, not a new C ABI or Swift feature. A future adapter must
catch all exceptions, serialize inherited engine work, preserve original opaque
source-key spellings, and remap original triangle ordinals to float-filtered
exported geometry. Baking, pixel/color-space contracts, GPU shading and texture
exports follow separately. Missing charts remain explicit and must not be shown
as complete texture support. Qt differential and rendered acceptance remain open.
