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

Geometry-preview loads now bind bounded embedded GLB assets to the existing
imported-model spine generator. Required asset failures remain primary errors;
opaque unreferenced assets stay untouched. Expanded/sourced/session owners disclose
unapplied GLB materials, textures and transparency; UV/bake operations still reject
imports pending complete material integration. See [design](docs/adr/0017-embedded-model-geometry-generation.md)
and [verification](docs/imported-model-generation-verification.md). Apple SDK/pin,
editor/file-preview adoption and full import/export parity remain separate stages.

## Build and test without Qt

Requires CMake 3.20+, a C/C++17 compiler, platform thread support and system zlib
headers/library (for example `zlib1g-dev` on Debian/Ubuntu; macOS SDK provides it). No Swift,
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
consumers must link those transitive archives and system zlib (`-lz`) too. No installable SDK or stable
binary ABI release is promised by this initial source setup.

The current API provides procedural/imported geometry previews, optional source
and UV metadata, and opt-in CPU baking. Imported GLB support currently applies to
geometry-preview paths only, with explicit material limitations. Editing/encoding,
export and animation integration remain later stages. `dust3d_mesh_load` returns
an owning handle for nonempty output. A NULL handle
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


### Opt-in native baked output

`dust3d_baked_mesh_load` accepts sized `Dust3DBakeOptions` and optional borrowed
straight-alpha RGBA8 `Dust3DBakeImage` descriptors. The returned independent owner
exposes borrowed geometry/UV metadata, map views and one tangent record per
exported triangle. Copy views before `dust3d_baked_mesh_destroy`; never separately
destroy the borrowed UV/geometry owner. Absent optional maps return zero views.

Zero dimensions select the portable baker defaults; small explicit dimensions are
useful for contract tests. UV and bake quotas are independent. Final bake quota
counts pixels and exported tangent records, with a separate temporary C++ baker
ceiling. Workspace and allocator capacity are excluded. Cancellation is cooperative;
retain its token until loading and all concurrent token operations finish, then
destroy it. Cancel/query do not acquire the engine mutex. Inherited generation
only observes phase boundaries. See `Dust3DBridge.h` and ADR 0015 for precise
options, status, lifetime and unfinished acceptance gates. A strict Swift 6 smoke
copies actual Capsule/Sword output; this does not establish renderer parity.

## Imported material inspection (prototype)

`dust3d_imported_assets_load` returns an immutable owner for required imported GLB
materials and encoded PNG/JPEG storage. Inspect material records and original
component/part bindings, copy borrowed strings/bytes, then call
`dust3d_imported_assets_destroy`. Null and out-of-range accessors return empty views.
Optional output limits cover retained record/string/buffer capacities; input and
decoded-model quotas apply separately. Empty editor graphs have empty material
sets. See the public header, [ADR 0018](docs/adr/0018-owned-imported-materials.md)
and [verification](docs/imported-asset-ownership-verification.md).

This operation does not generate geometry, validate image pixels, apply textures,
or complete imported UV/bake/PBR support. Existing geometry preview warnings and
UV/bake import rejection remain explicit until those integrations land. Apple
decoding belongs at the bounded ImageIO boundary, after copying the C views.

### Imported surface source attributes

`dust3d_imported_surface_mesh_load` returns one independent owner for generated
geometry and parallel source records. Supply `Dust3DImportedSurfaceOptions` with
`struct_size = sizeof(Dust3DImportedSurfaceOptions)` and zero for the default
64 MiB retained-payload budget (maximum 256 MiB). Query geometry, triangle records
and catalogs while that owner lives, copy any buffers needed later, then call
`dust3d_imported_surface_mesh_destroy` exactly once. Do not destroy its borrowed
geometry separately. Null queries are safe; load errors return null plus a typed
`Dust3DError` whose message preserves the cause.

```c
Dust3DImportedSurfaceOptions options = {sizeof(Dust3DImportedSurfaceOptions), 0};
Dust3DError error = {0};
Dust3DImportedSurfaceMesh *owner =
    dust3d_imported_surface_mesh_load(bytes, byte_count, &options, &error);
if (owner) {
    const Dust3DImportedSurfaceTriangle *triangles =
        dust3d_imported_surface_mesh_triangles(owner);
    size_t count = dust3d_imported_surface_mesh_triangle_count(owner);
    /* Copy count records and borrowed geometry before destroying owner. */
    (void)triangles;
    (void)count;
    dust3d_imported_surface_mesh_destroy(owner);
}
```

Authored UVs may repeat outside 0–1. They are not packed atlas UVs. Unknown UVs
are zero with `uv_known == 0`; absent catalog indices are `SIZE_MAX`. RGB encoding
is explicit: glTF vertex colors are linear, document fallback colors are sRGB;
alpha is always straight and linear. Values exclude material factors and texture
samples. Generated catalog membership does not establish unique Boolean
provenance. This API adds no texture decoding, sampling, atlas/bake integration
or new renderer behavior. See [design](docs/adr/0020-owned-imported-surfaces.md)
and [verification](docs/imported-surface-verification.md).

### CPU imported color sampling

`dust3d/uv/color_texture_sampler.h` provides owned RGBA8 sampling with repeat,
mirrored-repeat or clamp wrapping and explicit mip LOD. RGB is filtered in linear
space; alpha stays straight. Pass decoded top-to-bottom sRGB pixels through
`ColorTextureSampler::create(image, options)` and use `sampleLinear(u, v, lod)`.
Source buffers are borrowed only during construction. See
[ADR 0021](docs/adr/0021-cpu-imported-color-sampling.md) for quota semantics and
remaining imported atlas/baking integration. No image decoder dependency is added.

### Source-aware atlas layout

`SourceUvAtlasBuilder::build(object, legacyAtlas, options)` packs imported triangles
by original ordinal and preserves authored UVs separately from packed UVs. An optional
legacy region transforms existing procedural output without changing its internal
policy. Results own model/component identity strings and support repeated source UVs
and coincident imports. See [ADR 0022](docs/adr/0022-ordinal-source-uv-atlas.md) for
chart-count/payload bounds, cancellation limits and remaining bake integration.

### Source-aware CPU baking

`SourceTextureBaker::bake(atlas, inputs, options)` rasterizes authored imported UVs
into packed charts and composes borrowed procedural maps through the legacy region.
Provide exact-key materials, aligned corner colors and optional owned CPU samplers;
all borrowed storage must outlive the call. Output owns color/PBR maps and aligned
alpha/sidedness policies. Chart-owned dilation and one-texel collars support bilinear
output filtering; honor `maximumSamplingLod=0` until chart-isolated atlas minification
lands in #59. Source-image mip sampling remains supported. See [ADR 0023](docs/adr/0023-source-aware-cpu-baking.md) for
linear color, mip footprint, normal-transposition and resource/coverage requirements.
Native document loading and Apple adoption remain separate; no image decoder is added.

`SourceTextureBaker::bakeMipChain(atlas, inputs, options)` additionally owns safe
floor-halved atlas levels. Set `SourceTextureMipBakeOptions::legacyAtlas` to the
original procedural `UvAtlas` when a legacy region exists: its internal islands
must remain separate owners. Missing island metadata retains LOD zero. Color and
emission filter in linear light, scalar maps linearly, and normal vectors normalize
after averaging. Honor the returned common `maximumSamplingLod`; partial chains
must not be extended with global filtering. Base `bake()` and native accessors
remain unchanged. See [ADR 0060](docs/adr/0060-chart-isolated-owned-mip-chains.md)
for footprint, quota, validation and remaining native/Swift/export requirements.

### Native source-aware bake owner

`dust3d_source_baked_mesh_load` opts into imported/procedural composition without
changing the existing loaders. Resolve encoded materials with the inspection API,
decode matching images into bounded sRGB straight RGBA8, then keep those buffers
alive throughout this synchronous serialized call:

```c
Dust3DSourceBakeOptions options = {0};
options.struct_size = sizeof(options);
options.legacy.struct_size = sizeof(options.legacy);
options.model_images = decoded_model_images;
options.model_image_count = decoded_model_image_count;
Dust3DSourceBakedMesh* owner =
    dust3d_source_baked_mesh_load(bytes, size, &options, &error);
if (owner != NULL) {
    const Dust3DMesh* geometry = dust3d_source_baked_mesh_geometry(owner);
    const Dust3DSourceBakedTriangle* triangles =
        dust3d_source_baked_mesh_triangles(owner);
    Dust3DTextureView color =
        dust3d_source_baked_mesh_texture(owner, DUST3D_TEXTURE_COLOR);
    double maximum_lod = dust3d_source_baked_mesh_maximum_sampling_lod(owner);
    /* Copy needed views/catalogs before destruction; never destroy geometry.
       Honor maximum_lod (currently 0), uv_known, policy_known and tangent.known. */
    dust3d_source_baked_mesh_destroy(owner);
}
```

Decoded model IDs are exact byte spans from the material catalog; source buffers
and cancellation tokens are borrowed only during the call. Results own geometry,
source catalogs, packed UVs, tangents, complete-color maps and aligned draw policies.
Known mapped geometry has neutral vertex modulation. Direct imported component
color-image overrides reject explicitly until supported. No decoder or dependency
is added. See [ADR 0024](docs/adr/0024-owned-native-source-baking.md) for aggregate
quota, workspace/cancellation limits and [verification](docs/native-source-baking-verification.md).
The opt-in `dust3d_source_baked_mesh_load_with_mips` accepts
`Dust3DSourceBakeMipOptions`. Initialize `struct_size`, `source.struct_size` and
`source.legacy.struct_size`; `source` contains the same settings as above.
`maximum_levels` includes base level zero (zero defaults to 14, explicit one
retains only the base). Chart isolation can stop the chain earlier. Query
`dust3d_source_baked_mesh_texture_level_count(owner, map)` and borrow each
`dust3d_source_baked_mesh_texture_level(owner, map, level)` before destroying the
owner. Level zero aliases the existing map getter. Never generate a global mixed
atlas chain or sample beyond `maximum_sampling_lod`. Additional image records and
pixel capacities count toward the aggregate output quota; workspace is excluded.
See [ADR 0061](docs/adr/0061-native-owned-atlas-mips.md).
Apple SDK/rendering/export adoption remains required; no full material/editor
parity is claimed.

### Default fault-probe containment (#84)

Ordinary CMake/CTest builds exclude intentional allocation-failure probes. The
four legacy contract executables retain normal validation, ownership and cache
behavior, while JSON comparisons use ordinary inputs and compile-time signature
checks. Removed probes remain in Git history, not behind a runtime opt-in. The
source guard rejects their known restoration paths. Use a fresh build for
verification; do not run pre-containment binaries or replay historical fault
cases. Allocation-failure coverage is deferred, not claimed passing. GLB #69 and
#67 remain quarantined/blocked. See ADR 0045 for the audit correction and limits.

### Strict Swift mip ownership contract

Normal native macOS CMake test builds compile `tests/swift/SourceBakedMipSmoke.swift`
with Swift 6 complete concurrency checking and warnings as errors. The
`source_baked_mip_swift` CTest copies actual Capsule atlas levels before destroying
the native owner, verifies per-level dimensions/caps, and transfers immutable
owned data across actors. This exercises the C importer against the same CMake
engine libraries, without changing an Apple dependency pin. It runs automatically
with the full build and CTest; `dust3d_source_baked_mip_swift` is the focused build
target. It requires Xcode command-line tools and swift-format. Linux and sanitizer
configurations retain the C/C++ mip contracts; this macOS Swift consumer is excluded
there. It does not establish app renderer or full material/export acceptance.

### Portable worker framing (preparation only)

The CMake target `dust3d_worker_protocol` provides a standard-library-only C++17
version-one frame codec and document/request/revision response checks. The frame codec does not
launch a worker, validate a mesh payload or change the production engine pin.
`cmake --build build --target dust3d_worker_protocol_tests` and
`ctest --test-dir build -R worker_protocol_contracts --output-on-failure` exercise
small independent wire fixtures. Full CMake/CTest includes this contract.

Include `native/Engine/WorkerProtocol.h` and link the target. For example:

```cpp
dust3d::worker::Header request;
request.identity.document[0] = 1; // Substitute a parent-issued live-document identity.
request.identity.request = 1;
request.identity.revision = 0;
request.payloadBytes = archive.size();
auto wire = dust3d::worker::encodeFrame({ request, archive }, {});
```

Here `archive` is an owned byte vector. Header decoding rejects unsupported fields
and advertised lengths before body allocation; complete decoding copies owned
bytes and rejects fragments/trailing bytes. ProtocolError preserves a typed cause,
and ordinary allocation exceptions propagate. Parent transport must check the
pending identity before reading a body and separately validate result semantics
before publication. See [ADR 0062](docs/adr/0062-portable-worker-framing.md) for the
wire layout, limits and required containment follow-ups (#109).

### Worker base-mesh payloads

The same CMake library also provides `encodeMeshPayload` and `decodeMeshPayload`
in `native/Engine/WorkerMesh.h`. These separately validate owned expanded triangle
geometry and bounded UTF-8 warnings using an explicit integer/IEEE-754 wire format.
For example, `auto bytes = dust3d::worker::encodeMeshPayload(mesh, {});` encodes an
owned `MeshPayload`; `decodeMeshPayload(bytes.data(), bytes.size(), {})` returns an
independent owned value or throws a typed MeshPayloadError. Ordinary allocation
exceptions propagate. Neither function launches an engine or process.

Validate framing and pending identity before semantic decoding and publication.
Wire, decoded-record, source-count and warning limits are independent; logical
quotas exclude roots/capacities/allocator overhead and do not cap RSS. Empty meshes,
nonfinite coordinates, nonunit normals, invalid RGBA, degenerate triangles and
malformed text reject. Full CTest includes `worker_mesh_contracts`, including a
licensed Capsule through the ordinary native C API, framing and semantic codec.
See [ADR 0063](docs/adr/0063-worker-mesh-payloads.md) for the versioned layout and
remaining worker/source-aware/release work (#110).

### Typed worker failures

`native/Engine/WorkerFailure.h` provides `encodeFailurePayload` and
`decodeFailurePayload` in the same CMake library. For example,
`auto bytes = dust3d::worker::encodeFailurePayload({ dust3d::worker::FailureStatus::cancelled, {} }, {});`
encodes a typed cancellation with an originally empty cause. Decode with
`decodeFailurePayload(bytes.data(), bytes.size(), {})` to obtain owned original
bytes; malformed schemas, success/unknown statuses, limits and incomplete/extra
spans throw FailurePayloadError. Ordinary allocation errors propagate.

All six native non-success outcomes remain distinct. Opaque causes preserve
non-UTF-8 and control bytes without interpretation or logging. Validate outer frame
kind and pending identity first, then apply safe presentation and application
state policy. Default total failure payload is at most 4096 bytes including its
16-byte prefix. Full CTest includes `worker_failure_contracts`, with an ordinary
invalid archive through the existing C ABI. See
[ADR 0064](docs/adr/0064-worker-failure-payloads.md) for exact wire IDs, quotas and
remaining process/transport acceptance (#111).

### Single-job native worker prototype (Unix)

Build the headless helper with `cmake --build build --target dust3d_worker`. It
accepts no command-line arguments: feed one complete framed request, then EOF,
and receive one framed base-mesh or typed native failure:

```sh
build/dust3d_worker < request.frame > reply.frame 2> transport-diagnostics.txt
```

The request envelope is [ADR 0062](docs/adr/0062-portable-worker-framing.md), with
generateMesh operation and raw `.ds3` body (default 128 MiB input maximum). Responses
use [mesh](docs/adr/0063-worker-mesh-payloads.md) or
[failure](docs/adr/0064-worker-failure-payloads.md) schemas and echo pending identity.
Exit zero means a complete reply, including ordinary non-success outcomes; exits
64/65/70/74 distinguish usage/protocol/internal/I/O errors. Never publish a partial
reply or a reply from a nonzero/abnormal exit. Native causes remain owned protocol
data, not stderr logs. Runtime sanitizer stderr stays observable. Treat all child
stderr as bounded untrusted data in a future parent.

The portable `dust3d_worker_operation` adapter can also execute
`generateMeshReply(request, limits)` through the serialized native ABI or map
`copyNativeFailure(error)` into owned data. Public limits validate before execution;
consumer copy budgets can reject ordinary generated output without resource
exhaustion. It changes no global streams and invokes no save API. The executable
keeps OS descriptor handling separate and suppresses backend stdout corruption.

Full CTest includes `worker_operation_contracts` and `worker_process_contracts`,
with Capsule/Sword, existing seam diagnostics, typed failure, bounded data and real
ordinary I/O rejection. This is a development prototype: parent admission, real
pipe/queue/deadline/resource/cancellation/sandbox acceptance, source-aware results
and signed application packaging remain pending. It changes no production pin or
quarantine. See [ADR 0065](docs/adr/0065-single-job-native-worker.md) (#112).

The portable `PendingReply` gate in `WorkerAdmission.h` admits a collected helper
reply only once, after the caller supplies observed normal exit zero and the
current document/request/revision identity. It validates framing and payload
semantics and returns independent mesh/failure data. Call `retire()` when work is
cancelled, replaced or its document closes. Serialize all access; do not infer exit
from output bytes. This API does not launch or sandbox a child or collect pipes.
See [ADR 0066](docs/adr/0066-worker-response-admission.md).

On macOS and supported Linux hosts, CMake also builds `dust3d_worker_transport`.
`runWorkerJob(trustedAbsoluteHelperPath, request)` sends one request over real Unix
channels, concurrently drains bounded reply/diagnostic data and returns observed
process outcome. Use `PendingReply.complete` afterward with current identity;
transport bytes alone never authorize publication. This synchronous development
API has no deadline or hard cancellation and cleanup may block; run it off the UI
thread and keep production adoption gated. See [ADR 0067](docs/adr/0067-unix-worker-transport.md).


### Bounded worker scheduling

`RequestQueue` in `native/Engine/WorkerQueue.h` links portable scheduling state to
reply admission. Submit an owned request with a globally increasing request
number; `submit` returns any replaced waiting identity. `begin` transfers the
latest archive only if no request is active. The serialized owner can replace,
cancel or close while a separate I/O executor collects the active transport.
Return collected bytes and actual outcome to `complete`; only its owned admitted
result can publish. On a transport exception, wait for its cleanup/reaping, call
`abandon(identity)` on the serialized owner, and surface the original exception.
Foreign completion/failure cannot release a different active request.

```cpp
dust3d::worker::RequestQueue queue;
auto replaced = queue.submit(std::move(requestFrame));
auto next = queue.begin(); // nullopt while transport is active
// Transfer next to a separate I/O executor; return completion to this owner.
```

The queue retains one waiting archive and one active header; caller-owned input,
transport buffers and process RSS are outside that bound. Access is externally
serialized. Cancellation retires publication without stopping native work. No
deadline, forced termination, sandbox, production pin or application adoption is
included. See [ADR 0068](docs/adr/0068-bounded-worker-scheduling.md) (#115).

### FBX convention foundation (development)

`encodeFbxTexturedMeshScene` adds one composed atlas material and six optional
caller-validated PNG channels: color, normal, metallic, roughness, emissive and AO.
Scalar and pure image-length admission precede embedding; inputs remain owned by
the caller and output is independent. This does not decode images, bake source
materials or establish rendered shader parity. See [ADR 0083](docs/adr/0083-recovered-fbx-material-scenes.md).

`native/Engine/FbxMeshScene.h` encodes a geometry-only static scene from a bounded
`Object`, preserving source indexing, winding, optional corner normals/UVs and
reference axis/unit metadata. The pure `FbxMeshScenePlan.h` admits scalar counts
and staging bytes before allocation. Materials, rig/animation, full-document and
Apple UI/CLI export remain separate work. The optional Assimp fixture verifier
uses only Python's standard library; its value regressions join CTest when Python
is available. See [ADR 0082](docs/adr/0082-recovered-fbx-mesh-scenes.md).

`native/Engine/FbxDocument.h` preflights immutable node forests and returns owned
FBX7400 container bytes with checked absolute offsets, terminators and the MIT
reference footer. Source nodes remain unchanged; no destination file is written.
The existing vendor codec is linked only by a known-positive test oracle. Complete
scene/importer acceptance remains required; see [ADR 0081](docs/adr/0081-recovered-fbx-documents.md).

`native/Engine/FbxNode.h` supplies move-only immutable property/child trees with
checked aggregate depth, count, retained capacity and exact FBX7400 record sizes.
It does not emit a complete document or file. Child/property arguments may be
consumed on construction failure; borrowed values expire on move/destruction.
See [ADR 0080](docs/adr/0080-recovered-fbx-nodes.md).

`native/Engine/FbxPropertyBlock.h` supplies a move-only bounded property buffer
for FBX7400 scalars, binary strings/raw values and uncompressed arrays. It checks
byte/count admission before growth, preserves prior bytes/count on rejection and
keeps source inputs unchanged. This is in-memory encoding only; it does not write
a complete scene or file. See [ADR 0079](docs/adr/0079-recovered-fbx-properties.md).

`native/Engine/FbxAnimationMath.h` provides checked seconds-to-FBX-ticks conversion
and bounded Euler normalization, signed shortest differences and continuity
unwrapping, plus rigid-matrix XYZ Euler extraction. `RigMatrix.h` defines only the
fixed column-major value; `RigTransformValidation.h` checks finite affine,
right-handed unit frames and context-specific translation limits without invoking
a pose owner. It retains the reference exporter’s truncation and half-turn ties,
with typed invalid-value and limit errors. These pure C++ helpers do not evaluate
animation, write FBX, expose a C ABI, or change the Apple production pin. Static
serialization, rig/animation export and independent-consumer acceptance remain
separate stages under #99. See [ADR 0077](docs/adr/0077-recovered-fbx-conventions.md)
and [ADR 0078](docs/adr/0078-recovered-fbx-rigid-transforms.md).

### Worker lifetime policy (development)

`native/Engine/WorkerLifetime.h` defines an externally serialized pure lifecycle
policy with explicit work, stop-grace and post-exit drain budgets. It returns
single-use decisions and retirement reasons from monotonic elapsed observations;
it does not execute OS actions. `mayAdmit` still requires independent PendingReply
exit/frame/revision validation. The existing transport's unbounded cleanup is
unchanged. See [ADR 0069](docs/adr/0069-worker-lifetime-policy.md) and #123 for
integration and containment limits. All quarantines and the Apple pin remain.

### Opt-in supervised Unix operation (development)

`runSupervisedWorkerJob` adds elapsed-time retirement, a borrowed atomic
`WorkerCancellation` flag and post-exit channel-drain policy to the existing
collector. Retired results throw `WorkerStopped`, preserving owned bounded
original diagnostics and actual collected outcome; cancellation before spawn has
no collected value. Require exclusive child-reaping ownership, call off the UI
thread and still use PendingReply for normal results. Legacy runWorkerJob remains
unchanged. Active OS stop/escalation is not experimentally verified under current
probe restrictions; OS reap can still block. See [ADR 0070](docs/adr/0070-supervised-unix-worker.md)
for remaining resource, sandbox, parent-death and production-adoption gates.


### Worker resource-profile planner (development)

`native/Engine/WorkerResources.h` validates explicit CPU seconds, regular-file byte
and descriptor-number budgets and always plans disabling core dumps. A virtual
address byte cap is optional and is rejected for ASan; an unspecified runtime is
also rejected. The trusted platform adapter supplies support, largest convertible
finite values and inherited soft/hard limits (`nullopt` means infinity). Returned
finite effective caps never raise inherited limits. Planning is pure and makes no
OS calls; zero inherited caps can still make actual helper startup impossible.
This is not RSS, filesystem or descendant containment. The explicit helper CLI
can apply a supported development profile; Apple adoption remains separate. See [ADR 0072](docs/adr/0072-worker-resource-planning.md) (#123).


### Private helper resource setup (development)

`prepareWorkerResources` gathers requested resource observations, validates the
whole profile and applies finite effective caps in order. ResourceSetupError
preserves the original OS code, operation and resource; a caller must exit setup
on failure and must not process an archive. The macOS/Linux UnixResourceSystem
uses an explicit syscall seam and checked finite/infinity conversion. Actual
SystemUnixResourceCalls must only be constructed in a fresh helper. These private
libraries are used only by the helper's explicit opt-in resource launch. The
parent exposes a separate opt-in resource operation; Apple adoption remains open.
Recording tests never change limits in the test runner. See
[ADR 0073](docs/adr/0073-helper-resource-setup.md) for activation and evidence gaps.


### Explicit helper resource launch (development)

The helper accepts a versioned profile on macOS/Linux before reading its framed
request. Use the exact compiled runtime (`ordinary` or `asan`) and explicit finite
CPU-second, regular-file-byte and descriptor-number soft/hard budgets. Core dumps
are always disabled. For example, these are test-only settings, not production
budget recommendations:

```sh
dust3d_worker --resource-profile-v1 ordinary 600 600 16777216 16777216 128 128 \
    < request.frame > reply.frame 2> resources.log
```

Input is the existing binary request frame, not a raw `.ds3` archive. Successful
setup emits a bounded `resource-profile-v1` line with read-back effective limits
on stderr; the response remains framed on stdout. Invalid metadata/runtime and
virtual-cap activation are rejected before archive handling. Legacy no-argument
launch does not select these limits. The parent resource API below verifies the
report; production budgets, signed helper identity, memory/sandbox/parent-death
containment and Apple adoption remain required. See [ADR 0074](docs/adr/0074-opt-in-helper-resource-launch.md).


### Parent resource completion admission (development)

`runResourceLimitedWorkerJob` passes the explicit profile to the helper under the
existing supervision policy. It requires normal exit zero and a bounded matching
resource report before returning ResourceCollectedJob. ResourceJobError retains
the original collected outcome/reply/opaque diagnostics, truncation state and typed
metadata cause. It never retries as an unrestricted helper. For a trusted compatible
ordinary helper and the test-only budgets shown above:

```cpp
PendingReply pending(request.header);
ResourceLaunch launch { ResourceRuntime::ordinary,
    { { 600, 600 }, { 16777216, 16777216 }, { 128, 128 }, std::nullopt } };
auto result = runResourceLimitedWorkerJob(absoluteHelperPath, request, launch);
// Obtain current authoritative identity from the serialized application owner.
auto admitted = pending.complete(activeIdentity, result.collected.outcome,
    result.collected.reply.data(), result.collected.reply.size());
```

Run transport off the UI thread, serialize state/admission and retire failed or
replaced work through its owner. Resource observations preserve the original
binary diagnostic tail by offset; render causes safely rather than assuming UTF-8
or logging document data. Early transport failures retain their existing typed OS
causes; the new owned context applies after collection. Apple integration and
strong containment remain separate. See
[ADR 0075](docs/adr/0075-parent-resource-admission.md).

`native/Engine/FbxGeometrySelection.h` copies ordered source-face subsets into
independent geometry owners while preserving double positions and shared vertex
identities. It supports replacement corner UVs and bounded count/capacity
admission. Other Object/document metadata is excluded. See ADR0084 for measured
coverage, independent selected-face import and remaining export integration.

The internal `GenerationOutputs::fbxGeometry` hook retains original generated
double positions and shared topology independently of the float preview. It is
opt-in, bounded and publishes ownership only after success. See ADR0085; this
is not yet a C API or app export command.

`GenerationOutputs::useFbxAtlasUvs` selects complete packed UVs from the same
call's `UvMetadata` when retaining original export geometry. Missing outputs or
unknown atlas mappings reject explicitly. ADR0086 records real procedural-bake
alignment, scoped coverage and remaining source-aware export integration.

`native/Engine/PngEncoding.h` encodes tightly packed baked RGBA8 pixels as owned
PNG bytes through system zlib. It validates dimensions/spans and bounds encoded
output without full filtered-image staging. See ADR0087 for independent pixel/
CRC validation, dependency/license rationale and unforced boundary coverage gaps.

`native/Engine/FbxBakedMaterial.h` turns borrowed baked map views into owned PNG
channels with an aggregate retained-buffer quota and neutral material factors.
ADR0088 records a generated procedural scene re-import, comparing original
corners, packed UVs and raw baked pixels independently. This remains an internal
building block; the export C ABI and editor command are not yet exposed.

The source-matched static FBX C API (`Dust3DFbxExportOptions` and
`dust3d_fbx_export_load`) returns immutable owned bytes for explicit geometry or
procedural baked-atlas modes. Copy bytes/warnings before destroy. Imported model
assets reject before GLB decoding. ADR0089 documents limits, cancellation, the
real Swift6 actor-copy contract and unforced boundary coverage gaps. This does
not expose an app command or change the production dependency pin.

## Qt-free static FBX command

Build `dust3d_export` with CMake or `swift build --product dust3d-export` in this
core package, then run:

```sh
"$BUILD_DIR/dust3d-export" input.ds3 output.fbx
"$BUILD_DIR/dust3d-export" --baked --texture-size 256 input.ds3 output.fbx
```

The Unix command preserves source bytes and stages/flushes output before atomic
replacement. Geometry is the default; baked mode supports a procedural atlas.
See [command usage](native/ExportCli/README.md) and ADR0090 for bounds, exit codes,
independent C API re-import, measured coverage and remaining export acceptance.
It does not change the Apple pin or enable imported/rigged/animated/GLB export.

`native/Engine/ObjDocument.h` encodes original double vertices and triangle/quad
polygons using the reference OBJ geometry semantics. It preserves shared indices
and winding and has an opt-in original-object generation hook. ADR0091 records
scoped coverage, independent polygon re-import and the remaining preview-validity
coupling. This internal path is not yet exposed by the C API or export command.

Original export preparation now skips float preview conversion and vertex storage;
static FBX geometry/baked API and CLI callers use it. Default preview behavior is
unchanged. See ADR0092 for the controlled precision regression, independent baked
re-import, scoped coverage and remaining OBJ/app integration.


The development OBJ C API now exposes independent immutable original polygon bytes via Dust3DObjExportOptions and dust3d_obj_export_load. Copy borrowed bytes before destroy. ADR0093 records quota/lifetime semantics and validation limits. CLI dispatch and app adoption remain pending.


The export command also accepts .obj destinations for original double geometry v/f polygons. --baked OBJ rejects explicitly. ADR0094 documents Swift 6 owned-copy verification, independent generated OBJ re-import and remaining app adoption.


The Qt-free command accepts --batch followed by at most64 explicit input/output pairs. It preflights cross-job source/output aliases, commits jobs independently, continues failures and returns5 with a committed/failed summary. See native/ExportCli/README.md and ADR0095 for retry semantics and compatibility limits.


The headless command now also accepts one source with repeated -o/-output targets. These share batch source-protection/publication checks; aggregate legacy failure returns1. ADR0096 documents argument/status compatibility and the remaining Qt workflow gaps.


Use tools/benchmark_static_exports.py after a fresh source-audited Release build to collect ordinary Capsule/Sword static-export process wall times. tools/benchmark-static-exports.md documents hashes, diagnostic receipts and the limits: this is not peak-memory/GPU/large-model or full M8 acceptance.

## Generator-free animation metadata

The C++17 core compiles `dust3d/animation/animation_catalog.h` and the additional
biped declaration metadata without linking animation generators. `clipCatalog()`
returns the inherited 180 types (176 current choices plus four legacy aliases),
including rig/generator identity, duration, sample count, loop flag and numeric
default overrides. `standardBones(rig)` retains case-sensitive lookup and an empty
result for unknown rigs. References are immutable and valid until static shutdown;
C++ construction errors propagate to the C++ caller. This is not yet a C ABI or
Swift catalog API, and it does not establish native sampling/playback acceptance.

```cpp
#include <dust3d/animation/animation_catalog.h>
const auto& catalog = dust3d::animation::clipCatalog();
auto found = catalog.find("BipedWalk");
if (found != catalog.end()) {
    const double seconds = found->second.duration;
    const int samples = found->second.samples;
    // Metadata only; no generator is invoked.
}
```

See [catalog verification](docs/animation-catalog-metadata-verification.md) for
inventory/provenance and [ADR 0098](docs/adr/0098-generator-free-animation-catalog.md)
for the shared declaration design. Per-parameter editor descriptors, C/Swift
owned-copy adapters, Qt/native interoperability and runtime acceptance remain open
under [Apple #145](https://github.com/dalbrecht/dust3d-swiftui/issues/145). Core #95
and all other quarantines and the Apple production pin remain unchanged.
