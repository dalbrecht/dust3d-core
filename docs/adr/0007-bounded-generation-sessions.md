# ADR 0007: Bounded native generation sessions

Status: Accepted for source-matched prototype (core issue #23)

## Context

The Apple editor regenerates an entire document on each preview. The inherited
engine already supports component caches, but trusting persisted dirty flags can
reuse stale geometry. Cache state must remain private to an owning session, and
cache failures must not break valid primary generation.

## Decision

Expose an optional opaque C generation session, retaining the original decoded
snapshot, exact external asset revisions and one inherited generation cache.
Derive dirty flags from semantic old/new snapshots before generator preprocessing.
Use the shared engine mutex for legacy loads, session loads, reset and stats.
All returned meshes are independent immutable owners.

Compare asset type, normalized name, additional attributes and exact payload
bytes. Archive record order and storage offsets are not semantic revisions.
An optional noexcept document observer isolates metadata collection failures from
primary parsing. Retained assets own their bytes; no borrowed archive/XML views
survive the load call.

Limit retained object/value/buffer payload using the reviewed ownership visitors.
The default is 64 MiB; explicit zero disables retention; values above 256 MiB are
rejected. Asset payload is preflighted before copying, and actual generated cache
payload is measured after generation. Over-budget state is released in full.
This is not a peak-memory limit: temporary generation workspace, container-node
bookkeeping, allocator overhead and caller-owned output are excluded.

Cache preparation/allocation failures release state, report a separate bounded
cache diagnostic and generate fresh from the unchanged decoded source. Genuine
parse/generation/export failures clear partial cache state and preserve the
primary cause. Successful NO_GEOMETRY results can retain valid bounded state.
No genuine generation failure is silently retried with fresh state.

Per-call metrics count actual component-hit, component-rebuild and part-rebuild
sites in the engine. They are not inferred from invalidation decisions. The root
may rebuild even when children reuse geometry. No cumulative counters or timing
claims are exposed.

## Consequences

The existing stateless C API remains available. Swift actor ownership and editor
scheduling adopt the session in a subsequent reviewed dependency update.
This remains a source-matched prototype ABI, not a stable binary release.
Conservative invalidation and wholesale budget eviction may rebuild extra work.
Future archive decoding changes must audit the asset revision collector.
The full M3 source provenance, Qt differential corpus and performance acceptance
remain separate requirements.

See [verification and remaining limits](../generation-session-verification.md).
