# ADR 0204: Original imported FBX geometry

Status: Accepted locally. Production dependency pin unchanged.

Qt exports original double vertices and triangle topology, with available corner
normals and V-flipped UVs. Its material route embeds PNG color, normal, metallic,
roughness and AO maps; optional rig/animation paths are outside this work.

The former native bridge omitted an ImportedModels owner at document reading
and generation, unlike OBJ after ADR0203. Geometry-only support now owns
imports across both calls and preserves default import limits, warnings,
cancellation and output-owner quotas. Imported BAKED remains explicitly
unsupported; no silent geometry downgrade. No C ABI or app pin change.

The SDK/app currently select GEOMETRY only. Core BAKED already encodes procedural
atlas materials; this proposal does not adopt it or claim textured parity.
Faithful imported/mixed textured FBX remains required: an original-triangle-aligned
source atlas/material contract must precede encoding. Float-filtered source-baked
preview triangles cannot substitute for original-double topology.

The positive tests use only the exact previously reviewed mixed archive
2336 bytes (e6d6b03f55cc180bae1a249871701dd5161fa3ff3fe5db1e5a5fea1ce078221c)
and maintained Capsule (04095eded18e72a1c373673799fc68a6ec21048235c79086c8b18f450b485866).
A bounded independent FBX7400 consumer compares decoded geometry against direct
MeshGenerator Object output, not a second C API copy or encoder helpers. Retained
output lifetime and explicit imported BAKED rejection are separate assertions.
No rig, animation, external consumer process, catalog or historical writer fixture.

## Validation and limits

The fresh Debug baseline produced the expected mixed-import typed unsupported
RED, while Capsule and explicit imported BAKED rejection passed. After the narrow
owner change, the exact three cases passed in Debug and in a separate fresh
ASan/UBSan build. Quarantine guards and exact three-name registration assertions
preceded each run; reviewed source and input hashes remained unchanged.

The independent FBX7400 consumer matched direct generator original doubles,
triangle order/winding and available corner streams. Capsule retained 42 vertices,
80 triangles and 9,706 encoded bytes; mixed retained both actual source kinds,
13 vertices, 17 triangles and 3,718 bytes. Output survived input/reference release,
a subsequent export and native-owner destruction after caller copy. Imported
BAKED remained explicitly unsupported before GLB decoding.

These are three-case scoped contracts, not broad coverage or full Qt parity.
No explicit runtime positive UV-stream presence was logged; equality for optional
streams is not a claim of exercised V-flip coverage. ASan used detect_leaks=0;
there is no leak claim. No independent external FBX application was invoked.
Textured imported/mixed export, its original-face source atlas, rig and animation
remain outside this local checkpoint. The Apple production pin is unchanged.
