# ADR 0196: Hollow thickness implementation boundary

Status: investigation; no hollow geometry is enabled. Tracks
[Apple #96](https://github.com/dalbrecht/dust3d-swiftui/issues/96).

## Historical evidence

The current document reader preserves `hollowThickness` and validates that its
value is finite, but the current mesh generator does not consume it. The Qt
reference still preserves this metadata and does not establish current geometric
behavior. The original upstream
[implementation](https://github.com/dalbrecht/dust3d-core/commit/9ed4c29b39a805a08eee671c74cba23d329a2556)
used a UI range of 0 to 1. For a nonzero value it deferred end caps, copied each
vertex toward its source node by the specified fraction, reversed the inner
faces, and connected the outer and inner terminal rings. At 1 the inner vertices
coincide with their source nodes; this is a degeneracy that requires an explicit
decision rather than an assumption of valid solid geometry.

An independently run synthetic historical reference used a straight, two-node
tube with a square cross-section. At thickness 0 it produced 8 vertices and 6
faces; at 0.25, 0.5, and 0.99 it produced 16 vertices and 16 faces with no
zero-area faces. At 1 it produced 16 vertices and 16 faces, including four
zero-area faces as the inner vertices collapsed onto their source nodes. Each
indexed edge had two incident faces in all five cases; that does not establish
geometric manifoldness at coincident vertices. Inner positions matched the
historical interpolation for this fixture. The original builder was unchanged;
auxiliary files needed Qt 6 and CGAL compatibility adjustments. This single
fixture does not establish UV behavior, rounded or tapered geometry, imported
models, or out-of-range semantics. The preserved harness, compatibility patch,
and output live in the local `beta-preserved/reference/hollow-196` evidence.

## Current boundary

The older nodemesh builder is not the current `TubeMeshBuilder`. The current
builder interpolates nodes, optionally rounds ends, remeshes caps, retains source
UUIDs and per-corner UVs, and also supports closed circles. Imported-model parts
follow a separate deformation path. Copying the old implementation directly
would leave UVs, rounded tips, closed circles, nonmanifold geometry and imported
parts unspecified. Archive values outside the historical UI range may already
exist and must not be silently clamped or destroyed on save.

The new `tube_mesh_baseline_contracts` exercises a finite two-node solid tube:
valid closed two-manifold edge incidence, finite positions and UVs, aligned
face/UV corners and preserved vertex source IDs. It is a baseline, not evidence
that hollow generation exists or that every tube topology is valid.

## Required next decision and acceptance

The first unresolved decision is the meaning of the stored scalar at the
generation boundary. The historical control admitted the closed interval [0, 1],
but the historical interpolation collapses inner vertices onto the spine at 1.
Current archive admission accepts finite values up to the general numeric bound,
including values outside that interval. We do not yet have evidence whether to
preserve the historical degenerate result, reject 1 and out-of-range values at
generation with a typed unsupported diagnostic, or choose another documented
geometric meaning. Any choice must leave those original archive values intact
when opening and saving. This is a product/compatibility decision, not a choice
that can be inferred safely from the metadata alone.

The second unresolved decision is how the current renderer should map UVs to
inner faces and the two annular rims. The 2019 builder did not supply this
builder's per-corner UV contract. A flipped copy of the outer UVs is plausible,
but without a historical rendered fixture or an explicit texture policy it is a
new visual behavior. Rounded/interpolated terminal rings, circular tubes and
source UUID attribution likewise need independently checked expected geometry.

Before exposing a hollow control or consuming this metadata in generation,
specify supported values and their meaning, including zero and the degenerate
upper bound; decide unsupported-value diagnostics without compromising lossless
document preservation. Specify terminal rim winding, inner UV orientation,
source attribution, circular tubes, interpolation, rounded ends, mirroring,
deformation and boolean interactions. Compare simple straight, tapered and
rounded cases with independently reproduced historical fixtures where available;
verify finite nondegenerate triangles, manifold topology, vertex/UV ownership,
and current solid behavior. Unsupported cases must be surfaced explicitly, not
silently rendered as a complete hollow model. Tests must cover save/reopen and
undo in the Apple app after an independently reviewed core integration.
