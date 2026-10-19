# CPU color texture sampler verification

Scope: core issue #52, ADR 0021. This is the sampling foundation for imported
baking, not a completed imported atlas or export path.

## Local evidence

The new contract initially failed to compile because the sampler header did not
exist. After implementation, all 41 CTest contracts passed in Debug and separately
with ASan/UBSan. The macOS sanitizer run uses `ASAN_OPTIONS=detect_leaks=0` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. An initial leak-enabled run
aborted because this local runtime does not support leak detection. Linux CI keeps
leak detection enabled; that hosted result remains a separate merge gate.

SwiftPM built the static product using an isolated scratch directory. Manifest
lint, all native/test C++ format checks, explicit format checks for the new UV
files, diff whitespace checks and the source Qt boundary scan passed. Inherited
third-party compiler warnings and the existing native-build-system deprecation
warning remain; this is not a warning-free inherited build claim.

Clang instrumentation for `color_texture_sampler.cc` measures 173/173 executable
lines, 16/16 functions, 177/178 regions and 140/144 branches. This is scoped to the
new implementation, not the whole engine. Tests do not fabricate unreachable
branches to increase the number.

The numerical contract verifies padded-row copy and owner lifetime after source
release; lifetime beyond a caller's cancellation flag; linear RGB interpolation
and straight alpha including hidden RGB; both-axis wraps and bilinear seams;
nearest/linear magnification and all six minification modes; mip rounding and
finite large LOD/UV handling; odd and non-square mip chains; exact payload quotas;
invalid enums, dimensions, tiny readable spans, overflow metadata and nonfinite
coordinates; and pre-cancelled construction. All fixtures are tiny synthetic MIT
arrays in test source. No visual baseline, UI, dependency or C ABI changes apply.

## Reproduction

Use the README CMake configure/build/CTest commands, then repeat with
`-DDUST3D_ENABLE_SANITIZERS=ON`. Run `swift build --build-system native -j 4`,
`xcrun swift-format lint --strict Package.swift`, and root-config clang-format
checks on the new files plus native/test files. On platforms supporting it,
retain `ASAN_OPTIONS=detect_leaks=1` as in CI.

## Remaining acceptance

Authored source UVs still need a separate path from packed UVs. Atlas rasterization,
material/vertex factors, explicit footprint/LOD, alpha/scalar policy, owned native
bake publication, Apple adoption and textured export/re-import remain outstanding.
The existing procedural baker keeps its inherited color policy. Neither core #41
nor Apple #143 is closed by this foundation.
