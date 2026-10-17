# ADR 0001: Boolean intersection loops and vertex welding

Status: Accepted for core issue #3.

A rounded/subdivided crossing of two procedural chains failed to generate because
two intersection loops shared a degree-four vertex. The old walk globally marked
vertices visited, preventing the second loop from closing. Traverse each undirected
edge once instead, splitting closed cycles when a walk revisits a vertex. Validate
symmetric, even-degree, non-self adjacency first; reject invalid boundaries instead
of returning partial polygons. Accumulate loop output only after traversal succeeds.

The resulting mesh also exposed a sliver between an original vertex and a cut
vertex about 2.2e-8 units apart. Original vertices bypassed the cut-vertex weld map,
and near-equal coordinates could straddle truncated PositionKey cells. Map original
and intersection vertices through the same indices. Within this boolean operation,
look in neighboring cells and compare actual coordinate distances at the existing
1e-5 grid resolution. Do not add alias cells for neighboring matches, preventing
transitive growth of the weld tolerance. Remove triangles and cut edges collapsed
by this mapping. The global PositionKey implementation remains unchanged.

This is a bounded change to inherited C++17 boolean processing, independent of Qt,
Swift and the C ABI. Preserve inherited paths/formatting and keep it separate from
Apple adapter features so the fix can be offered upstream when authorized.

Regression acceptance requires complete finite colored meshes, unit normals,
paired oppositely oriented triangle edges at a stated 1e-5 test tolerance, and
expected connected-surface counts. Cover the original touching-loop fixture,
translation, hard joins, a centered difference and separated parts beyond the weld
resolution. These checks do not prove all vertices are manifold or establish full
boolean/modifier parity. Sanitizers and hosted Linux/macOS gates remain required.
