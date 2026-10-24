# ADR 0084: Own static FBX source-face selections

Status: Delivery validated locally; review and full M4/M7 acceptance pending (#99).

Static per-material export needs a geometry subset with explicit source-face
correspondence and replacement atlas UVs. Provide a pure move-only owner rather
than recover the archived C bridge, whose owners depend on quarantined rig and
animation paths. No C ABI, generation hook, production pin or quarantine changes.

Retain all shared double-precision positions and vertex indices without
compaction; copy only selected triangle rows, optional corner normals/UVs and
source-relative face indices. Explicit per-selected-face UVs replace the source
UV stream. Selected indices must be nonempty, unique, increasing and in range.
Validate selected rows, matching stream counts and finite bounded coordinates
before copying. Returned geometry outlives its source and override arrays.
Colors, tangents, rig, components and document metadata are not copied: this is
an export geometry subset, not a lossless Object or document clone.

Count-only planning admits at most1M vertices and5M source triangles under a
64MiB default/256MiB maximum logical payload quota. Include temporary setter
copies of corner arrays, then check actual retained and temporary capacities.
Allocator bookkeeping, source storage and process RSS are outside this budget.
The retained source-face map is relative to this input Object; generation identity
and stale-result fencing belong to the later integration boundary.

## Initial local verification and limits

The missing API and selected-face verifier began red. Fresh Debug CMake build
and all74 CTest contracts pass (12.94s final run). New test sources were inspected
and the source quarantine guard passes. Strict C++17 warning and fresh scoped
ASan/UBSan builds pass. SwiftPM builds successfully (0.98s); inherited UUID
narrowing warnings remain. Eighteen geometry/material verifier tests pass in
normal and optimized Python. No additional dependencies were installed.

Scoped implementation coverage uses one instrumented implementation object and
an uninstrumented test driver:105/106 lines (99.06%),111/112 regions and95/96
branch outcomes. The actual container-capacity rejection remains unforced; no
allocation-failure or large-allocation probes were introduced. This is not whole
core coverage or enforcement of a process-memory bound.

Fresh known-positive selected-face output was re-imported with the installed
Assimp CLI. The optimization-safe verifier confirms one face, all imported
positions/normals, replacement UVs, winding and both identity transforms.
Negative verifier cases use small synthetic XML values, not native decoder
probes. Reproduce with a fresh build and an empty caller-owned fixture directory:

```sh
"$BUILD_DIR/dust3d_fbx_geometry_selection_tests" "$FIXTURE_DIR/selection.fbx"
assimp dump "$FIXTURE_DIR/selection.fbx" "$FIXTURE_DIR/selection.assxml"
python3 -B -O tests/core/verify_fbx_mesh_scene_import.py "$FIXTURE_DIR/selection.assxml" --selected-face
```

Static source-bake orchestration must retain original double geometry rather
than reconstruct it from float render buffers. Multiple-material grouping,
C ABI ownership, atomic output, native UI/CLI integration and broader importer
acceptance remain required. Rig/animation and the GLB path remain quarantined.

## Delivery revalidation

Prepared on tested atlas-material delivery `7ab6c4e9`, retaining the reviewed
worker/writer contracts and tested FBX prerequisites. The only new native contract
is the audited selection test. Register the geometry verifier under optimized
Python in CTest as well: the 11 geometry and seven material value cases now run
under both normal and optimized Python. These are small synthetic verifier inputs;
no negative fixture is supplied to a native decoder or importer.

The fresh ordinary CMake build and 81/81 CTest contracts passed (17.50 seconds).
Fresh strict-warning C++17 scoped ASan/UBSan selection contracts, SwiftPM (1.39
seconds), manifest lint, formatting, source quarantine and Qt boundary passed.
Earlier 74-case and scoped coverage measurements are historical, not remeasured;
the actual container-capacity guard remains unforced.

The fresh ordinary selection executable generated a 2538-byte known-positive FBX.
Installed Assimp Version 6.0 produced a 1904-byte XML dump, accepted by the explicit
`--selected-face` verifier under Python optimization. Positions, normals,
replacement UVs, winding and both identity frames match. Exact source revision,
binary/fixture hashes and owned outputs are retained with local evidence. No
archived executable, generator/evaluator, fault or exhaustion probe was used.
Prerequisite reviews, source-bake integration, native binding and broader M4/M7
acceptance remain required; production pins and quarantines are unchanged.
