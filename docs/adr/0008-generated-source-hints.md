# ADR 0008: Optional generated source hints

Status: Accepted for the source-matched prototype, pending full verification and review.

## Context

M3 needs generated source associations for native inspection and picking. The
engine already records position-to-node and wrapping weights, but these are
quantized associations rather than exact surface provenance. Boolean output can
lack an association; generated mirror/interpolation identities need not belong to
the original document. The inherited UUID parser accepts opaque non-hex keys and
normalizes braced/unbraced aliases to the same engine identity. Indexed rendering
can merge vertices with identical attributes even when their source hints differ.

## Decision

Add an opt-in `dust3d_sourced_mesh_load` entry point and a separate opaque owner.
Legacy and cached-session entry points retain their behavior and allocate no
source export buffers. All generation entry points share `engineMutex()`.

Capture an owned original node/part identity table before transferring the snapshot
to `MeshGenerator`. Never emit generated IDs as original document keys. Resolve
engine identity aliases as ambiguous, retaining the original table without
arbitrarily selecting an alias. Identifiers remain opaque archive byte strings, not
Foundation UUID values. Source identity is a hint: quantization and generated-ID
collisions can associate an existing original identifier with an imprecise surface.
This API does not claim exact Boolean/component ownership.

Export one record per retained expanded triangle corner. Append it only after
float-precision triangle filtering, so source corner i also matches index i when
an independent indexed mesh is created. Source records remain separate from
render-vertex deduplication. Prefer the complete weighted association over the
single-node map when both exist. Preserve every weight, including unresolved
entries, without renormalizing. Reject negative/nonfinite weights; represent
missing identities and aliases explicitly rather than inventing a node.

Before new `PositionKey` lookups, check the actual scaled double against signed
long bounds. Use an exclusive power-of-two upper limit: on Apple Silicon,
`long double` cannot accurately represent `LONG_MAX`, so a rounded inclusive
comparison could still admit an undefined cast.

Options carry their exact struct size. The source logical payload limit defaults
to 64 MiB and cannot exceed 256 MiB. Charge identity records, owned string lengths
including terminators, corner records and influences before each growth. Size
subtraction prevents overflow. Geometry, allocator overhead, vector spare capacity
and temporary identity lookups are outside this logical payload limit. Source
allocation/budget failure fails only the opt-in operation; no partial owner escapes.
RAII releases geometry, original strings and arrays on every failure.

The owner lends immutable geometry and source arrays until destruction. Callers
must not destroy its borrowed geometry; `dust3d_mesh_create_indexed` makes an
independent copy. Swift consumers copy source arrays and strings before destroying
the owner. Neither STL nor C++ exceptions cross the public header.

## Consequences and next stages

The initial source call is stateless. Integrating source export with the bounded
session, copying it into Swift value types, and source-based native picking are
subsequent reviewed stages; this PR alone does not finish M3. No dependencies or
licenses change. Upstream generator paths and numerical behavior are preserved.

Tests cover actual chains/Boolean examples, multi-node wrapping, mirrored and
interpolated geometry, float-filtered corners, identity aliases, owner lifetimes,
source limits and C/Swift importers. These are compatibility fixtures, not a claim
of exact source provenance or complete Qt differential acceptance.
