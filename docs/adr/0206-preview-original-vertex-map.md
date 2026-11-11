# ADR 0206: Original vertex identity for prepared preview corners

Status: Private pipeline hook, core #322 / Apple M5 epic #144.

Final rig weights are indexed by the original Object vertex. Native preview
expands triangle corners and omits triangles that collapse at Float32 precision.
Consequently, joining final weights to draw vertices by array position is wrong.

GenerationOutputs.previewVertexIndices optionally receives one original vertex
index per admitted draw corner. Mapping uses the very same admission loop and
triangle/corner order as positions, including shared vertices and repeated
indices. No positions are matched or quantized to recover identity. The mapping
is an independently owned vector, replaced only after all requested preparation
and final cancellation checks succeed. On failure its previous contents survive;
the caller must still discard the partially prepared Dust3DMesh as before.

The existing five-million-triangle gate bounds the staging map to fifteen
million size_t records. No mapping allocation occurs unless requested. This
private hook has no public C ABI or payload-quota claim. Original-export purpose
rejects the hook even when a valid export owner was supplied. The borrowed output
pointer must outlive the synchronous serialized call and must not alias inputs.

Scoped tests prepare tiny ordinary Objects only. They verify shared/reordered
corners, an omitted double-valid Float32-collapsed triangle, coordinate agreement,
unchanged default admission, retained output after a later bad index, cancellation,
all-collapsed rejection, export-purpose rejection and independent source lifetime.
No mesh/rig generator, pose, animation, exporter, Qt or fault probe is executed.
The engine link contains these implementations but the scoped test does not call
them. Generation/grounding, retained coherent snapshot/Object ownership, weights,
rest matrices and Apple adoption remain separate work. Production pin unchanged.
