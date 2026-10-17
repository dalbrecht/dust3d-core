# Contract fixtures

These MIT project fixtures come from the standalone viewer at commit
`4f88ebf8`. `Capsule.ds3` was produced with the repository's independent Python
archive writer; `Sword.ds3` was generated from
`tools/agent_modeling/examples/sword.json`.

Expected source semantics: Capsule has 3 nodes and 1 part; Sword has 18 nodes and
5 parts. Tests check finite geometry, unit normals, retained colors and ownership;
they deliberately do not assert incidental vertex ordering or exact triangulation.

The archive files contain byte offsets and a CRLF header. Do not text-normalize
them. Future fixtures must include source/license, expected behavior, tolerance and
upstream revision metadata. Keep minimized crash reproducers as regressions.

`RoundedCrossing.ds3` is a synthetic MIT fixture independently encoded with Python
for core issue #3 (Codex, 2026-10-16). It has five nodes, two parts and three edges,
with both parts subdivided and rounded, default interpolation and Normal combine.
The source revision is `685b68fb45405dabb4bb6f72c5b0e2598d4b9fdf` (engine sources
unchanged at `e9edb9da5d99251d870facb7810ef4a93b7d33b5`). Expected behavior is a
complete finite colored mesh with unit normals within the existing 0.001 squared-
length tolerance; no incidental triangulation baseline is prescribed. The original
core returns generation failure with repeated-halfedge/polygon-ring diagnostics.

The related synthetic MIT fixtures use the same source revision and provenance:

- `RoundedCrossingTranslated.ds3` translates origin and nodes by 0.123456 on all axes; the union remains one connected surface.
- `RoundedCrossingHard.ds3` enables hard joins on the second part; the union remains one connected surface.
- `RoundedCrossingDiff.ds3` centers the crossing bar at Y 0.5 and uses Inversion, unambiguously cutting the vertical part into two surfaces.
- `SeparatedNearWeld.ds3` uses two single-node parts of radius 0.1 with centers separated by 0.20004; the 4e-5 gap must retain two surfaces.

The new contracts weld exported float positions at 1e-5 for topology inspection,
reject collapsed edges, require each undirected edge twice with opposite winding,
and assert connected-surface counts. This verifies closed edge topology, not the
stronger condition that every vertex neighborhood is manifold. The centered
subtraction avoids using a tangential contact as an ambiguous component-count oracle.
