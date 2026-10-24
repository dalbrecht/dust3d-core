# ADR 0083: Recover static atlas materials and embedded PNGs

Status: Delivery validated locally; review and full M4/M7 acceptance pending (#99).

Recover the saved static material extension over the audited FBX scene builder,
preserving the newer pure geometry admission and early document-option checks.
The private checkpoint records exact archived-source provenance. No GLB catalog,
rig, animation, source bake, destination writer, C ABI or production pin changes.

Represent one composed atlas with finite unit-range RGBA/metallic/roughness values
and six optional encoded PNG channels: color, normal, metallic, roughness,
emissive and AO. Inputs must already be validated PNGs from a caller image
boundary; this encoder only checks signatures and copies bytes. Preserve full
binary typed names, deterministic internal filenames, scalar/texture bindings
and a single AllSame material association. This is not a general image decoder
or complete source-material/shader implementation.

A separate FbxMaterialImagePlan admits six numeric lengths, aggregate encoded
bytes at most 64 MiB, minimum signature spans and UV presence before byte access.
The scene separately validates complete UVs. Numeric-only cases cover zero/six
channels, exact and over-budget sums, overflow-sized lengths and missing UVs
without allocating large buffers. Encoded copies, inputs, temporary work and
allocator overhead are not a total RSS budget.

The restored material API began red before implementation. Fixed authored 2x2
RGBA PNGs have distinct first pixels per channel and partial alpha. C++ contracts
check scalar/signature/UV rejection, deterministic ownership, typed NUL names,
embedded bytes and each optional channel. Invalid signature bytes reach only
validation, never an image decoder or independent importer.

The archived Python verifier falsely accepted changed opacity under Python -O;
the regression was recorded red. Reuse explicit validation exceptions and test
changed bindings/opacity/UVs, channel swaps, missing images, CRC changes and
optimized execution with small synthetic XML/PNG values. This is controlled
fixture validation, not malformed native-asset probing. Its standard-library
tests join CTest when Python is present, without a new production dependency.

Use the already installed independent Assimp CLI only with freshly generated
known-positive material output. Additional importer SDKs or a custom test decoder
remain alternatives; no package is downloaded or linked to products. Full source
bake orchestration, multiple-material groups, tangent/normal-map rendering, alpha
compatibility, broader importers, rig/animation and native UI/CLI export remain
required. Scalar/pixel binding checks alone do not establish full parity.

## Initial local verification

Source quarantine and formatting guards pass. Missing material/image-plan APIs
and optimized-verifier false acceptance were recorded red. A clean Debug rebuild
and 72 contracts passed; after adding the full optimized-verifier test variant,
the regenerated final build/gate passes 73/73 CTest contracts (25.12 seconds).
Seven material-verifier value tests run in both normal and optimized Python.
Fresh strict-warning C++17 scoped ASan/UBSan builds pass both material and
geometry-only contracts. Core SwiftPM builds successfully; inherited UUID
narrowing warnings remain outside this change.

Two uninstrumented test drivers use identical instrumented core objects for
clean combined coverage. Shared scene implementation coverage is 281/282 lines
(99.65%), 115/116 regions and 95/96 branch outcomes; the actual staging-capacity
guard remains unforced. Image admission covers 16/16 lines, 15/15 regions and
10/10 branch outcomes using numeric metadata. The existing geometry plan also
remains fully covered. These are scoped measurements, not whole-core coverage
or actual large-image/process-memory acceptance.

All six authored PNGs were checked for structure, CRCs and exact pixels before
native import. A fresh normal helper generated the 10626-byte material fixture;
installed Assimp (reports Version 6.0) produced an 18153-byte XML dump. The explicit
verifier under Python -O confirms scalar factors, six embedded image channels,
their decoded pixels, bindings, material association and UV/geometry data.
Only this known-positive output reached the importer; negative verifier cases
use small synthetic values outside native decoding. This is one atlas fixture,
not arbitrary material, source bake, renderer or complete exporter parity.

Reproduce after a fresh build using an empty caller-owned fixture directory:

```sh
"$BUILD_DIR/dust3d_fbx_material_scene_tests" "$FIXTURE_DIR/material.fbx"
assimp dump "$FIXTURE_DIR/material.fbx" "$FIXTURE_DIR/material.assxml"
python3 -B -O tests/core/verify_fbx_material_scene_import.py "$FIXTURE_DIR/material.assxml"
```

## Delivery revalidation

Prepared on tested geometry-delivery stage `428faec9`, retaining the reviewed
worker/writer contracts and tested FBX prerequisites. Only the material contract
and normal/optimized Python material-verifier runs are newly registered. Fresh
ordinary CMake build and 79/79 CTest contracts passed (16.59 seconds), including
seven verifier cases under each Python mode. Fresh strict-warning C++17 scoped
ASan/UBSan builds passed both material and geometry-only contracts. SwiftPM built
(1.22 seconds); manifest lint, formatting, source quarantine and Qt boundary passed.
Earlier 73-case and coverage measurements above are historical, not remeasured;
the actual staging-capacity guard remains unforced.

Before independent import, all six authored PNGs passed bounded structure, CRC
and exact-pixel validation. Fresh ordinary executables generated a 10626-byte
material fixture and the 2670-byte geometry fixture. Installed Assimp Version 6.0
produced 18170-byte and 2201-byte XML dumps; both explicit verifiers passed under
Python optimization. Material factors, six embedded channels with exact decoded
pixels, bindings, material assignment and UV/geometry data match. Exact source
revision, binary/fixture hashes and owned outputs are retained with local evidence.
Only freshly generated known-positive outputs reached the importer; no invalid
signature or negative verifier fixture entered a native image decoder/importer.
Broader material/renderer, source-bake, production binding and full M4/M7 acceptance
remain pending. All production pins and quarantines remain unchanged.
