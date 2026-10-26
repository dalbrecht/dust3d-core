# Contract fixtures

These MIT project fixtures come from the standalone viewer at commit
`4f88ebf8`. `Capsule.ds3` was produced with the repository's independent Python
archive writer; `Sword.ds3` was generated from
`tools/agent_modeling/examples/sword.json`.

Expected source semantics: Capsule has 3 nodes and 1 part; Sword has 18 nodes and
5 parts. Tests check finite geometry, unit normals, retained colors and ownership;
they deliberately do not assert incidental vertex ordering or exact triangulation.

The archive files contain byte offsets and a CRLF header. Do not text-normalize
them. Future fixtures must include source/license, expected behavior, tolerance and
upstream revision metadata. Keep minimized crash reproducers as regressions.

`RoundedCrossing.ds3` is a synthetic MIT fixture independently encoded with Python
for core issue #3 (Codex, 2026-10-16). It has five nodes, two parts and three edges,
with both parts subdivided and rounded, default interpolation and Normal combine.
The source revision is `685b68fb45405dabb4bb6f72c5b0e2598d4b9fdf` (engine sources
unchanged at `e9edb9da5d99251d870facb7810ef4a93b7d33b5`). Expected behavior is a
complete finite colored mesh with unit normals within the existing 0.001 squared-
length tolerance; no incidental triangulation baseline is prescribed. The original
core returns generation failure with repeated-halfedge/polygon-ring diagnostics.

The related synthetic MIT fixtures use the same source revision and provenance:

- `RoundedCrossingTranslated.ds3` translates origin and nodes by 0.123456 on all axes; the union remains one connected surface.
- `RoundedCrossingHard.ds3` enables hard joins on the second part; the union remains one connected surface.
- `RoundedCrossingDiff.ds3` centers the crossing bar at Y 0.5 and uses Inversion, unambiguously cutting the vertical part into two surfaces.
- `SeparatedNearWeld.ds3` uses two single-node parts of radius 0.1 with centers separated by 0.20004; the 4e-5 gap must retain two surfaces.

The new contracts weld exported float positions at 1e-5 for topology inspection,
reject collapsed edges, require each undirected edge twice with opposite winding,
and assert connected-surface counts. This verifies closed edge topology, not the
stronger condition that every vertex neighborhood is manifold. The centered
subtraction avoids using a tangential contact as an ambiguous component-count oracle.

`tests/core/numeric_validation_tests.cc` independently constructs synthetic MIT
archives in memory for core issue #5 (2026-10-17), based on maintained revision
382fe0be977170812577ab400873c10fc6f575e5. Two nodes, one part and a colored
component exercise canvas/node/part/component numeric attributes. Dot and comma
forms must generate identical counts, positions, normals and colors, with finite
unit normals (squared-length tolerance 0.001). Malformed suffixes, duplicate
separators, nonfinite values, underflow, excessive magnitude and negative comma-
decimal radii must reject with a typed diagnostic. These compare equivalent
numeric spellings; they do not prescribe a general triangulation baseline.

`ImportedMaterial.png` and `ImportedMaterial.jpg` are synthetic MIT 2x2 image
fixtures for core #46 (Codex, 2026-10-18; base core 030074d). The PNG was independently
encoded for the earlier GLB/ImageIO development probe, with top-to-bottom RGBA
pixels `[255,0,0,128]`, `[0,255,0,255]`, `[0,0,255,255]`, `[255,255,0,64]`.
Standalone ImageIO inspection verified its dimensions and raw samples. The JPEG
was encoded from that source using macOS `sips -s format jpeg`; its dimensions are
2x2, and its lossy pixel values/removed alpha are not a lossless RGBA oracle.

SHA256:
- PNG: `16b2ab21aa4c82b1cf5d68309350062d9f3611d85cae9078b7bbc18604a4064a`
- JPEG: `4b3593d330793b34e5c66636c1cbc0f8c8572b2fc2a40f0b4aa5bbcdac192920`

The C ownership contract builds independent GLB/`.ds3` wrappers at runtime and
requires exact encoded bytes/material metadata, not rendered pixels. No native
image decoding, Qt render baseline or screenshot parity follows from that test.

ImportedSurface.ds3 is a synthetic MIT fixture for core issue #50, derived from
Apple PR #154's independently generated ImportedPreview.ds3. The original closed
four-face GLB, two-node spine, UVs and embedded PNG are retained. A Python binary
writer appends normalized unsigned-byte COLOR_0 values: red/128 alpha, green/255,
blue/64 and yellow/192. Each source vertex occurs in three triangles. The separate
GLB base-color factor is (0.25, 0.5, 0.75, 0.5); source RGBA must not absorb that
factor or the ordinary preview's forced opacity. Archive offsets/chunk lengths are
recomputed. This is an encoded input fixture, not a rendered snapshot baseline.
SHA256: 78d9e6a94cceccc4b5814692d25e8ca97ca01968c14927880a5e6a5837f715d6.

`worker_uv_visual_tests.cc` constructs a small independent synthetic MIT archive
in memory for core #164 (2026-10-25; base `dc3c0680`). It uses the existing
single-node schema with original opaque component key, explicit alpha-first green
color and a nonzero hex image UUID. The request supplies 48x96 resolved dimensions
without pixels. The same contract reuses licensed Sword and synthetic
RoundedCrossing above for gradient and unmapped-Boolean UV transport. It adds no
binary fixture or rendered snapshot baseline. See
`docs/worker-uv-visual-verification.md` for independent assertions and limits.

`ProceduralImage.ds3` is a small MIT positive metadata fixture for core #245.
It retains Capsule.ds3 geometry/parts/component identity, adds only the existing
component colorImageId `{eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee}`, and embeds the
existing synthetic ImportedMaterial.png unchanged under its matching images key.
Python ElementTree serializes the model; an independent byte writer recomputes
archive offsets and lengths. No native writer, fault case or image decoder is
used to create it. Metadata extraction asserts exact encoded PNG ownership; this
is not rendered pixel evidence. SHA256: 93f13ca989621318a39dae7f87131c1d4d554526fa29b64eac01217134a568f5.
