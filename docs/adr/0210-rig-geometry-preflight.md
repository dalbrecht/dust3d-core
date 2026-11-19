# ADR 0210: Geometry preflight before inherited rig calls

Status: Private geometry boundary, core #328 / Apple M5 epic #144.

Final joint/weight copying validated positions after actual rig generation. Add
an earlier read-only geometry preflight before Object copying and inherited
binding/eyelid methods. Eyelid generation indexes Object vertices through faces;
those indices and triangle shape must be checked before that code executes.
PositionKey already guards quantization; reuse its constructor instead of
reimplementing a platform-long range formula or assuming64-bit long.

Validate nonempty positions/triangles,1M maximum original vertices,5M maximum
triangles, finite coordinates bounded by1M and the guarded PositionKey range,
exactly three indices per face and indices below original vertex count. Counts
are admitted before traversal. Configurable lower vertex/triangle quotas permit
ordinary small limit tests. Duplicate/degenerate triangles remain admitted:
render Float32 collapse filtering remains a separate existing step. No repair,
quantization storage or source remapping is performed. Inputs remain immutable.

The helper uses only maximumVertices and cancellation from RigFinalDataOptions;
triangle count has its own1..5M parameter. Invalid data/count configuration,
quota/range and cancellation remain typed RigFinalDataError failures. The original
PositionQuantizationError message survives range translation. Cooperative checks
run before/after and per1024 vertices/faces. Geometry validation performs no rig
method, pose/animation, cache materializer, writer or Qt call. Error messages can
allocate normally; no allocator-failure or exhaustion probe is introduced.

Tiny malformed field fixtures run only this validator, never unsafe inherited
paths: infinity/NaN/out-of-bound coordinates, bad index, nontriangle, empty mesh,
lower quotas and precancel. Ordinary geometry, duplicate/degenerate faces and
the coordinate/long-range boundary are also covered. FinalRigGeneration calls
the helper before copying/generating, while existing actual-rig fixtures retain
ordinary geometry. Snapshot provenance/numeric graph validation, source maps,
work/cancellation bounds, UV materialization, C ABI/Swift adoption and full Qt/HIG
acceptance remain separate requirements. Production pin/quarantines unchanged.
