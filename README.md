# Dust3D Core — maintained downstream

Public C++17 modeling engine and validated C API for the native Apple apps.
Preserves full history from [huxingyi/dust3d](https://github.com/huxingyi/dust3d)
and regularly integrates upstream through reviewed synchronization PRs.
The original MIT license and third-party notices remain intact; new project code is MIT.
This repository is a history-preserving duplicate, not a second GitHub network fork.

Internal `SnapshotInvalidation` derives conservative component/part invalidation
from immutable snapshot differences, ignoring persisted dirty flags and following
profile, inheritance and wrapping dependencies. See [verification](docs/snapshot-invalidation-verification.md).
Optional generation sessions now integrate this invalidation with owned bounded caches;
see [session design](docs/adr/0007-bounded-generation-sessions.md).

Retained cache/output owners also support checked, allocation-free payload
accounting, including reserved capacities and borrowed geometry boundaries. See
[verification](docs/retained-payload-verification.md). This is a conservative data
payload estimate, not a process-memory ceiling. Session budgets default to 64 MiB;
zero disables retention and the supported maximum is 256 MiB. Cache-only failures
fall back to fresh generation with a separate diagnostic; returned mesh owners
survive later session loads, reset and destruction. All native generation shares
one engine lock. The stateless API remains available. See
[session verification and limits](docs/generation-session-verification.md).

Optional `dust3d_sourced_mesh_load` now exports independently owned original-key
source hints alongside geometry. Per-corner records preserve weighted associations,
unknown/ambiguous identity states and indexed triangle alignment. These are
quantized hints, not exact Boolean surface ownership. Source output has a separate
64 MiB default logical payload budget (256 MiB maximum); legacy/ordinary session loads do
not acquire source buffers. `dust3d_generation_session_load_sourced` obtains the
same independent source owner while reusing bounded component caches; see
[cached source verification](docs/cached-source-verification.md). See [contract and verification](docs/source-provenance-verification.md).

The inherited UV packer now uses wide score/area arithmetic and rejects invalid
quantized dimensions or charts erased by padding precision. Failed `tryPack`
attempts clear prior layouts; see [regression and limits](docs/uv-packer-numeric-verification.md).
`dust3d/uv/uv_atlas_builder.h` now provides owned portable atlas metadata from a
const generated Object plus resolved component colors/image dimensions. Outputs
include corner UVs, explicit missing-chart markers, component UUIDs and ordinal
seam-gradient descriptors. The default logical output budget is 64 MiB (maximum
256 MiB); workspace and allocator overhead are excluded. See
[contracts and remaining limits](docs/uv-atlas-verification.md) and
[ADR 0011](docs/adr/0011-portable-uv-atlas.md). Baking, Apple app adoption and full
texture rendering remain separate stages. Existing geometry APIs remain compatible.

Opt-in `dust3d_uv_mesh_load` now returns an independent C owner with triangle-aligned
UVs, explicit known/unknown mappings, chart/gradient descriptors and original
component strings. Only float-retained geometry triangles gain published UV
records. Caller-resolved bounded image dimensions avoid Qt/global image lookup;
strict source color parsing preserves alpha-first notation. Aliases and generated
identities remain explicit. The default 64 MiB logical quota charges retained
atlas/C metadata and identity records (maximum 256 MiB), excluding geometry and
workspace/allocator overhead. See [C/Swift contracts and limits](docs/uv-metadata-verification.md)
and [ADR 0012](docs/adr/0012-owned-uv-metadata-boundary.md). These are metadata,
not baked texture pixels or new renderer functionality.

## Build and test without Qt

Requires CMake 3.20+, a C/C++17 compiler and platform thread support. No Swift,
Qt, display server or external geometry download is needed.

```bash
git clone --recurse-submodules https://github.com/dalbrecht/dust3d-core.git
cd dust3d-core
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

For ASan/UBSan, configure a separate build with `-DDUST3D_ENABLE_SANITIZERS=ON`.
The `dust3d_engine` CMake target links the required core/remesher dependencies;
its public header is `native/Engine/include/Dust3DBridge.h`. Direct static-library
consumers must link those transitive archives too. No installable SDK or stable
binary ABI release is promised by this initial source setup.

The current API generates procedural geometry and colors from existing `.ds3`
archives. Textures are untextured previews with warnings; embedded GLB geometry
is explicitly unsupported. Editing/encoding, texture baking, export and animation
APIs are planned. `dust3d_mesh_load` returns an owning handle for nonempty output. A NULL handle
with `DUST3D_STATUS_NO_GEOMETRY` means generation succeeded without triangles
(for example an isolated closed loop with its fill disabled). Other non-OK
statuses remain failures. Callers can clear a preview for this distinct result
without treating malformed input or failed geometry as successful emptiness.

Input validation does not impose hard resource limits inside
legacy geometry algorithms; generation is serialized and not interruptible.

## Maintenance and quality

- [Upstream synchronization and provenance](docs/upstream-maintenance.md)
- [Editor, repository and testing roadmap](docs/editor-port-plan.md)
- [C++ conventions](docs/cpp-standards.md): retain C++17 and the inherited `.clang-format`.
- `.standards` pins the shared coding standards; `AGENTS.md` supplies core-specific rules.
- CI builds on Linux/macOS and runs a separate Linux ASan/UBSan configuration.
- CTest now includes twenty-five contracts/scenarios, including indexed ownership/reconstruction/budgets/seams, exported precision, bounded angles, cloth openings, configured stitching lines/loops/fill, successful empty output, numeric locales and boolean boundaries; this is not full engine coverage.
  Existing C++ warnings and broad algorithm coverage remain adoption work.

`upstream-sync` is untouched upstream history; `master` contains downstream additions.
The Qt application and original tools remain as reference source, excluded from the
portable build. Legacy Qt release workflows are replaced by core CI on the maintained
branch. The Apple [viewer/editor repository](https://github.com/dalbrecht/dust3d-swiftui)
remains separate; its current viewer PR is not coupled to this repository's migration.

---

The original upstream README follows for provenance and Qt reference workflows.
It does not describe the native core build above.

# Dust3D

Dust3D is a cross-platform 3D modeling software that makes it easy to create 3D models for video games, 3D printing, and more. It features built-in rigging, procedural animation and UV unwrapping, with export to GLB and FBX formats.

For more information, visit [dust3d.org](https://dust3d.org/).

## Getting Started

These instructions will get you a copy of Dust3D up and running on your local machine for development and testing purposes.

### Prerequisites

- Qt 6 (recommended) or Qt 5, with the following modules: qtbase, qtsvg, qtmultimedia
- C++ compiler with C++17 support

#### Windows

1. Download and install the `Qt Online Installer`
2. Run the installer and choose the Qt 6 archives you want to install (required: qtbase, qtsvg, qtmultimedia)
3. Install Visual Studio 2022

#### macOS

1. Install Qt via Homebrew: `brew install qt`
2. Install Xcode Command Line Tools: `xcode-select --install`

#### Linux

1. Install Qt 6 using your distribution's package manager:
   - Ubuntu/Debian: `sudo apt install qt6-base-dev qt6-svg-dev qt6-multimedia-dev libgl-dev`
   - Fedora: `sudo dnf install qt6-qtbase-devel qt6-qtsvg-devel qt6-qtmultimedia-devel`

### Building

1. Clone the repository
```
git clone https://github.com/huxingyi/dust3d.git
```

2. Build using qmake

#### Windows

1. Open the Qt Creator IDE
2. Select "Open Project" from the File menu
3. Navigate to the project directory `dust3d/application` and open the `.pro` file
4. Select the desired build configuration (e.g. Debug or Release) from the dropdown menu at the bottom left of the window
5. Click the "Run" button to build and run the project

#### macOS / Linux

1. Change to the project directory `dust3d/application`
2. Run `qmake` to generate a Makefile
3. Build the project using `make`

### Releasing

1. Make sure all changes are merged to master branch including CHANGELOGS update
2. Run `git tag <version>` (e.g. `git tag 1.1.0`)
3. Run `git push origin <version>`
4. Wait for Actions/release to finish and download all the Artifacts
5. Go to `Tags/<version>` and create release from tag
6. Title the release as `<version>`
7. Copy description from CHANGELOGS
8. Drag the Artifacts to binaries: `Dust3D-win32-x86_64.zip`, `Dust3D-x86_64.AppImage`, `Dust3D.dmg`
9. Publish release

## License

Dust3D is licensed under the MIT License - see the [LICENSE](https://github.com/huxingyi/dust3d/blob/master/LICENSE) file for details.

<!-- Sponsors begin --><!-- Sponsors end -->

## Boolean regression corpus

The portable suite includes rounded crossing, translated, hard-join, difference
and near-separated procedural fixtures. In addition to finite geometry, colors and
unit normals, these cases check closed oriented surface edges and connected-piece
counts at an explicit 1e-5 position tolerance. See the
[fixture provenance](tests/core/fixtures/README.md) and
[boolean traversal decision](docs/adr/0001-boolean-intersection-loops.md).
This is targeted regression evidence, not full inherited geometry coverage or Qt
feature parity.

A direct low-level boolean test also checks thin solids across zero on each axis,
retaining their triangles and signed volume when same-key points exceed the weld
tolerance.

## Numeric archive compatibility

The native validator accepts both dot and comma decimal separators, matching the
inherited geometry parser and archives from older locale settings. Normalization
is local to validation; the archive/snapshot data is not rewritten. Full input
consumption, finite values, the existing magnitude limit, numeric underflow and
negative node-radius rejection remain enforced. The `contract_numeric_decimals`
CTest entry verifies actual C ABI mesh equivalence and malformed-input rejection.


### Exported triangle precision

The validated C ABI emits triangles with positive area at its float coordinate
precision. Fully validated triangles that collapse during conversion are omitted;
if all generated triangles collapse, loading fails with GENERATION_FAILED and a
precision-specific diagnostic. Successful engine output with no triangles retains
its distinct NO_GEOMETRY result. Source node/part counts and owning-buffer
lifetimes are preserved. See [verification](docs/exported-triangle-precision.md)
and ADR 0003 for the policy and its limits.


### Optional indexed buffers

Given a valid expanded owner, `dust3d_mesh_create_indexed(source, maximum_bytes,
&error)` returns an independent owner. Destroy the source after conversion if it
is no longer needed. Read unique `Dust3DVertex` values and UInt32 triangle indices
with `dust3d_indexed_mesh_vertices` / `dust3d_indexed_mesh_indices` and their count
functions; release with `dust3d_indexed_mesh_destroy`. Source counts and warning
text are copied. Full position, normal and color values define sharing, so hard
normal and color seams survive reconstruction.

The maximum-byte argument limits exported vertex/index payload, with zero meaning
256 MiB. It excludes the expanded input, temporary lookup storage and allocator
overhead. Conversion errors never publish partial ownership and preserve their
cause. The expanded API remains available. See [verification](docs/indexed-mesh-verification.md)
and ADR 0004. Renderer adoption, source identities, UV/material attributes and
high-density/peak-memory acceptance remain later stages.


## Optional offscreen bake reference

A separate [developer harness](tests/reference/README.md) runs the preserved Qt
UV/bake sources against analytic charts and the image-free fixture corpus. It
uses development-only Qt Core/Gui, verifies matching native build configuration
and refreshes native archives before linking. Native CMake/SwiftPM products and
default CI remain Qt-free. Probes measure image transposition, alpha rounding,
seam gradients, material maps, procedural color/fold normals and narrow numerical
UV agreement. See [observations and limits](docs/qt-bake-reference-verification.md).
This is test tooling, not portable pixel baking or full editor parity.

## Portable CPU texture baking

The C++ `dust3d/uv/texture_baker.h` API bakes explicit RGBA8 source images,
solid/seam colors, material scalars and world-space pattern/fold details into
independently owned maps. Inputs remain unchanged; unknown UVs stay explicit.
See [contracts, measured Qt comparisons and alpha policy](docs/texture-baker-verification.md)
and ADR 0014. C/Swift pixel ownership, decoding and Apple rendering follow in
separate dependency stages; this API alone does not complete editor M4.
