# ADR 0198: Owned indexed-mesh OBJ output

Status: local implementation; tracks core #63.

The additive C API accepts an already owned, immutable indexed mesh and sized
output/cancellation options. It emits classic-locale OBJ positions (`v`), normals
(`vn`), and one-based triangle corners (`f i//i`) in input index order. Float
values round-trip, and full-attribute seam vertices stay separate. Axes, units and
winding are unchanged. Color, UV, materials, rig and animation are not emitted.
This formatter performs no archive read or imported model generation; the
existing original-double polygon OBJ API retains its separate behavior.

The source and cancellation token are borrowed during the call. The resulting
opaque OBJ owner has independent bytes and a copied warning; borrowed output
views expire on owner destruction. Output quota covers retained owner, warning,
and actual text capacity, not source ownership, caller copies, temporary work,
allocator overhead or process RSS. The default is 64 MiB and maximum 256 MiB.
Cancellation is cooperative during validation and each formatting pass; it is
not a hard latency guarantee. No partial owner is published on failure.

Pure synthetic tests independently parse floats, face indices and normals,
exercise seams and source/output lifetimes, and check typed input, logical
quota, and already-cancelled token behavior. A bounded Capsule procedural test
compares emitted records with the source indexed mesh. Imported archive export,
material export, real GPU results and full M7/release acceptance remain separate.
