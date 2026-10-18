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

- Full Debug build and **37/37 CTest entries passed**, 23.46 seconds.
- The ASan/UBSan build passes the direct spine and expanded imported-model
  contracts, 1.99 seconds, with UBSan halt-on-error enabled. Other local instrumented
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


## Review correction: independent zero-width scaling

Automated finding 4308286986 identified `0/0` in inherited spine deformation when
`deformWidth` is zero. Both a direct portable deformer test and an actual imported
`.ds3` load failed before the fix; the latter reported nonfinite generated geometry.
Width and thickness now multiply the radius/source scale independently. This small
core fix preserves the inherited layout and can be extracted for upstream review.

The direct contract covers zero/positive/negative width, zero/positive thickness,
cut rotation, zero radius and a bent spine. The C API regression requires finite
flattened faces with remaining thickness. The full corrected Debug build passes all 37 contracts (24.54 seconds), relevant
ASan/UBSan contracts pass (2.28 seconds), and SwiftPM/formatting/Qt boundary checks
pass. Logs are retained under `/private/tmp/dust3d-core-pr43-zero-width-*`.
The resolver-only coverage measurement above is unchanged source from the initial
head; no whole-engine coverage claim is made. Fresh exact-head review and all
hosted CI checks remain required before merge.


## Review correction: transformed imported normals

Finding 4308327427 identified frame-only normals under rotated/nonuniform vertex
deformation. Both direct expected-vector tests and a public C API perpendicularity
test failed before the fix. Normals now use the inverse transpose of the local
vertex-deformation Jacobian, including longitudinal and radius-gradient terms.
The generator supplies the full source point so tapered cross-sections retain
their shear contribution. The legacy source-Y overload evaluates the centre.

The direct contract covers rotation, inverse nonuniform scales, reflected scales,
thin scales, taper and finite-difference tangents on a bent spine. Singular
transforms return an unavailable normal; the generator retains geometry-derived
normals rather than overriding them with zero or nonfinite data. Actual GLB
fixtures with explicit and generated normals verify perpendicular output under
rotation/scaling; the zero-width archive verifies the singular fallback. Final
corrected Debug build passes all 37 contracts (23.46 seconds); both relevant
ASan/UBSan contracts pass (1.99 seconds). SwiftPM, formatting and Qt boundary
checks pass. Logs are under `/private/tmp/dust3d-core-pr43-normal-*`.
Fresh exact-head automated review and all hosted CI checks remain required.
No whole-engine coverage claim is made.
