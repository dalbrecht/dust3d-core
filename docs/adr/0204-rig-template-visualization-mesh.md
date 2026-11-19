# ADR 0204: Portable owned template visualization geometry

Status: Private extraction and C boundary locally validated, core #319 / Apple M5 #144. Apple adoption remains pending.

Qt BoneManageWidget's template pane calls RigSkeletonMeshGenerator with radius
0.02 and normalization. Its skin pane uses the same geometry with actual generated
rig data and normalization disabled. The active native ABI has no equivalent mesh
owner. Stored template names/assignment controls do not supply that visualization.

Extract the template geometry to private native/Engine/RigTemplateMesh values.
Preserve the source's eight-vertex tapered solids, face winding/order, 32-sample
rings built by RopeMesh(radius0.001,sectionSegments3), and maximum-extent1.5
normalization. RopeMesh interpolates three positions per ring sample; ordinary
bones therefore have296 vertices/588 triangles, including288 ring vertices and576
ring triangles. Keep that geometry rather than replacing it with line segments.

Each vertex retains its source bone index and bone/selectedBone/ring role. Exact
byte lookup distinguishes canonical Unicode spellings. Ring roles stay separate
from selected solid roles; Qt UI colors do not enter portable geometry. Results
own all vectors and are independent of later calls and input lifetimes. No engine
globals or document/rig/pose/animation generation is invoked.

Inputs require at most256 unique nonempty names,1024 bytes per name, finite
coordinates/radii at most1M, positive start radius and nonzero bone length. Valid
tiny axes below1e-6 retain the reference ring skip. All seven licensed template
inputs have nonzero axes. Zero-length malformed input rejects in this initial
template contract; this does not establish future actual-generated-rig admission.
Output vertex/triangle/logical-byte quotas are checked before geometry allocation.
They are not allocator/transient/process-memory guarantees. Invalid input, quotas
and invalid output have distinct RigTemplateMeshError failures; ordinary C++
allocation exceptions remain a future C-boundary mapping responsibility.

Private C++ API only; STL ownership never crosses the active public C ABI. The
same extraction compiles in the CMake engine and SwiftPM source list. The focused
CTest target links only the new math and existing RopeMesh/BaseNormal/Vector3 plus
a test-only licensed XML reader. It does not link generator, writer, GLB or Qt code.
Source guards precede fresh validation. No rejected broad237/catalog335 runtime
gate, allocation-failure, crash, signal or exhaustion probe is authorized here.

Validation includes actual RED missing header, source-derived topology/winding/
normalization/role/ownership/quota cases and all seven original template XML files.
The corpus uses float parsing then exact double promotion, matching Qt BoneNode
fields; the Qt XML loader leaves capsule radius zero. Provenance JSON records
source revision, hashes and license. CMake contract and fresh ASan/UBSan pass;
actual CMake engine and isolated SwiftPM Dust3DEngine compilation also pass.
Measured new-file coverage is97.62% lines,96.83% regions,93.86% branches. Four
missed lines are output-invariant rejection guards; no fault injection was used
to force them. Detailed logs/profiles are retained outside the repository. This is source-derived compatibility evidence, not an executed Qt visual
or full end-to-end comparison. No full inherited coverage is claimed.

Follow with bounded opaque C owner/accessors, checked Swift copies and native
preview lifecycle/rendering before claiming a usable template pane. Skinning,
weights, IK, pose and physical keyboard/VoiceOver remain separate requirements.
Production Apple pin48b0ce2d and all quarantines remain unchanged. The existing core
CI runs unfiltered CTest; publication must respect the current runtime restrictions,
not move a rejected local gate to another execution surface.

### Owned C boundary

The follow-up public API accepts borrowed source-ordered float bone records and
version1 options, then returns an opaque independent owner. Consecutive draw
records form triangles with flat normals, exact source bone indices and roles.
Names are copied as exact non-NUL UTF-8; canonical Unicode spellings remain distinct.
The owner retains no caller input. Accessors borrow immutable views until destroy;
NULL owners and out-of-range name requests return empty results.

Options require exact version/size, reserved zero and explicit positive quotas.
The draw cap applies to expanded triangle vertices. Logical retained output charges
32 bytes per draw record plus each name's bytes and terminating NUL; container and
temporary workspace are excluded. Internal indexed geometry uses the same caps,
then expanded output is checked before publication. Ordinary allocation exceptions
map to a typed limit error; original diagnostics are preserved. No fault injection
is used. Engine calls serialize, with cooperative cancellation checks around math
and copying. Cancellation is not interruptible inside a single helper call.

A pure-C consumer checks actual linkage, ownership and record offsets. Public C++
contracts check independent results, copied names, exact/short quotas, normals and
roles, scalar validation, null accessors and pre-cancelled tokens. Both focused
CTest contracts pass; fresh ASan/UBSan and actual CMake engine compilation pass.
These are template-only tests with EngineSupport/Cancellation and the existing
math dependencies. They do not invoke generated rigs, poses, writers or GLB.
Swift copies and native rendering remain future work; the production pin is unchanged.
