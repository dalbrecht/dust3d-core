# ADR 0005: Derive cache invalidation from snapshot changes

Status: accepted for the invalidation foundation; persistent cache activation pending.

## Context

The inherited generator accepts persistent component/part cache contexts, but uses
`__dirty` snapshot attributes to decide what to rebuild. Archive flags cannot serve
as a trusted revision authority for native edits, undo, document replacement or
foreign files. Parent attributes, profiles and wrapping bindings introduce
cross-component dependencies.

## Decision

Add an internal portable C++17 snapshot comparison returning dirty part identities,
dirty component identities and a root flag. A null previous snapshot means first
load. Both source snapshots remain immutable. Ignore `__dirty` at every attribute
map; compare all other attributes conservatively, including unknown metadata.

Changed nodes and edges invalidate their old and new part owners. Part and
node-level cut-face references propagate profile changes to consuming parts.
Parts propagate to linked components, children to parents, and wrapping bodies
to binding consumers. Use both snapshots so removed references and identities
remain invalidated and can later be evicted. Separate part/component namespaces.

Authored component changes invalidate old/new descendant inheritance paths and
linked parts. Aggregate invalidation from a child does not flow back down into
unrelated siblings. Visited sets terminate dependency/subtree traversal even for
cyclic or incomplete metadata; document validation remains a separate boundary.
Canvas, root and animation changes conservatively invalidate all entries.

## Consequences

This provides deterministic invalidation analysis without Qt, new dependencies,
C ABI changes or enabling persistent cache reuse. Exact attribute spellings can
produce safe extra invalidation (for example `1` versus `1.0`); this is not a minimal
numeric fingerprint. Root changes currently invalidate the entire graph.

Before activation, a generation session must validate inputs, include external
asset revisions, manage cache ownership and budgets, prune removed/synthetic
mirror entries, discard partially updated contexts after failed generation, and prove cached output matches fresh generation across the
modifier/profile/binding/history corpus. This helper alone does not establish
incremental generation, source provenance or M3 acceptance.
