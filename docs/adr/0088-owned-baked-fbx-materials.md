# ADR 0088: Encode owned baked FBX materials and compare generated scenes

Status: Local implementation; review and full M4/M7 acceptance pending (#99).

Bridge portable baked RGBA maps to the static atlas material without duplicating
raw pixels. makeFbxBakedMaterial borrows six optional TextureImage views in fixed
color/normal/metallic/roughness/emissive/AO order; color is required. Validate every
image span before compression. Encode each present image as an independent PNG,
leaving missing channels absent. The caller establishes common atlas provenance;
different map resolutions are valid. No input view escapes in the result.

Use neutral white/opaque base and roughness1; metallic1 when a metallic map is
present, otherwise dielectric0. The existing scene builder activates emission
when its map is present. These scalar values avoid multiplying baked map data by
a second arbitrary source factor. They do not prove shader, color-management,
alpha or tangent-space agreement across applications.

The aggregate budget counts retained PNG vector capacities, default/maximum64MiB.
Pass only the remaining capacity allowance to each PNG encoder and reject when
less than its minimum span remains. A later malformed image is rejected before
any compression. Raw source pixels, compressor workspace, root metadata and
later FBX scene buffers are excluded; this is not a total RSS bound. No new
dependency, file writer, C ABI, pin change or quarantined path is introduced.

## Verification

Missing adapter/verifier APIs were recorded red. Source audit/quarantine and
formatting guards pass. Recovery validation: fresh Debug CMake build and87/87 contracts pass (36.00s).
SwiftPM builds in3.32s after the shell sandbox blocked its nested manifest sandbox;
the authorized build succeeds with the required execution permission. Fresh
strict-warning scoped ASan/UBSan instruments the baked-material adapter, PNG
encoder and baked-material driver; the driver passes against fresh ordinary
inherited libraries. Inherited geometry/system zlib are not instrumented by
this scoped run. Formatting, Swift manifest lint and Qt boundary checks pass.
The tested recovery source is2abeed7e764f2dc51053ab8a434deaefa5136b1e;
its source tree, output hashes and logs are retained with the recovery checkpoint.

Adapter contracts cover all six channel identities, ownership/source mutation,
neutral factors, missing optional channels/color, invalid later pixel spans,
per-image quota rejection and aggregate capacity exhaustion. Historical adapter
coverage was24/25 lines (96%),29/30 regions and21/22 branch outcomes; it was
not remeasured during recovery. The defensive
capacity recheck remains unforced because encodePng already guarantees its
allowance. Do not use allocation-failure probes to force it. This is not whole-
core coverage or complete material/normal-map acceptance.

The generation contract now applies controlled metallic0.375, roughness0.625 and
emissive0.5 to the licensed Capsule's in-memory parts, then runs a real64-square
procedural bake and embeds its maps with original double geometry/packed UVs.
The original fixture archive stays unchanged. An optional test-only output
writes known-positive FBX and a separate oracle from original double corners and
raw baked pixels; it is not an atomic production writer or shipping export CLI.

Fresh recovery output is20732bytes. Installed Assimp6.0 imports it into40277bytes of XML.
The independent optimized-Python verifier compares all80 faces/240 corners,
positions/normals/winding, packed UVs (2e-5 text-dump tolerance), both identity
transforms, neutral scalar factors, exact embedded color/metallic/roughness/
emissive pixels, CRCs, complete deflate streams and texture/material/UV bindings.
The270662-byte oracle comes from the pre-encoding source, not from parsing FBX.
Only this known-positive output reaches Assimp; negative verifier regressions
use small synthetic XML/PNG/source values outside native decoding.

Five verifier unit tests run normally and with Python optimization, covering
changed geometry/normals/UVs/winding, changed raw pixels/bindings, missing opacity
and optimized false-acceptance regression. The existing PNG verifier is reused
at a byte boundary, retaining its independent header/CRC/row/pixel checks.

Reproduce with a fresh build and empty controlled output directory:

```sh
"$BUILD_DIR/dust3d_fbx_generation_geometry_tests" tests/core/fixtures "$FIXTURE_DIR"
assimp dump "$FIXTURE_DIR/baked.fbx" "$FIXTURE_DIR/baked.assxml"
python3 -B -O tests/core/verify_fbx_baked_scene_import.py "$FIXTURE_DIR/baked.assxml" "$FIXTURE_DIR/expected.json"
```

## Remaining acceptance

This establishes one procedural generated/baked scene, not arbitrary source-
aware imported geometry or complete exporter parity. Actual baked normal/AO
maps, alpha/double-sided policy grouping, broader independent renderers/Qt,
owned export C ABI, aggregate whole-operation limits, cancellation/progress,
atomic writes and native UI/CLI commands remain required. The production Apple
pin and all GLB/rig/animation/writer/fault quarantines remain unchanged.
