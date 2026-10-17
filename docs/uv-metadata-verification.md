# Owned UV C metadata verification

Core issue #33; ADR 0012. C++ atlas prerequisite: reviewed PR #32, merge
0c4966abdc6398a3e64302bce4f3a374b5a54e27.

## Failing-first and acceptance evidence

The initial C contract build fails on missing Dust3DUvMesh/types/functions. The
implemented contract uses public C interfaces plus narrow analytic internal
fixtures, not fault switches or replacement generators.

- Capsule: 80/80 known records, 20,957 logical metadata bytes.
- Sword: 320/320 known records, 18 charts and real gradient descriptors,
  101,177 logical metadata bytes.
- RoundedCrossing: 306/434 known records, 104,412 logical metadata bytes;
  128 unknown records remain explicit rather than fabricating charts.

Exact metadata quota succeeds; one byte less fails descriptively on each model.
Retained owners survive failed/subsequent calls. Copied metadata and independently
indexed geometry survive owner destruction. The real precision fixture generates
near and far components, verifies the original triangles start with the far one,
and drops that prefix at float precision. Published metadata has exactly the
surviving geometry count and references the nearby original component.

Analytic tests exercise opaque original keys, alias ambiguity, conflicting visual
aliases, generated identities, alpha-first mixed-case colors, unsupported source
colors and image metadata without a registry. Public loader tests exercise
resolved images/colors, missing dimensions, default white, malformed/duplicate/
unterminated image descriptors, null handles, missing/incorrect options, quotas,
texture limits and optional diagnostics. Opt-in failures leave ordinary preview
available. The C consumer compiles against the additive header. Strict Swift 6
consumers copy UV/chart/gradient buffers and original strings before owner release,
then verify copied Capsule/Sword data after another independently destroyed load.

## Gates and scoped coverage

Full Debug build/CTest passes 32/32 (25.76 seconds). Full ASan/UBSan
build/CTest passes 32/32 (79.55 seconds), with halt-on-undefined-behavior enabled.
Focused LLVM line coverage for UvMetadata.cc is 197/213 (92.49%); all 25
functions execute, with 91.67% region and 81.88% branch coverage. Uncovered paths
include allocation/unknown exception conversion, atlas error conversion, internal
invalid/generated-identity skips, conflicting image-presence aliases, a fully
identity-exhausted quota and a defensive packed-ID buffer invariant. No production
fault switches or artificial branches were introduced to raise coverage. This is
scoped adapter coverage, not inherited full-core coverage. SwiftPM source build and strict Swift 6/warnings-as-errors
session/source/UV consumers pass (UV Capsule 80 triangles/1 chart, Sword 320/18).
Formatting, source Qt-boundary and whitespace checks pass.

## Limits

No image bytes are decoded, sampled or baked. Color channels remain in their
source encoding; no calibrated/sRGB/linear conversion claim is made. Image
identity output uses engine canonical notation; component keys retain original
spelling. Generated and ambiguous component associations stay explicit.

The owner retains the atlas and C metadata under a logical quota, not a process
memory/time ceiling. Geometry, temporary workspace and allocator/capacity overhead
remain excluded. No full inherited engine coverage, Qt atlas/pixel parity,
complete mapping, exact Boolean ownership, textured indexed sharing, Apple app
adoption or full M4 completion is claimed. No native UI changes require screenshot
validation in this core-only PR.
