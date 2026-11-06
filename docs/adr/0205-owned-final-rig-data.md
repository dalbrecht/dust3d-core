# ADR 0205: Owned final joint and vertex-weight values

Status: Private data boundary, core320 / Apple M5 epic144. No generated-rig API claim.

The reference RigGeneratorWorker generates positions, applies bindings/grounding,
then optionally adds eyelids before publishing the final rig and Object. The
Object contains four vertexBone channels, including wrap blends. Template-only
geometry and two-weight exports cannot replace that supported pipeline.

Add copyFinalRigData over already finalized RigStructure/Object values. It copies
exact byte-distinct source-order names, resolved parent indices, Float32 head/tail,
radius/closing values and all four positive influence slots. No pointer into the
caller survives. Parents must form a resolvable forest. Zero-length actual joints
are accepted, independently of the stricter template visualization input contract.
Empty channels/zero-weight slots mean unused; unbound vertices have four no-joint
sentinels and zero weights. Positive weights resolve to known joints, are finite
and nonnegative, and sum to1 within1e-4. Values are not renormalized, sorted,
merged or assigned fabricated Root weights.

Hard ceilings are256 joints/1024-byte non-NUL UTF8 names/1M vertices; coordinates
and nonnegative radii are finite within1M, closing values finite. All populated
source channels match source vertex count. Preflight logical retained bytes charge
native joint/weight records and NUL-terminated name bytes before result allocation.
Source storage, map workspace and allocator overhead are excluded. Names/control
validation reuses only the allocation-free UTF8 primitive, with no worker runtime.
Typed invalid-data/limit/cancel failures leave prior results and caller data alone.
The atomic cancellation flag is borrowed for this call and observed during copying.
Allocation exceptions remain a future C boundary's responsibility.

Fresh tests construct ordinary value fixtures and link only the new helper/test
with standard/header-only data types. No RigGenerator method, mesh generation,
Object cache materializer, PoseGenerator, Qt, renderer or writer is called. Tests
cover source lifetimes, exact Unicode, forward parents/cycles, four weights,
unbound/sparse slots, malformed values, quotas and cooperative cancellation.
Source guards precede scoped normal/sanitizer tests. No failure injection, signal,
resource exhaustion, rejected broad CTest/catalog inventory or old199 replay.

This does not establish semantic assignment correctness, final grounding/eyelid
behavior, rest matrices, C ABI, actual generated geometry, Apple UI or Qt/HIG
acceptance. The actual serialized generation pipeline and opaque owner/copies
remain required. Retain the source snapshot/final Object coherently in that
future owner so these per-vertex bindings cannot be attached to another mesh.
Production Apple pin and quarantines remain unchanged; no core publication yet.

Local validation: missing-header RED preceded implementation. The new anchored
CTest contract and fresh ASan/UBSan pass. The actual CMake engine and SwiftPM
Dust3DEngine target compile; SwiftPM reports existing uuid.h precision warnings.
Measured helper coverage is100% lines/regions/functions and96.88% branches. No
inherited coverage or generator/pose acceptance is inferred from those results.
