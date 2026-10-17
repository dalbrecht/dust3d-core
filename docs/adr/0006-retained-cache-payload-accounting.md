# ADR 0006: Account owned retained payload before enabling cache sessions

Status: accepted for accounting; persistent generation sessions remain pending.

## Context

GeneratedCacheContext retains parts, component meshes, Boolean combinations,
mirror metadata and wrapping surfaces/weights. Exported vertex counts cannot
bound these owners. Geometry also contains borrowed views: SolidMesh refers to
vectors owned by MeshCombiner::Mesh, and its spatial tree refers to its box array.

## Decision

Add an allocation-free C++17 RetainedPayloadBudget. Check multiplication and
addition against remaining bytes before changing committed usage; rejection is
sticky. Account object/value storage, actual vector/string capacities, nested
metadata, owned pointers, spatial-index nodes and wrapping cache entries.

Classify borrowed data pointers and C ABI destruction callbacks explicitly. Do
not copy container comparison/hash policies during inspection. Stateless
allocators and policies without owned state are supported; unsupported ownership
shapes fail at compile time rather than silently disappearing. Inline geometry
containers need only their retained capacity/value payload, without per-element
walks. Dynamic owners supply a nonthrowing visitor.

Keep ownership visitors alongside core types. Bind every owner field with C++17
structured bindings so upstream field additions require an accounting audit.
Preserve inherited identifiers, formatting and CRLF headers; do not change public
C ABI layouts or enable cache reuse in this stage.

## Limits and consequences

This is a conservative retained *data payload* budget. Container-node/allocator
bookkeeping, transient generation work, caller-owned outputs and process RSS are
not a hard ceiling. Small-string capacity can be counted again despite inline
storage; that safe overcount may evict earlier. The session must account every
state object it retains, including source snapshots and any result copies.

Ownership must be valid and acyclic, and callers must keep objects alive and
unchanged while inspecting. Borrowed buffers count at their owner. Inspection
must still work under memory pressure; tests observe ordinary C++ new around
real cache and output traversal. No platform image/UI dependency is introduced.

Session ownership/eviction, shared engine serialization, external asset revisions,
failed-generation recovery and cached-versus-fresh geometry acceptance remain
required before enabling persistent caching or claiming M3 completion.
