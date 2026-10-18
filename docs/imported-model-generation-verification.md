# Embedded-model geometry verification

Tracked by core #41. Final local validation passes; hosted review/CI remain required.
This stage does not establish full M4 or application import support.

The first end-to-end C API test failed with the existing imported-GLB rejection.
The corrected reader binds a self-contained, closed tetrahedron from an actual
`.ds3` archive to `ImportedModel` generation. Four output triangles remain finite,
and changing `deformWidth` from 1 to 2 doubles the generated X extent.

The contract exercises:

- Required missing/malformed assets, original GLB diagnostics, invalid UUID syntax,
  brace aliases, case preservation and ambiguous alias rejection.
- Opaque unreferenced assets and known-role retained metadata. A failing regression
  reproduced an unknown imported role falling back to procedural geometry; it now
  returns an explicit unsupported-target diagnostic.
- Unchanged source archive bytes, independent mesh/indexed/sourced owners, source
  corner alignment, input overwrite and later load/session destruction lifetime.
- Unapplied material warnings, coexisting image warnings, and explicit UV/bake
  rejection. The encoded image marker is not a pixel-decoding test.
- Disabled, tiny and retained cache budgets; actual component reuse; changed GLB
  bytes under the same UUID matching fresh generation; required-error recovery.
- A real multi-model archive exceeding the 64 MiB decoded-owner quota before
  generation. No production test/failure switches or reduced quota are used.

Final local results:

- Full Debug build and **36/36 CTest entries passed**, 42.13 seconds.
- An ASan/UBSan build of the complete linked engine passes the expanded imported-model
  contract, 3.10 seconds, with UBSan halt-on-error enabled. Other local instrumented
  contracts were not rerun; hosted sanitizer CI still runs the full suite.
- SwiftPM source product builds; strict Swift/C++ formatting and Qt boundary scan pass.
- Resolver-only LLVM coverage: **83/83 lines and 5/5 functions (100%)**,
  **71/82 branches (86.59%)**. This is not whole-reader/engine or 100% domain coverage.
- Shared asset references generate both distinct parts; encoded-image accounting and
  coexisting compatibility warnings are exercised without claiming pixel decoding.

Evidence logs are retained under `/private/tmp/dust3d-imported-model-*`.
The independent direct-generator bent-spine probe and SHA256 manifest are in
`/private/tmp/dust3d-imported-spine-probe`, recorded in issue #41. That probe
bypasses native archive loading and is not end-to-end app acceptance.

Reproduce with the README's CMake/CTest gate. For the relevant instrumented path,
configure a separate build with `DUST3D_ENABLE_SANITIZERS=ON`, build
`dust3d_imported_model_tests`, and run `ctest -R contract_imported_models` with
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. Hosted Linux sanitizer CI also
runs the complete suite with leak detection; no local Linux/32-bit result is claimed.

Next acceptance includes imported materials/pixel decoding, aggregate transferred
asset ownership, C/Swift integration and exact Apple pin, native open/edit/undo/save,
Quick Look/thumbnail rendering, export re-import and broader upstream comparisons.
