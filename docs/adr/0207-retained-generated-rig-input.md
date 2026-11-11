# ADR 0207: Bounded retention of coherent generated rig inputs

Status: Private ownership boundary, core #323 / Apple M5 epic #144.

Qt rig generation takes the generator-processed Snapshot and UV-mapped Object.
Native preview previously destroyed both after flattening triangles. Retain the
processed snapshot and complete Object together through an optional generation
output. Do not reconstruct a snapshot from the original archive or flatten away
source/UV/weight metadata.

GeneratedRigInput owns both unique pointers and exposes const borrowed views.
The factory consumes inputs even on failure. Caller provenance guarantees that
the two inputs came from one generation; this holder does not infer or validate
semantic rig readiness. Object gains a complete dynamic-payload visitor over all
public containers and six private cached containers, regardless of presence flags.
Existing generic visitors include nested UUID strings, immutable source hints,
indexed procedural UVs, topology, source associations and all four weight arrays.

A single conservative budget accounts Snapshot and Object inline objects and
owned capacities before wrapper allocation. Default64MiB, maximum256MiB; allocator,
container-node bookkeeping, wrapper and temporary-work storage are excluded.
Immutable shared source distributions charge per logical owner conservatively.
No artificial allocation failures or resource-exhaustion probes are used.

The optional generateMesh hook takes the processed snapshot and stages the bounded
owner before prepareGeneratedObject can publish any requested outputs. Preparation
borrows from that pending owner; only a noexcept unique_ptr move remains after
successful preparation. Quota/allocation/preparation/cancellation failure therefore
leaves the caller's previous input owner unchanged. Direct borrowed preparation
rejects retention because it cannot transfer coherent ownership. Original-export
purpose also rejects it. Existing callers request no retention and keep their path.

Native UvMetadata stores a separate atlas; it does not materialize Qt-equivalent
UV data into Object. This owner is generator input retention, not finalized rig
or UV-ready output. Subsequent actual rig processing must complete UV/source
materialization as needed, integrate #321 rebinding reset, ground bones/mesh
coherently, distinguish optional eyelid absence, derive rest matrices and join
weights to admitted draw corners through #322 mapping. No C ABI or Apple adoption.

Fresh value tests exercise transfer/address identity, processed snapshot identity,
UV/normals/model/fourth-weight retention, exact aggregate budget, typed missing/
quota failures, all six private cache accounting paths, indexed source/UV records,
scoped component keys and all four weight arrays. They invoke no generation,
pose, writer, Qt, signals or deliberate allocation failure. Pipeline wiring is
compiled, not claimed runtime-validated; production pin and quarantines unchanged.
