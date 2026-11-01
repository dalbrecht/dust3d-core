# ADR 0200: Indexed node-source corner ownership

Status: local implementation; production core dependency remains unchanged.
This continues ADR 0199 and the ownership prerequisite for Apple #96.

## Decision

Node associations are hint metadata, not a proof of skeletal influence or original
surface ownership. Capture the engine's actual per-vertex node/weight decisions
before position-key maps collapse distinct vertices, and retain them parallel to
actual triangles as three owning SourceCorner records. Empty influence lists are
explicit unknowns. Unweighted records contain exactly one unit hint; weighted
records preserve finite nonnegative input weights without normalization.

SourceProvenance consumes a complete nonempty triangleSourceCorners channel by
original triangle and corner ordinal. It never consults positional maps to fill
missing indexed hints. Its legacy interface remains available only when the
channel is empty. Snapshot node membership and UUID alias ambiguity are resolved
at export as before. Generated mirror identities are not silently remapped to
original nodes. The native preview filter passes original ordinals even when an
earlier double-precision triangle collapses during float conversion.

Tube, stitch and loop builders already expose actual vertex source IDs; imported
geometry keeps its existing nearest-node hint decision separately for each original
vertex. Wrap output retains the builder's actual weighted associations. Combined,
uncombined and preview paths preserve aligned corner records and cache copies.

Boolean output uses the combiner's originating face, not a global position map.
Exact source-face endpoints copy original ordering and duplicate influences. New
corners interpolate sparse weights in double precision; positive unknown inputs
produce unknown output, and unsupported positive underflow/overflow likewise
produces unknown rather than fabricated or normalized weights. Mixed node hints
become weighted hints. This describes approximate provenance, not skin weights.

Recombination exposes the original pre-compaction vertex ID for each regenerated
vertex. A generated bridge corner may retain only a unique agreeing distribution
from actual incident source corners. Unknown or conflicting incidents remain
unknown. Consensus compares distributions, not influence-vector ordering; it does
not use MeshCombiner's positional vertex hints as proof of ownership.

Wrap bind inputs use one consensus sample per actual vertex. They do not duplicate
samples per incident triangle or consult positional maps for indexed meshes.
The existing single-node sampling heuristic chooses the greatest positive aggregate
hint after grouping duplicate UUID entries; exact aggregate ties retain first UUID
occurrence, while full exported weights remain unchanged.

## Validation and limits

Regressions cover coincident independent tubes, original and mirrored IDs, cache
copies, imported nearest hints, loop and weighted-wrap output, Boolean unit/weighted
interpolation, unknown contributors, bridge consensus, alias ambiguity, invalid
weight/channel admission, exact source payload quotas and float-filtered original
corner ordinals. Retained cache accounting includes nested influence capacity and
UUID text through explicit owning-record visitors. Full selected CPU/sanitizer and
app-build results belong in the external build evidence, not this design claim.

Corner records retain shared immutable influence distributions. Copies share the
same const vector and owned node text; complete replacement captures a new value
without mutating any aliases. This removes per-copy payload/vector/string allocations
without a custom container or mutable shared state. Exact influence order, duplicates
and scale survive sharing. Retained accounting conservatively charges the full
distribution plus an explicit shared-ownership metadata allowance per logical owner,
even when physical storage is shared; this is not an allocator/RSS measurement.

A bounded regression proves 96 corner copies retain one immutable distribution,
replacement and caller input mutation are isolated, and surviving owners retain
valid data after original release. Before sharing, the bounded coincident-tube
fixture accounted 9,984 source bytes for 96 corners. The shared policy intentionally
overcounts aliases, so its accounting number is not a physical savings benchmark.

Corner arrays remain eagerly populated for preview generations, and derived sparse
Boolean distributions still need working storage. Cache accounting can evict
conservatively, but output quotas do not bound inherited generation workspace or
process RSS. Representative large-document performance and broader pooling remain
pre-adoption follow-up work; no stress/exhaustion probe is authorized here.

Legacy positional colors, smoothing and reference Qt/rig data remain separate
compatibility paths. This migration does not enable hollow geometry, decide its
upper-bound/UV semantics, change document persistence or public C ABI, adopt recovered
mip/OBJ branches, change feature quarantines, or establish release/UI acceptance.
