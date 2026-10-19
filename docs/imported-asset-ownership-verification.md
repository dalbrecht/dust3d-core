# Imported material/image ownership verification

Tracked by core #46, following merged geometry PR #43 (master 030074d).
This is a C ownership/metadata boundary; it does not establish pixel decoding,
baking, renderer adoption or full M4 acceptance.

The public C contract first failed to compile because the owning API did not
exist. Its first runtime pass exposed the mesh reader's nonempty-geometry
restriction. A private inspection option now permits empty material sets while
keeping graph/reference validation and legacy geometry-loading behavior intact.

The contract exercises:
- Actual 2x2 PNG/JPEG fixture bytes preserved through GLB and `.ds3` to public C
  views, with scalar RGBA/metallic/roughness/emissive, alpha mode/cutoff, sidedness,
  all sampler fields, and absent/default distinctions.
- Shared model references, uninstanced imported parts, original opaque part and
  component aliases, case-preserving model IDs and ambiguous asset rejection.
- Input preservation/overwrite, subsequent owner loads/destruction and independent
  copies surviving owner destruction without dereferencing expired views.
- Exact measured and insufficient output quotas, hard quota/options validation,
  null/out-of-range accessors, empty/procedural material sets, malformed numeric
  fields/references, missing/malformed/unsupported required assets, and original
  error precedence before output quotas.
- Real late allocation failures in 32 positions of a successful load's allocation
  sequence. The public API returns LIMIT_EXCEEDED and the original bad_alloc
  diagnostic, partial owners unwind, and an older live owner's views remain valid.
- A deliberately invalid encoded-image marker remains opaque storage. ImageIO
  validation is still required; MIME forwarding must not imply successful decoding.
- The pure-C consumer instantiates the public structs and invokes the new API.

Logical output accounting includes retained object/record/string/buffer
capacities; it excludes temporary parser/decoder work and allocator metadata.
See ADR 0018 and the public header for lifetimes and independent quotas.

Final boundary-only LLVM instrumentation measures **151/153 lines (98.69%)**,
**14/14 functions (100%)**, and **68/68 branches (100%)** for
`native/Engine/ImportedAssets.cc`. Only the defensive non-standard-exception
catch-all handler's two lines remain uncovered. The decoder already rejects
zero-length image buffer views, so an unreachable empty-image branch was removed.
No fake production exception/branch is added to claim 100% line coverage.
This is not whole-reader/inherited-engine or 100% domain coverage.

Final local gates:
- Complete Debug build and **39/39 CTest entries pass**, 58.49 seconds.
- Complete linked ASan/UBSan build and **39/39 CTest entries pass**, 177.75 seconds,
  with UBSan halt-on-error. Local macOS results do not replace hosted Linux leak checks.
- SwiftPM source product builds, 41.07 seconds.
- Strict C++/Swift manifest formatting, Qt boundary and whitespace checks pass.
- macOS `leaks --atExit` reports **0 leaks / 0 leaked bytes** for the new
  imported-asset contract executable, including its 32 actual allocation failures.

Build/test/leak logs are under `/private/tmp/dust3d-imported-assets-*`;
instrumentation and JSON/report output are under
`/private/tmp/dust3d-imported-assets-coverage`. Timings are validation evidence,
not performance parity measurements. Fresh exact-head automated review and all
hosted CI checks remain required before merge. The Apple pin is unchanged.
