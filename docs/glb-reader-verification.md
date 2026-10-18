# Internal GLB decoder verification

Tracked by [core #41](https://github.com/dalbrecht/dust3d-core/issues/41).
Local validation passes; automated review and hosted CI are required before merge.
Native `.ds3` APIs still reject imported models. No end-to-end import, scene,
spine-deformation, texture rendering or export parity is claimed by this stage.

## Contract evidence

Self-contained C++ fixtures construct actual little-endian GLB chunks and binary
accessors; expected geometry and metadata are independent numeric assertions.
The new contract is one of 35 CTest entries and contains these case families:

- Positions, generated/supplied normals, winding, interleaved vertex stride,
  UInt8/16/32 indices, float RGB and normalized UInt8/16 RGBA.
- UV identity, selected primitive material, metallic/roughness, alpha mode/cutoff,
  double-sided state, emissive factor and sampler defaults/filter/wrap enums.
- Independent geometry, UV, color and encoded-image ownership after input overwrite.
  The encoded-image marker is opaque test data, not proof of successful PNG decoding.
- Missing vertex colors remain absent so the inherited generator retains part
  colors. Mixed colored/uncolored primitives receive aligned neutral defaults.
- Truncated/duplicate/missing chunks, invalid schema/types/counts/references,
  duplicate JSON keys, nonfinite/out-of-range coordinates, accessor overflow and
  component/vertex alignment, illegal index stride and unsupported versions.
- Scene references, transforms and explicit unsupported representations, missing
  UVs, duplicate position-key UV triangles, degenerate/canceling topology,
  external resources and unsupported image formats.
- Input, JSON bytes/events/depth, aggregate vertex/triangle and encoded-image limits.

TDD reproduced missing decoder support, invalid scene acceptance, missing material
metadata, alignment/index-stride acceptance, newer minimum-version acceptance,
incorrect aggregate-limit status and synthesized colors overriding part colors.
A fixture initializer syntax error temporarily caused an unguarded CTest command
to run an older executable. That result is excluded; the final evidence below
comes from successful fail-fast build/test sequences.

## Final local results

- Complete Debug CMake build and **35/35 CTest entries passed**, 37.84 seconds.
- Production `GlbReader.cc`, inherited PositionKey and the expanded contract built
  and passed together under **ASan/UBSan**, with UBSan halt-on-error enabled.
  This focused instrumented executable does not establish whole-engine coverage.
- SwiftPM source product built; strict Package.swift formatting and all native/test
  C++ formatting checks passed. The native source boundary scan found no Qt types.
- The preserved Qt reader compiled through its forwarding header and read the
  baseline triangle in an isolated development probe. Native products acquire no Qt.
- The relocated JSON header and MIT license match their original bytes exactly.
  The inherited header still emits two deprecated literal-operator warnings;
  no vendor formatting or warning suppression was added.
- LLVM source coverage for **GlbReader.cc only**: **487/488 lines (99.80%)**,
  **397/432 branches (91.90%)**, and **24/24 functions**. The uncovered executable
  line is the integer-width guard for narrower hosts; the local run is 64-bit.
  No 32-bit, 100% domain, vendor or inherited-engine coverage claim is made.

Logs are retained under `/private/tmp/dust3d-glb-publish-*`, including CTest,
sanitizer/coverage builds and runs, SwiftPM, and the LLVM coverage report.
The reference forwarding probe is in `/private/tmp/dust3d-glb-forwarding-qa`.

## Reproduction and remaining integration

Run the README's full CMake/CTest gate and SwiftPM build. Focused instrumentation
can compile `native/Engine/GlbReader.cc`, `dust3d/base/position_key.cc` and
`tests/core/glb_reader_tests.cc` together with C++17, root/native/third-party/Eigen
include paths and `-fsanitize=address,undefined -fno-omit-frame-pointer`.
Use a separate build for LLVM `-fprofile-instr-generate -fcoverage-mapping`.

Document asset binding, actual spine deformation/part transforms, asset-aware
cache invalidation and quotas, decoded-image integration, C/Swift ownership,
reviewed Apple pinning and rendered/export compatibility remain stages of #41.
Broader scene/material support and fuzzing remain required for the full M4/M8 plan.
