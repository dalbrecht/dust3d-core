# Optional offscreen Qt bake reference

Runs the preserved application UV/bake implementation as a separate developer
probe, before extracting portable pixel baking. This standalone CMake project is
never included by the root CMake project or SwiftPM. Native libraries, default
contracts and Apple apps do not acquire Qt dependencies.

## Build and use

Requires the normal C++17/CMake toolchain and **development-only Qt 6.4+ Core and
Gui** with the offscreen platform plugin. For example, Homebrew's `qtbase` provides
those modules on macOS. No Qt source is copied into the portable engine. Qt keeps
its own license; this optional reference executable is not shipped with the MIT
native products. Analytic tests alone and source inspection cannot check actual
QPainter/image sampling or alpha behavior, which is why this separate tool exists.

From the repository root (choose fresh build directories):

```sh
cmake -S . -B /tmp/dust3d-native-reference -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/dust3d-native-reference --parallel 4
ctest --test-dir /tmp/dust3d-native-reference --output-on-failure
cmake -S tests/reference -B /tmp/dust3d-qt-reference \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qtbase \
  -DDUST3D_CORE_BUILD_DIR=/tmp/dust3d-native-reference
cmake --build /tmp/dust3d-qt-reference --parallel 4
ctest --test-dir /tmp/dust3d-qt-reference --output-on-failure
```

On other systems, point `CMAKE_PREFIX_PATH` to the installed Qt prefix as needed.
`DUST3D_CORE_BUILD_DIR` must contain matching native engine/core/remesher/triangle
static archives from this checkout, compiler, configuration and architecture.
Use the shown single-configuration Makefiles/Ninja layout, with the cache and
archives in the same build directory. Multi-configuration archive layouts are not
supported by this optional helper; native builds themselves remain unaffected.

Configuration checks the source directory and sanitizer mode. Before linking,
the helper rebuilds the matching native engine target. A reference-owned completion
stamp is an explicit linker dependency, so Ninja relinks after that nested build.
This intentionally relinks each successful reference build; native archives remain
owned by the native build and are not deleted when cleaning the reference target.
Do not simultaneously run another build in that same native build directory.
Native link dependencies and preserved Qt application sources remain unchanged.

Run one probe to print JSON. An optional third argument saves diagnostic PNGs to
an explicit output directory; no files or images are written there by default.
The CLI always selects `QT_QPA_PLATFORM=offscreen` and passes only its executable
name to GUI startup. Extra platform options are rejected instead of being consumed
by Qt; no app UI is launched.

```sh
/tmp/dust3d-qt-reference/dust3d_qt_bake_reference \
  image "$PWD/tests/core/fixtures"
/tmp/dust3d-qt-reference/dust3d_qt_bake_reference \
  procedural "$PWD/tests/core/fixtures" /tmp/dust3d-bake-diagnostics
```

Do not commit locally rendered PNGs as snapshot baselines. Diagnostics are not
reference-design approval or evidence of full GPU/display parity.

## Scenarios and boundaries

- `solid`: opaque and partial-alpha color, including Qt premultiplied byte rounding.
- `image`: twelve independent source-pixel centers in asymmetric 2×3/3×2 images;
  the fixture must exercise an actual flipped chart, including axis transposition.
- `gradient`: real seam endpoint direction and encoded channel interpolation.
- `material`: optional metallic/roughness/emissive maps, defaults and reference
  clipping of emission strength above one.
- `procedural`: world-space Spots plus a serialized physical fold, nonuniform
  color, perturbed normal texels, flat other charts and resolved tangents. A valid
  partial local UV chart bounds test cost while exercising actual algorithms.
- `transparent`: records observed RGBA and overwrite flag; it does **not** assert
  that a reference transparency limitation would be correct behavior to copy.
- `corpus`: image-free Capsule/Sword/RoundedCrossing through the existing validated
  native reader/generator, then preserved Qt UV orchestration. Compares portable
  original-object corner UVs at tolerance 1e-10 and records zero-UV rows. Qt has no
  explicit known marker, so zero rows are compared only on these fixtures. This
  does not independently test Qt archive loading, generator UI configuration,
  imported assets or float-filtered C geometry ordering.
- `input-bounds`: rejects a sparse oversized input before allocating archive bytes.

Two additional negative startup tests reject extra Qt platform options and unknown
scenarios. `qt_reference_incremental_relink` builds the real target, touches only
`uv_atlas_builder.cc`'s timestamp (not its contents), then checks the native archive
refreshes and the executable relinks afterward. It verifies the source hash stays
unchanged. CTest runs this case serially; do not run it concurrently from multiple
reference build directories sharing one checkout. Ninja users should run the full
suite with Ninja installed as their selected CMake generator, not assume Makefiles
exercise Ninja scheduling. The helper leaves the source timestamp refreshed, which
may cause other native build directories to recompile that unchanged file.

The reference remains fixed at 4096 color / 2048 fold-normal / 1024 material maps.
Probes do not change private fields or add production fault switches. Qt-internal
libraries are not covered by native C++ coverage reports. Existing Qt source API
deprecation warnings are reported rather than hidden or mass-rewritten.

## Sanitizers

Build native archives separately with `DUST3D_ENABLE_SANITIZERS=ON`, then configure
a fresh reference build using that archive directory and
`-DDUST3D_REFERENCE_ENABLE_SANITIZERS=ON`. Both sets must use compatible ASan/UBSan
flags. Run CTest with `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`.
Qt's installed shared libraries remain uninstrumented; this does not establish
Qt-internal sanitizer coverage. macOS does not support the Linux leak-detection
option used by hosted core CI. No sanitizer flags affect native builds by default.
