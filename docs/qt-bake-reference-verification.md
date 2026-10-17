# Offscreen Qt bake reference verification

Core issue #35; ADR 0013. Optional standalone tool; no native target or default
CI gains Qt. Local execution: macOS arm64, Qt compile/runtime 6.12.0 from the
standard development-only Homebrew Qt Base package.

## Failing-first evidence

Initial standalone configuration fails on the missing probe source. A later
startup regression exposes Qt consuming `-platform offscreen` before CLI argument
validation: the probe incorrectly succeeds and runs the solid bake. The test
expects rejection and fails. GUI startup now receives only the executable name;
original extra arguments reject before Qt can consume them. Unknown scenarios
also reject. Source-directory and sanitizer-mode mismatch configurations fail
with their specific diagnostics. The optional build refreshes matching native
archives before linking, avoiding stale-code numerical comparisons.

## Measured observations

| Probe | Actual observed result |
| --- | --- |
| Solid opaque | RGBA [51,102,204,255], exact source channels |
| Partial alpha | Source [102,204,51,128] becomes [102,203,52,128], premultiplied byte rounding |
| Asymmetric images | One actually flipped chart; all twelve source pixel centers preserved, including UV-axis transposition; solid color does not modulate images |
| Seam | Large-side [229,0,25,255], small-side [25,0,229,255] |
| Material | Metallic .5 →128, roughness .25 →64; emission strength 2 clips to 1 and yields [255,128,0,255] |
| Procedural | Spots red range 0–255; 90/256 sampled fold normals nonflat; other chart flat; four finite resolved tangents |
| Transparent green | Requested [0,255,0,0] becomes [0,0,0,0]; alpha stays zero, overwrite flag false |

The transparent case did **not** reproduce the initially suspected sentinel-based
alpha overwrite. It records an observation rather than asserting that any future
transparency limitation is correct behavior. Separate coverage masks remain a
portable API design choice, not a proven bug fix based on this run.

Original-object corner-UV comparison with the preserved Qt orchestration:

| Fixture | Triangles | Qt zero-UV records | Maximum portable absolute UV error |
| --- | ---: | ---: | ---: |
| Capsule | 80 | 0 | 0 |
| Sword | 320 | 0 | 0 |
| RoundedCrossing | 434 | 128 | 0 |

Finite/triangle-alignment checks precede comparisons; the acceptance tolerance is
1e-10. Qt has no known marker, so zero records are interpreted only for this
image-free corpus. This establishes a narrow numerical UV comparison, not full
file/generator/render equivalence. The 128 missing mappings remain real and
explicit; no root default or interpolated coordinates are invented.

## Final local gates

- Optional reference Debug build/CTest: 10/10 (32.99 seconds).
- Optional reference ASan/UBSan build/CTest: 10/10 (138.03 seconds), halt-on-UB.
- Default native Debug build/CTest: 32/32 (23.31 seconds).
- Default native ASan/UBSan build/CTest: 32/32 (69.34 seconds), halt-on-UB.
- SwiftPM source build and strict Swift 6/warnings-as-errors session/source/UV
  consumers pass; UV Capsule 80/1 chart and Sword 320/18 charts.
- New reference/native test formatting, native adapter formatting, manifest lint,
  scoped native Qt-source check, native executable Qt-link check and whitespace
  validation pass. Existing reference Qt `mirrored` API deprecations remain
  visible; source is not mass-migrated or warnings hidden.
- Negative configuration checks verify different source directory and mismatched
  sanitizer options reject. A sparse >256 MiB input rejects before allocation.

Qt's installed shared libraries are uninstrumented. Sanitizers cover the harness,
compiled reference application sources and native archives, not all Qt internals.
The reference is optional and not run by the unchanged hosted default CI; these
local probe results are recorded separately. No new full-core coverage claim is
made for a test driver or inherited application code.

## Remaining acceptance

No portable texture pixel implementation, decoded user-image/profile handling,
seam-aware indexed renderer/PBR/transparency, complete mapped corpus or native
M3/M4 acceptance is delivered here. The corpus uses the existing validated native
reader/generator, isolating UV orchestration; it does not independently validate
Qt archive loading or every generator UI setting. Procedural probes cover Spots
and one valid serialized fold, not all pattern/fold families. Fixed reference
image sizes and observed durations are not performance budgets. Diagnostic PNGs
are opt-in temporary outputs, never locally committed snapshot baselines.
