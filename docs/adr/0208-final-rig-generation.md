# ADR 0208: Owned actual rig generation and binding

Status: Private core operation, #324 / Apple M5 epic #144.

Retained processed snapshot/Object inputs now feed the inherited actual rig
pipeline. One RigGenerator instance generates actual positions, applies source
bindings and common mesh/bone grounding, then conditionally generates eyelids.
The same instance matters: origin offsets and isolated-node assignments are
state. The reusable #321 reset is integrated before this operation.

Generate over a private Object copy and borrowed immutable processed snapshot.
Return the final Object and copyFinalRigData output together only after complete
success, retaining original vertex identity, all four influence slots, geometry,
UV/cache/source metadata and exact names. Input/template values stay unchanged.
Prevalidate names and resolved acyclic hierarchy before inherited generation.
An empty template/mesh is invalid. Failure causes remain typed; parsing/allocation
exceptions propagate for a future C boundary. Optional eyelid detection returning
false preserves a valid base rig and reports eyelidsGenerated=false.

The operation internally serializes with engineMutex; callers must not already
hold it. Stage cancellation checks run before/between inherited stages and final
publication. There is no claim of cancellation inside inherited graph, nearest
binding or eyelid loops. Final payload quota combines full Object retained
capacity accounting and RigFinalData logical records/names; maximum256MiB.
Input, borrowed snapshot, template/generation/copy workspace and allocator
bookkeeping are excluded. Source Object and structural template are preflighted;
result Object is recounted after bindings/eyelids before final data allocation.

This private operation accepts validated, coherent generator-produced inputs.
Counts/work/mirror depth must be admitted by its caller before invocation; it is
not an untrusted document C API. Existing source-keyed associations remain in
the original frame after grounding; indexed ownership/UV records must be retained
or missing fallback data materialized before downstream use. No UV atlas is
invented or rebuilt here. Original vertex weights require #322 draw mapping for
render use. Rest matrices, actual generated-rig C ABI, Apple UI, large corpus,
broader eyelid/model corpus and physical Qt/HIG acceptance remain separate.

A freshly built tiny fixture uses numeric zero origins, two canonical UUID nodes,
one Model part/assigned Body edge and three mapped vertices/one triangle. Actual
positions are Y1..3 before grounding and Y0..2 afterward, replacing template99..100.
The test verifies shared grounding, source weights, preserved original/template,
UV/topology, optional absent eyelids, precancel, quota limits and invalid hierarchy.
The exact audited closure executes no mesh/pose/animation generator, world/inverse
matrix operation, writer, Qt, signals, deliberate fault or old Apple199 controller.
Source quarantine guards precede scoped execution. Production pin unchanged.

Successful eyelid coverage now also uses two ordinary open annuli (16 vertices,
16 triangles) with four boundary loops. The source detector excludes the two
outer loops by its head-relative radius filter and selects the two symmetric
inner holes. Normal scoped CTest and fresh ASan/UBSan pass. Four generated names,
Head parent indices, finite closing angles, left/right upper/lower vertex binding,
unit weights, unchanged input and retained topology are checked. This synthetic
geometry contract is not visual acceptance or coverage of real-world eyelid models.
No pose, animation, rest transform, writer or Qt runtime is executed by the test.
