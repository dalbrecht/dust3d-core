# ADR 0199: Triangle-indexed procedural UV ownership

Status: local implementation; not approved for app dependency adoption.
Tracks Apple #96's prerequisite ownership refactor. Hollow geometry stays disabled.

## Problem and trace

Tube and stitching builders produce face-corner UVs, but MeshGenerator historically
stores them in componentTriangleUvs maps keyed by three quantized PositionKeys.
The legacy atlas canonicalizes cyclic position triples and merges charts globally.
Coincident or near-coincident triangles can therefore acquire another surface's
coordinates or owner. This is unsuitable for inner/rim geometry, and a position
match cannot establish triangle or source identity.

MeshCombiner already reports the originating operand/triangle for Boolean output.
MeshState uses that information to transfer imported corner attributes; recombination
also reports retained source triangle ordinals. Quad recovery only changes the
polygon presentation array, not the aligned triangles. Those identities provide
an existing route for procedural corner attributes without reconstructing them
from positions. Procedural component keys now travel separately in exact original
string form, retaining the distinction between document aliases.

## Decision

Introduce a distinct optional ProceduralTriangleUv record parallel to actual
triangles. Each engaged record owns its component UUID and three normalized local
chart coordinates in actual corner order. This is separate from imported authored
triangleCornerUvs. A nonempty channel is complete in length: null records explicitly
mean unowned or unmapped output. Atlas consumers must not fill them from coincident
legacy geometry. An explicitly generated gradient record has no single component
owner: it instead carries an opaque chart identity and topology-derived endpoint
owners. MeshRecombiner exports actual bridge face ordinals and boundary vertex IDs,
so each generated seam uses the established 512-pixel gradient without a position
match. Missing or ambiguous endpoint ownership retains the prior gray default;
chart identity is used only for grouping, and first-face order keeps equivalent
cold builds deterministic. Empty channels retain the legacy compatibility path.

UvMapPacker accepts triangle-ordinal maps in addition to its legacy position maps.
Packing transforms coordinates and preserves ordinal identity; it does not join
charts merely because vertices coincide. The indexed atlas groups chart inputs by
explicit owner, sums area from actual triangle indices and returns independently
owned, triangle-aligned UV/owner/known arrays. A missing component visual input
leaves its UV mapping unknown while preserving its known owner.

Object UV copying may copy indexed records only for matching triangle topology.
Mismatched topology gets explicit unknown records. Copying a legacy channel clears
stale indexed state. Retained cache accounting must include optional owned UUID
text, not just inline corner storage; logical atlas accounting includes ordinal
map records and layout records and remains distinct from temporary workspace/RSS.

The source-bake adapter retains the original triangle index through float filtering,
copies its indexed record into the compact procedural object, then scatters packed
UVs through the retained-index map. An original-index remap preserves shared source
vertices without welding distinct coincident IDs, maintaining procedural detail
normals and adjacency. This avoids rebuilding ownership through rounded
positions after dropped or coincident faces. Existing imported source-atlas metadata
stays separate.

## Incremental validation

New bounded synthetic regressions cover coincident and quantization-colliding
triangles, distinct owners and same-owner seams, cyclic/reversed corner orders,
explicit unknown mappings, malformed channel lengths/coordinates, exact logical
quota boundaries, copy independence and retained owner storage. A native source-bake
regression drops a degenerate prefix and verifies that the two retained coincident
owners sample their own red/blue charts. Producer/Boolean/mirror, exact bridge ordinal, cache, preview and texture-detail
regressions accompany the migration. Texture admission applies the old integer
position-key bound only when a legacy lookup is actually required. Indexed chart
baking uses finite actual coordinates and vertex IDs. In a layout containing both
mapping forms, the indexed entries are authoritative.

The old RoundedCrossing UV test expected unmapped clipped faces from positional
lookup loss. Its expectation is now complete mapping for that licensed fixture:
source barycentric-corner and explicit generated-gradient regressions establish
where those mappings originate. Separate synthetic null records still require
unknown output and prohibit borrowing from coincident geometry.

Run the source quarantine guard before fresh builds and only inspect/run audited
ordinary targets. No allocation-failure, crash, OS-signal or exhaustion probes are
introduced or authorized. Test results and remaining gaps belong in the verification
record; an implementation proposal is not evidence that they passed.

## Remaining scope and constraints

Source-node UUID and weighted-hint migration is implemented separately in ADR 0200.
Legacy positional color, smoothing and reference rig attribute maps still need
separate migration/audit. Boolean-generated seams must remain explicit when no
source interpolation is proven; this ADR does not invent inner/rim UV policy or
hollow scalar semantics. Broader geometry/texture and app compatibility must be
verified before claiming the entire ownership refactor complete.

The production app core revision stays 48b0ce2d6d61cf58731bb0c3e14ef5965a686d68.
Recovered mip/OBJ branches remain independent review work. No app pin, document
schema, public C ABI, hollow geometry, quarantine rule or release approval changes.
