# ADR 0209: Final grounded rig rest transforms

Status: Private finalized rig output, core #325 / Apple M5 epic #144.

FinalRigGeneration now owns one rest-world/inverse-bind pair per final joint in
exact source order, including generated eyelids. Reuse the inherited numerical
computeBoneWorldTransforms/computeBoneInverseBindMatrices methods over validated
post-grounding/post-eyelid data, rather than inventing a local-frame convention.
Matrices are column-major double arrays acting on column vectors; translation is
at12..14. The world origin is the absolute bone head and +Z points down its axis.
The inherited parent inverse/local/recomposition yields absolute rest frames.
Zero-length actual bones retain valid translation-only frames.

Hierarchy/data validation precedes these numerical methods. They use only standard
containers, Matrix4x4, Quaternion and Vector3; no pose/controller/animation, mesh,
cache materializer, writer or Qt dependency is invoked. Numerical calls remain
serialized. Cancellation is checked before/between methods and per joint. The
inherited inverse routine silently returns identity for singular matrices, so
validate generated world matrices as unit right-handed rigid frames before
inversion, validate inverse frames, require complete map cardinality/exact names,
and verify both products approximate identity within absolute1e-6 per component.
No singular/fault injection or malformed native-runtime probe is used.

Precharge two16-double arrays per joint (256bytes, at most65536 for256joints)
before allocation, within the existing aggregate Object capacities +final-data
logical quota. Map/vector headers, allocator bookkeeping and numerical work maps
are workspace/overhead under the existing contract, not RSS guarantees. Output
remains local until every validation/cancellation check succeeds.

Tests verify world head translation, +Z orientation, world*inverse identity for
ordinary Body and generated eyelid joints; an unused zero-axis child must retain
a translation-only identity rotation. Existing geometry/weight/parent/UV/input
contracts continue. Normal scoped numerical runtime passes. This does not establish
pose deformation, animation, export integration, C ABI/Apple adoption, broad corpus
or physical visual/HIG acceptance. Production core pin and quarantines unchanged.

Validation: missing-restTransforms member RED preceded implementation. Fresh normal,
ASan/UBSan and the anchored final_rig_generation_contracts CTest pass; real CMake
and SwiftPM Dust3DEngine builds pass. A final exact aggregate quota succeeds and
one byte less rejects with a typed limit failure, under normal and sanitizers.
Independent source review found no concrete correctness issue. No full inherited
coverage or physical acceptance claim follows from these scoped results.
