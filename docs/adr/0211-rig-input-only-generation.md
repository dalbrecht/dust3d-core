# ADR 0211: Retained rig inputs without discarded preview allocation

Status: Private pipeline mode, core #329 / generated-rig C boundary prerequisite.

Generated rigs consume processed Snapshot/full Object values before preparing
a grounded preview. The existing preview generation path allocates a flattened
buffer that this operation would immediately discard. Add rigInputOnly as an
appended GenerationPurpose value, preserving existing preview/export enum values.

Require the coherent rigInput output and reject render/UV/export hooks, images
and atlas mode. Preserve original source node/part counts and warning in returned
Dust3DMesh metadata, but never reserve/populate preview vertices. Take processed
snapshot and full Object, preflight/stage the bounded unique owner, check final
cancellation, then publish by noexcept move. Other generation purposes retain
existing preparation. Direct borrowed prepareGeneratedObject rejects this mode.
No UV materialization, float collapse filtering, rig generation or exporter runs
inside this retained-only path. Original geometry remains for later finalization.
The existing retained-input quota ceiling is validated before mesh generation.
Raw mesh-generation workspace remains outside retained payload quotas.

The fresh scoped test loads only the existing known-positive procedural Capsule,
with imports=nullptr. It verifies retained nonempty full geometry/processed source,
source counts, zero preview capacity, precancel and invalid/missing owner hooks
without replacing prior output. It does not run existing broad tests that also
invoke exporters. CMake normal and ASan/UBSan engine/test builds pass; real SwiftPM
Dust3DEngine builds. No rig/pose/animation, GLB decode, writer, Qt, signals, faults
or exhaustion probe executes. Source guards precede tests. Independent source
review found no concrete correctness defect. This is not a new app release or
proof of full generation resource/time containment. Production pin unchanged.
