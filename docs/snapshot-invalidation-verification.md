# Semantic snapshot invalidation verification

Tracking: core issue #19; ADR 0005. Portable internal foundation only; no C API or
Apple dependency update is needed until a consumable session API is reviewed.

## Contracts

- First load invalidates every part/component and establishes a root revision.
- No-op snapshots and changes only to persisted `__dirty` attributes do not
  invalidate. Neither source snapshot is modified.
- Node/edge edits, additions/removals and ownership changes invalidate old/new
  owning parts. Unknown attributes count conservatively as changes.
- Part/node cut-face references propagate transitively; component ownership,
  ancestors and wrapping bindings propagate across old/new dependency graphs.
- Authored group changes invalidate descendant inheritance paths; dirty aggregate
  parents do not invalidate unrelated siblings. Reparenting and child order matter.
- Removed identities remain in the output for later eviction. Typed namespaces
  prevent same-string part/component IDs from becoming one dependency.
- Global canvas/root/animation changes invalidate all; cyclic/incomplete metadata
  terminates safely. Empty child slots do not invent empty component identities.

## Evidence

The first contract build failed because the new header/API did not exist. The
implementation then passed the focused contract and all 26 current Debug tests.
Expanded independent metadata graphs cover node/edge lifecycle, profile chains
and cycles, group inheritance, reparent/order/binding edits and deletion, unknown
metadata, dirty flags in both directions across every map, empty snapshots,
identity namespace collisions and complete source-map immutability.

Standalone Clang instrumentation of `SnapshotInvalidation.cc` measures 140/140
lines and all seven functions, with 108/110 branches (98.18%). This measurement
excludes inherited code. The remaining branches are uncommon root/direct empty
link shapes, not evidence of complete generator coverage. The fixture graphs use
opaque identifiers because this pure analysis does not validate/generate meshes;
existing archive/generation contracts separately exercise real UUID documents.

Final full Debug and ASan/UBSan suites pass all 26 tests each. The actual
SwiftPM engine product builds with the new helper, all native/test C++ formatting
and Swift manifest lint pass, and source/manifest Qt/Apple boundary checks pass. macOS ASan leak detection is disabled because the runtime does not
support it; the hosted Linux sanitizer job retains its configured checks.

## Remaining acceptance

Persistent caches are not activated. Cache budgets/ownership, external assets,
synthetic mirror eviction, failed-generation context recovery, session integration
and actual cached-versus-fresh
geometry comparisons remain required. Static snapshot analysis does not validate
archives, prove incremental performance, expose source IDs or complete M3.
