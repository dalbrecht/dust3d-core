# ADR 0082: Recover bounded geometry-only FBX scenes

Status: Delivery validated locally; review and full M7 acceptance pending (#99).

Recover the archived static mesh builder over the source-audited FBX value
layers. The private checkpoint retains exact source provenance. Encode original
indexed positions and triangle order, complemented final polygon indices,
optional complete corner normals and V-flipped UVs. Retain the reference Y-up,
positive-Z-front and unit metadata. Returned container bytes own their data;
source geometry remains unchanged. No material, image, skin, animation, file
publication, C ownership adapter or Apple production pin is introduced.

Validate document options before geometry, including nonzero quotas too small
for an output owner. A regression with invalid options and an empty object first
returned the wrong input error; it now rejects the options immediately. Validate
triangle arity/indices, complete corner streams, finite positions within +/-4M
and attributes within +/-1M before staging their values.

Separate pure count admission into FbxMeshScenePlan. It permits 1M vertices and
5M triangles and checks aggregate logical position/index/normal/UV staging bytes
against 256 MiB before allocation or array traversal. The builder uses those
admitted scalar counts for reservations and retains actual capacity guards.
Numeric-only tests exercise count ceilings, overflow-sized counts and staging
rejection without allocating large meshes. Source buffers, encoded copies,
allocator overhead and temporary work remain outside this logical budget;
this is not process RSS containment.

The optional importer verifier checks the explicit two-triangle fixture's
positions, normals, UVs, faces and transforms. A regression proved the archived
assert-based verifier falsely accepted changed positions under Python -O.
Replace assertions with explicit exceptions, retain original numeric causes and
test accepted/changed/missing/nonfinite values plus optimized CLI execution.
These tests use small synthetic XML values, never malformed native geometry.
They join CTest when a Python interpreter is available; the C++ build remains
portable without Python, with an explicit skip notice. Only the standard library
is used; no Python package or production dependency is added.

Assimp remains optional already-installed acceptance tooling. Alternatives are
an additional importer SDK or another custom decoder; the existing independent
CLI verifies this controlled export without becoming a product dependency.
Only freshly generated known-positive FBX reaches it. No unsafe GLB, animation,
writer-fixture, allocation-failure, signal or resource-exhaustion probe is used.

Even a successful controlled geometry import does not establish materials,
rigging, animation, arbitrary corpus compatibility or full M7/editor parity.
Existing quarantines and completed-review/all-green-CI merge gating remain.

## Initial local verification

Source quarantine and formatting guards pass. Current-source missing headers,
option-error ordering and optimized-verifier false acceptance were recorded red
before their implementations/fixes. A clean Debug CMake rebuild and all 70 local
CTest contracts pass (32.98 seconds), including the eight Python verifier value
tests. A fresh strict-warning C++17 scoped ASan/UBSan build of the scene, plan and
FBX value layers passes the final scene contracts. The core SwiftPM product builds;
its inherited UUID narrowing warning remains outside this change.

Scoped scene coverage is 151/152 lines (99.34%), 69/70 regions and 61/62 branch
outcomes. The actual staging-capacity guard remains unforced. The pure plan covers
17/17 lines, 24/24 regions and 18/18 branch outcomes using numeric-only cases;
this does not establish whole-core coverage or actual large-document budgets.

A fresh normal test executable generated the 2670-byte owned two-triangle scene.
The installed Assimp CLI (reports Version 6.0) imported it; its 2178-byte XML dump
passes the explicit verifier under Python -O. Every position, normal, UV, face and
both identity matrices match. Shared source indices become six imported corner
vertices, preserving the normal/UV seam. This is one controlled geometry fixture,
not arbitrary meshes, materials, animation, native app export or full M7 parity.
Only this known-positive file reached the importer; no malformed/native fault
probe or archived executable was run.

Reproduce with a fresh build and an empty caller-owned fixture directory:

```sh
"$BUILD_DIR/dust3d_fbx_mesh_scene_tests" "$FIXTURE_DIR/scene.fbx"
assimp dump "$FIXTURE_DIR/scene.fbx" "$FIXTURE_DIR/scene.assxml"
python3 -B -O tests/core/verify_fbx_mesh_scene_import.py "$FIXTURE_DIR/scene.assxml"
```

## Delivery revalidation

Prepared on the separately tested container-delivery stage `75d1b579`, retaining
reviewed worker/writer contracts and the tested FBX prerequisites. Only the scene
contract and eight value-only Python verifier cases are newly registered. Fresh
ordinary CMake build and 76/76 CTest contracts passed (17.04 seconds), along with
fresh strict-warning C++17 scoped ASan/UBSan scene contracts, SwiftPM (1.21
seconds), manifest lint, formatting, source quarantine and Qt-boundary checks.
Earlier 70-case and scoped coverage measurements are historical, not remeasured.

The fresh ordinary executable generated the 2670-byte controlled fixture. The
installed Assimp CLI reports Version 6.0; its 2194-byte XML dump passed the explicit
verifier under Python optimization. Positions, corner normals/UVs, face winding
and both identity transforms match. Binary hashes and the exact tested source
revision are retained with local evidence; no archived executable was used.
Only that freshly generated known-positive FBX reached Assimp. This remains one
controlled geometry fixture, not material, rig, animation or full M7 acceptance.
Prerequisite reviews, production binding and broader corpus acceptance remain
required; all production pins and quarantines remain unchanged.
