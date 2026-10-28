# Native editor implementation and repository plan

Status: active repository setup and proposed editor delivery plan, October 14, 2026. The viewer PR is the first independent deliverable; editor work begins after it lands. Estimates below are sequencing guidance, not a delivery commitment.

## Product boundary

Keep `Dust3DViewer` as a separate read-only macOS app with its own executable, bundle identifier, launch/package command, and release artifacts. Add `Dust3DEditor` as a different app, initially macOS 14+, Swift 6, SwiftUI and Metal. Share document/engine services and reusable rendering code; never make opening a model in the viewer start an editor session. Preserve the Qt app as a compatibility oracle during the port. iOS, cloud collaboration, and new modeling features are outside parity scope.

Full implementation means native support for the existing repository's document, modeling, material, rigging, animation, sound and export workflows. Feature names alone are insufficient: establish a fixture and acceptance case for each behavior before calling it complete. The current viewer supports procedural mesh previews only and is not an editable document model.

## Repository structure and ownership

**Selected end state: two public MIT repositories.** Keep [`dalbrecht/dust3d-swiftui`](https://github.com/dalbrecht/dust3d-swiftui) for Apple apps, and use [`dalbrecht/dust3d-core`](https://github.com/dalbrecht/dust3d-core) for portable C++ and its native C API. The user authorized repository setup on October 14, 2026. The core preserves the existing Git history and tracks [`huxingyi/dust3d`](https://github.com/huxingyi/dust3d); it is a maintained downstream, not a decision to stop integrating upstream. The Apple repository remains the existing GitHub fork and can be used to submit focused upstream contributions. The new core repository is a history-preserving duplicate with an explicit upstream remote and tracking branch, rather than a second GitHub-network fork. Preserve the original MIT copyright/license and all third-party notices; new project code uses MIT. Third-party components retain their existing licenses, not an assumed MIT relicensing.

| Repository | Owns | Must not own |
| --- | --- | --- |
| Apple apps (`dust3d-swiftui`) | Swift document/application layers, viewer and editor executables, SwiftUI components, Metal renderer/shaders, Apple packaging, UI tests | Geometry algorithms, duplicate vendored C++ dependencies, Qt controllers |
| Core fork (proposed) | `dust3d/`, required geometry dependencies, validated archive/document adapters, versioned C ABI, headless build/CLI, C++ tests/fuzzers and upstream integration | SwiftUI/AppKit, app state, Metal UI, Qt-dependent public headers |

Repository infrastructure is established now; migrate app consumption only after the core contract and editable-document requirements are proven. The viewer PR stays independently landable with its current in-tree engine. Until the Apple dependency migration lands, the viewer branch is the source for its adapter; after migration, the core becomes the single source of truth. Cross-repository changes use paired PRs and exact commit pins, never unrecorded copies. Organize native code along these boundaries before moving it:

```text
native/Engine/                    # future core adapter; remains Qt-free
Sources/Dust3DKit/                 # typed Swift engine/document adapter
Sources/Dust3DDocument/            # pure editor state and commands
Sources/Dust3DApplication/         # generation, persistence, undo coordination
Sources/Dust3DRendering/           # shared Metal rendering and picking
Sources/Dust3DViewer/              # standalone read-only app
Sources/Dust3DEditor/              # document scenes, tools, inspectors
Tests/<matching target>/          # behavioral tests
Tests/Compatibility/              # licensed archive and parity corpus
docs/adr/                        # architecture and compatibility decisions
```

These are target boundaries, not instructions to add empty modules. Extract rendering when the editor first needs it; add other targets when they gain real responsibilities. Keep `Package.swift` as the authoritative dependency/build declaration. Separate app packaging scripts and bundle identities. Put legacy Qt code under a documented reference boundary; avoid moving inherited trees merely for cosmetic consistency.

### Split tradeoffs and migration gates

Two repos improve upstream merges, portable CI and release ownership, but introduce coordinated versioning, cross-repo changes and binary supply-chain work. A single repo is simpler while editable snapshot and asset APIs are changing. Do not copy the engine into a second repo and independently patch both.

1. Finish viewer landing and establish editor load/save requirements (M0–M1). Setup verified both the Apple fork master and upstream HEAD at `3fbcaa7a3e8792d1c2ca31ca2ade442dcd21b222`; preserve that provenance and re-audit divergence on each synchronization. The core starts from that ancestry plus the native adapter, with no rewrite of inherited history.
2. Give the engine a standalone CMake build and headless contract tests in addition to its current SwiftPM build. Use explicit source/dependency lists. Demonstrate a clean build with no Apple or Qt frameworks.
3. Version the C API: capability query, ABI version, sized structs, explicit ownership/destruction, error/status contracts, and document/mesh/asset handles. Keep STL, Qt types and exceptions private. Capture limits and cancellation semantics in public documentation.
4. Maintain the history-preserving core repository and all dependency license notices. Keep upstream geometry changes separate from native-adapter changes where possible. Resolve inherited warning fixes through focused C++ PRs rather than whole-tree formatting.
5. Start with an exact pinned source revision integrated by a documented submodule/build adapter. SwiftPM cannot assume a C/C++ target can import source files from outside its package; prove the layout on a clean clone, or build/link the standalone core through a generated local artifact. Do not use a moving branch or developer-specific search path.
6. When the ABI stabilizes, publish checksummed static XCFramework releases for arm64 and x86_64 macOS with headers/module map, symbols and licenses. Consume a versioned SwiftPM binary target from the Apple repo. Support a documented local-source override for engine development. Test deployment-target and Swift toolchain compatibility, and package transitive C++ dependencies without symbol duplication.
7. Run contract fixtures against every candidate core release, then update the Apple dependency in its own PR. Upgrade viewer and editor together only after both pass. Retain the last compatible engine release for rollback.

Each repo gets its own pinned coding-standards configuration. The core inherits the existing C++17 `.clang-format`, `.cc`/`.h` conventions and project C++ supplement. The Apple repo inherits Swift 6/concurrency and UI standards. The existing standards audit identifies missing upstream C++ rules and native-boundary guidance; propose those upstream separately. CI must measure new Swift domain coverage and instrument new C++ adapter code explicitly; the current Swift coverage does not measure the inherited engine. Document justified legacy/manual exceptions instead of advertising a false whole-engine coverage gate.

## Architecture

Use value-based editable document state with stable UUIDs, ordered component hierarchy, node/edge graph, materials, reference images, imported assets, rig definitions, animations and sound parameters. Preserve unknown attributes and archive items so unsupported future fields survive save. Separate semantic document state from selection, camera, window layout and generation caches.

Commands validate invariants and produce the next state plus an undo record. A drag is one undo transaction, not one record per pointer event. The main actor owns scene presentation and command routing; actors own file access and generation scheduling. Never place geometry generation, texture baking or export on the main actor. Every derived result carries its source revision, and stale results cannot replace newer state or be exported as current.

Extend the engine with narrow capabilities: decode/encode document, generate indexed geometry with source IDs and UVs, generate textures, import/export assets, generate rigs and evaluate animations. Swift owns app workflows; portable algorithms stay C++. Investigate Qt-dependent services individually rather than translating `QObject`/signals into the engine. Retain serialized native entry points until globals and UUID generation are audited. Add cooperative cancellation only at safe algorithm boundaries; evaluate a worker process for hard time/memory limits and crash containment before general distribution.

## Existing feature inventory and parity acceptance

Source anchors are repository-relative references for implementation planning. `application/sources/document.h` and the `document_{node,edge,part,component}.cc` files define editing semantics; `document_window.cc` defines file/menu/export workflows. Core algorithms remain under `dust3d/`.

| Area | Required native behavior | Acceptance evidence |
| --- | --- | --- |
| Documents | New/open/save/save as, dirty state, reopen, multi-window, recent files, atomic saves, autosave/recovery, duplicate and asset retention | Qt→native→Qt fixtures; failure cannot corrupt original; cancel/close recovery cases |
| Graph editing | Add/connect/delete nodes and edges, reverse edges, move/scale/radius, selection, copy/cut/paste, undo/redo, front/side profile and reference turnaround images | Command invariant tests; continuous gesture undo; keyboard and pointer workflows |
| Constraints | X/Y/Z/radius locks; part visibility/lock/disable; origin handling and mirror behavior | Locked entities unchanged; mirrored results match fixtures |
| Parts/components | Naming, ordered hierarchy, grouping/reordering, selection previews, combine modes, targets, segment counts and smoothing cutoff | Hierarchy round trips; engine parity on unions/differences and ordered combinations |
| Modeling modifiers | Subdivision, interpolation, hard joins, rounding/chamfer, width/thickness/unified deformation, cut faces/rotation/linked profiles, hollow thickness, loop fill, close side/front/back including depth/sharpness, wrapping | One focused fixture per modifier plus interacting combinations; tolerance-based geometry comparisons |
| Materials/textures | Component colors/images, transparency, metalness, roughness, emissive, countershade, procedural detail, UV generation and baked normal/AO/material maps | UV/index integrity; linear/sRGB correctness; deterministic bake comparisons and reference renders |
| Imports | Existing embedded GLB assets and their transforms/materials; reference image formats actually supported by Qt | Asset preserved on save; malformed/unsupported inputs fail explicitly; no partial model presented as complete |
| Rigging | Existing rig types, eyelid option, bone hierarchy/properties, weights, skeleton display and IK manipulation | Joint/weight invariants, rest-pose parity, known deformation fixtures |
| Animation/audio | Animation list/name/type/parameters, generation, preview, timeline/playback and existing procedural sound/WAV workflows | Stable duration/time units; pose fixtures; audio length/format; cancellation and export diagnostics |
| Export | OBJ, GLB, FBX, GLB/FBX with WAVs; static and rigged/animated paths; headless/batch behavior | Re-import into independent consumers; topology, UV/material, skeleton and animation assertions; failed writes leave no claimed success |
| Desktop experience | Menus/shortcuts, tools and inspectors, progress/errors, drag/drop, camera modes, wireframe, fit, focus/accessibility, settings | Native UI scenarios, keyboard-only operation, VoiceOver, large-document interaction checks |

Do not assume every available core algorithm is exposed by the current Qt app. During M0 enumerate actual menu actions, serialization fields and animation/sound types into a tracked parity checklist. Include command-line/agent modeling tools as file producers; preserve their existing `.ds3` interoperability.

## Delivery milestones and PR sequence

Each milestone produces independently reviewable PRs and keeps the viewer usable. Start with a compatibility corpus before broad UI construction. No milestone is complete solely because its screens exist.

### M0 — Viewer landing and parity baseline

- Land the standalone viewer PR; preserve its product identity and documented procedural-preview limits.
- Inventory Qt actions and serialized attributes, mark each supported/preserved/rejected, and create issues for the following milestones. Record feature changes already present in this fork versus upstream Dust3D.
- Build fixtures for modifiers, textures, embedded imports, rigs, animations/audio, malformed archives and real user documents with permission. Capture Qt numerical/export output and reference renders; avoid byte-exact mesh comparisons when ordering is incidental.
- Done: agreed parity matrix and fixtures; viewer CI passes; first editor scope is graph editing and lossless save, without unsupported feature loss.

### M1 — Editable document and safe persistence

Dependencies: M0. Suggested PRs: document schema/codec; commands/invariants; persistence/recovery.

- Add a decode API that returns editable data and retained archive assets, rather than only generated vertices. Add an encoder with validated offsets and checked size arithmetic; audit the inherited writer's deprecated formatting and error handling.
- Model stable IDs, hierarchy order, unknown fields and assets. Define migration/version policy. Preserve features before their editing UI exists; prohibit a destructive save if preservation is impossible.
- Introduce pure add/remove/move/radius/connect commands, undo transactions and typed errors. Wrap atomic writes, file coordination and recovery in a thin file service.
- Done: document→save→reopen semantic equivalence; Qt loads native-saved fixtures; unknown assets preserved; invalid commands leave state unchanged; disk-full/interrupted-write/close-cancel cases tested.

### M2 — Separate editor app and interactive canvas

Dependencies: M1. Suggested PRs: editor scene/shared rendering; selection/picking; graph tools/undo UI.

- Add the editor executable and separate bundle ID; extract shared renderer without moving viewer state into editor modules. Use document scenes with independent state/undo per window.
- Add front/side orthographic editing, 3D preview, reference image placement, node/edge overlays, hit testing, marquee/multiselect and coordinate transforms.
- Implement drawing/connection, selection, move/scale/radius, delete, clipboard with UUID remapping, constraints and keyboard commands. Debounce preview generation, retain last valid mesh with explicit stale/error state.
- Done: create a capsule and multi-part sword, edit both, undo/redo, save/reopen in Qt; two windows do not share selections or undo; gestures remain responsive while generating.

### M3 — Complete procedural modeling

Dependencies: M2. Suggested PRs grouped by component hierarchy, modifier families and generation scheduling.

- Implement every part/component property in the parity matrix with validated ranges, lock behavior and undo. Add ordered component tree, inspector selection, previews and grouping/reordering.
- Expose generated source IDs and indexed geometry; implement component preview caches keyed by semantic revision. Audit graph assumptions: the current viewer rejects branching/disconnected chains, so do not loosen validation without proving generator support or defining explicit editing diagnostics.
- Add incremental regeneration only after correctness is established. Bound cache memory; discard obsolete work and surface generation failures.
- Done: modifier corpus passes geometry/bounds/normal checks and agreed Qt tolerances; editing combinations round-trip; no unsupported property silently resets.

### M4 — UVs, materials, textures and embedded models

Dependencies: M3, with import preservation already required in M1. Suggested PRs: asset service/import; UV/material ABI; CPU bake extraction; PBR renderer/inspectors.

- Isolate Qt image/resource dependencies from `mesh_generator`, `uv_map_generator` and related services. Prefer portable pixel buffers in C++ and ImageIO/CoreGraphics only at Apple decoding boundaries; preserve existing bake algorithms where possible.
- Expose indexed vertices, UVs, material groups and texture buffers with explicit pixel format/color space/ownership. Implement PBR shading, transparent material ordering, normal/AO/emissive maps and image material controls.
- Port embedded GLB loading and transforms with bounded parsing; decide supported extensions and handling of unsupported ones in an ADR.
- Done: texture/import fixtures render and export correctly; UV seams/material grouping retained; all archive assets survive; malformed images/GLB cannot crash the app.

### M5 — Rigging and skeletal editing

Dependencies: M3; material/export integration follows M4. Suggested PRs: rig adapter; skeleton overlay/bone inspector; IK/weights.

- Extract portable work from `rig_generator_worker`, `rig_skeleton_mesh_generator`, bone structures and `skeleton_ik_mover`. Model rig parameters and bone edits without Qt types.
- Add skeleton visibility/picking, hierarchy/properties, rig generation progress, rest pose and deformation preview. Define coordinate/axis and joint index conventions once across renderer/exporters.
- Done: valid parent graph and normalized weights; expected rest pose; fixture deformation parity; undoable rig edits persist; failures retain recoverable document data.

### M6 — Animation and procedural audio

Dependencies: M5. Suggested PRs: evaluation APIs; animation management/playback; sound generation and synchronization.

- Add required `dust3d/animation` and `dust3d/sound` code as explicit Qt-free targets; they are not compiled by the current viewer package. Inventory existing animation types/parameters rather than assuming a general keyframe editor.
- Port preview/export worker orchestration into actors. Add animation list and parameter inspectors, playback/time controls, skeleton skinning and audio preview where supported by the reference app.
- Done: sampled poses and durations match fixtures, playback independent of frame rate, sound exports have correct format/duration, stale generation and cancellation do not publish wrong animations.

### M7 — Export and automation parity

Dependencies: M4 for static exports; M5–M6 for animated exports. Static OBJ/GLB can land earlier.

- Extract Qt-independent writers from `glb_file` and `fbx_file`; keep licensing and axis/unit conventions explicit. Provide OBJ, GLB, FBX and model-plus-WAV workflows with asynchronous progress and atomic output handling.
- Add a headless core CLI and/or Apple command entry point compatible with current batch workflows. Share export use cases with UI; never export a stale mesh or report a failed/empty export as success.
- Done: independent re-import verifies geometry, UVs, materials, bones and animations; batch exit codes and diagnostics tested; partial multi-file failures are recoverable and disclosed.

### M8 — Hardening, distribution and parity release

Dependencies: all parity milestones. Split into focused performance, security and release PRs.

- Benchmark corpus load/edit/generate/bake/export latency, peak memory and interaction frame time on Apple Silicon and supported Intel hardware. Establish measured budgets before optimization; profile expensive algorithms and upload/copy costs.
- Instrument new C++ adapters with coverage and ASan/UBSan; fuzz archive/XML/asset parsing and command sequences. Resolve legacy warnings in narrow commits. Evaluate isolated engine worker and hard cancellation/resource limits for untrusted documents.
- Complete keyboard/VoiceOver, appearance, multi-window recovery, file associations and settings checks. Add signed release builds, entitlements/sandbox decision, notarization, versioning, reproducible packaging and upgrade tests.
- Done: no unresolved blocking parity defects; all automated gates green; manual GPU/export/accessibility matrix recorded; editor and viewer packaged separately. Viewer gains compatible shared improvements without acquiring editing behavior.

## First editor PR after viewer merge

Implement M1's document codec and immutable state with lossless round-trip fixtures, then the minimal command/undo layer. Keep the renderer unchanged in that PR. The first user-visible editor PR follows with a separate app, node movement/radius editing, undo and save. This prevents a convincing canvas from hiding a lossy persistence model.

## Verification and risk controls

- New pure Swift domain: 100% meaningful coverage; application/infrastructure: at least 95%, with thin native/manual boundaries disclosed. Add a real CI threshold checker rather than relying on the current coverage report alone. C++ adapter coverage is a separate instrumented job; inherited algorithm coverage is a tracked adoption gap.
- Keep formatting, Swift warning errors, explicit C++17 rules, Qt source/link checks and clean-clone builds in both applicable repositories. Test ABI ownership, size arithmetic, malformed inputs and long-running generation. No artificial hooks just to increase coverage.
- Use semantic equality for archives, tolerance-based geometric assertions and independent format readers. Maintain rendered reference baselines on controlled CI machines with human review; do not treat local GPU pixels as portable golden files.
- Biggest risks: lossy snapshot/asset saves, Qt-bound texture/export utilities, fork-specific animation/audio behavior, engine global state, expensive uncancellable generation, and API/version drift after a repo split. Resolve each with an early fixture or spike before expanding its UI.
- Decisions still needed before release: engine fork name/ownership, supported Intel/macOS matrix, exact rig/animation parity list, sandbox/distribution channel and representative large-document budgets. These do not block the initial document/command work.

## Release ordering

1. Review and land the standalone viewer PR; do not merge it automatically.
2. Open document/command foundation PRs against the landed branch and track the parity checklist as issues.
3. Switch Apple app consumption to the established core repository only after standalone build and document/API gates pass; publish engine versions through reviewed releases.
4. Ship editor previews by completed milestone with explicit unsupported-feature behavior. Declare full parity only after M8 acceptance, not after the initial editing UI.

## Upstream relationship and synchronization

The core default branch is `master`; `upstream-sync` contains only upstream commits. Retain upstream file paths and keep new adapters/build/tests separate. The legacy Qt tree remains reference material and is excluded from the core build; deleting it is unnecessary and would increase synchronization conflicts.

Configure `origin` to the core repository and `upstream` to `https://github.com/huxingyi/dust3d.git`. Advance `upstream-sync` by fast-forward only after fetching upstream master. Open a `codex/sync-upstream-*` PR that merges that exact tracking commit into core master. Inspect changes to serialization, vendored dependencies, geometry and licenses; run the compatibility corpus and sanitizer matrix before approval. Never auto-reset maintained master to upstream or force-push a synchronization branch. Record the upstream revision and release contract in each core release. Where a portable fix is suitable, propose a focused PR through the existing GitHub fork; upstream acceptance is optional and must not be assumed.

A sync workflow may check and open a PR when explicitly enabled, but setup does not schedule unattended synchronization or contact upstream maintainers. Check upstream during maintenance/release work. Signed/checksummed core releases and exact dependency pins keep Apple releases reproducible.

## Rigorous core testing program

Yes: separating the portable engine allows tests to run without SwiftUI, Metal, Qt or a display server, independently of Apple app release schedules. Repository separation enables this; the tests and gates still have to be implemented and maintained.

Initial setup provides a standalone CMake/CTest build and headless C API contract tests. CI covers Linux and macOS, with separate address/undefined-behavior sanitizer coverage on Linux. This is a foundation, not a claim of complete algorithm coverage.

Expand in this order:

1. **Trust boundary:** archive offsets/lengths/depth, invalid identifiers/references, nonfinite geometry, unsupported assets, ownership/lifetimes, bounded diagnostics and failure state. Add regression fixtures for every discovered defect.
2. **Document fidelity:** semantic load/save equivalence, unknown fields/assets, version compatibility, malformed inputs, independent Python-produced archives and Qt round trips.
3. **Geometry properties:** finite vertices/unit normals, valid indices, nondegenerate triangles where appropriate, manifold/watertight properties when required by a feature, stable bounds, modifier combinations and permutation/orientation cases. Compare semantics and tolerances rather than incidental vertex order.
4. **Differential corpus:** run known models through upstream and downstream builds; classify intentional differences and fail unexplained regressions. Preserve the upstream executable/version used for each baseline. Add representative real documents with appropriate permission and licensing.
5. **Dynamic analysis:** ASan/UBSan on new and inherited execution paths, leak checks, and TSan once concurrency semantics are implemented. Serialization currently prevents concurrent generation; test concurrent callers without assuming algorithms are internally thread-safe.
6. **Fuzzing:** libFuzzer targets for archive/XML/asset decoding and command sequences, seeded with minimized corpus cases. Use short PR smoke budgets and longer explicitly configured nightly runs; retain crash reproducers as regression tests. Do not add nightly schedules without authorization.
7. **Coverage:** LLVM branch/line reports separated into new adapter and inherited geometry. Establish the inherited baseline first; enforce at least 95% meaningful new adapter coverage and 100% pure validation/domain coverage where testable, with documented boundary exceptions. Block changed-code regressions and track legacy coverage improvement; never claim Swift XCTest coverage measures C++.
8. **Resource/performance:** fixed benchmark corpus, wall time, peak RSS, output size, cancellation and stress tests. Establish budgets on controlled runners; investigate significant changes before shipping. Limits in the current parser do not bound CPU/memory inside geometry generation.
9. **ABI/release compatibility:** compile a pure-C consumer, verify symbols/header layout and ownership against supported released clients, test clean installs, no Qt linkage, required license notices, and both Apple apps against each candidate engine release.

All core dependency upgrades and upstream-sync PRs use the same tests. Keep fixture metadata (source, license, upstream revision, expected behavior, tolerance) next to fixtures. Review intentional baseline changes; do not automatically bless newly generated output. Core release readiness requires documented sanitizer, compatibility and ownership results, not just a successful build.


### Native invalidation foundation checkpoint (core #19)

An internal portable snapshot comparator now derives changed part/component
identities and dependency closure without trusting saved dirty flags. See
[verification](snapshot-invalidation-verification.md) and ADR 0005. It preserves
input snapshots and unrelated component caches conservatively; persistent cache
ownership/budgets, assets, mirror eviction and actual session integration remain
pending. This does not complete semantic component caching or M3 acceptance.


### Cache ownership accounting checkpoint (core #21)

Portable C++ ownership visitors now account retained object/value/buffer payloads,
reserved capacities and nested cache families without allocating or double-counting
borrowed geometry. See [verification](retained-payload-verification.md) and ADR 0006.
Allocator bookkeeping and transient/process memory are outside this metric.
Persistent sessions, eviction/failure recovery and actual cached-versus-fresh
acceptance remain pending; this does not complete component caching or M3.


### Native generation session checkpoint (core #23)

An optional C session integrates semantic invalidation, owned asset revisions and
retained-payload accounting with actual inherited component reuse. Mesh owners
remain independent, cache-only failures degrade to fresh generation with their
original diagnostic, and primary failures release partial cache state. See ADR
0007. Swift actor integration, obsolete-work scheduling, source provenance,
controlled benchmarks and the Qt differential acceptance corpus remain pending.
This does not complete M3 or establish full editor feature parity.


## Core source-hint stage (core #25)

The opt-in source-owner implementation captures original opaque node/part keys,
retains all wrapping weights and explicit unknown/ambiguous outcomes, and aligns
per-corner associations after float filtering and indexed conversion. Design and
limits: [ADR 0008](adr/0008-generated-source-hints.md) and
[verification](source-provenance-verification.md). It does not claim exact Boolean
provenance. Cached source export and Swift/editor adoption follow after full core
gates and review; M3 and broader differential acceptance remain incomplete.


## Cached core source-hint stage (core #27)

The sourced-session API reuses the existing bounded component caches while exporting
current per-call original identities and weighted corner hints. Source and cache
budgets/diagnostics remain independent, including cache-only fallback and primary
source-failure recovery. See [ADR 0009](adr/0009-cached-source-generation.md) and
[verification](cached-source-verification.md). Swift/editor pin and adoption follow
after core review and gates; M3 and broader Qt acceptance remain incomplete.


## UV packer safety prerequisite (core #30)

Portable atlas extraction exposed inherited signed area overflow under UBSan.
The focused generic fix widens scores/products, checks quantized dimensions and
rejects precision-erased charts; see [verification](uv-packer-numeric-verification.md)
and ADR 0010. Atlas construction (core #29), texture baking, Apple adoption and
full M4/native/Qt acceptance remain incomplete.


## Portable UV atlas metadata (core #29)

The C++ atlas builder now replaces Qt/global-image orchestration with explicit
resolved component inputs and owned layouts, corner UVs, mapping masks and
ordinal gradient descriptors. Cyclic corner order retains seam priority; missing
charts stay explicit. See [verification](uv-atlas-verification.md) and ADR 0011.
This stage depends on reviewed numeric packer safety. C ABI original-key and
triangle-ordinal mapping, Apple pin/adoption, baking, PBR/transparency, asset
imports and full M4/Qt acceptance remain incomplete. M3 native acceptance also
remains required; this extraction does not waive it.


## Owned UV C boundary (core #33)

Opt-in C owners expose retained atlas/C metadata and original component keys
without changing ordinary preview loads. UV publication follows the actual float
triangle filter; aliases/generated identities and missing charts stay explicit.
Caller-resolved image dimensions and strict source colors avoid Qt registries.
See [verification](uv-metadata-verification.md) and ADR 0012. Apple exact-pin/SDK
and editor adoption, pixel baking/PBR/transparency and full M3/M4/Qt acceptance
remain separate gates. Existing indexed geometry sharing does not include UVs;
textured indexed output must incorporate seams/materials before rendering.


## Offscreen bake reference probes (core #35)

An optional standalone Qt Core/Gui harness compiles the preserved application bake
sources unmodified, with matching/fresh native archives. Analytic probes cover
image transposition, seam/material/color/alpha and actual Spots/fold-normal work.
On Capsule/Sword/RoundedCrossing the original-object portable corner UVs match
reference orchestration with maximum observed error zero; RoundedCrossing's 128
zero-UV records remain shared limitations. See
[verification and scope](qt-bake-reference-verification.md) and ADR 0013.
Native dependencies/default CI stay Qt-free. This does not waive missing mapping,
portable CPU pixels, Apple image decoding/renderer, full Qt file/generator or
broader M3/M4 acceptance. No local PNG snapshot baseline is committed.

## Portable CPU texture baker (core #37, merged in #38)

The C++ baker now exercises actual color/image/seam/material outputs and inherited
world-space pattern/fold kernels with independently owned pixels. The focused
contracts include generated Capsule, Sword and RoundedCrossing bakes, explicit
unknown tangent markers, deterministic output, invalid input/quota/cancellation
and lifetime checks. ADR 0014 records the ownership and raster design. Six optional Qt interior pixel comparisons pass within one byte, with exact
image/scalar/normal samples; straight-alpha source opacity is an intentional
difference from layered Qt painting. PR #38 landed after clean exact-commit automated review and all six CI checks.
This checkpoint does not count M4 as complete or imply Apple renderer adoption.


## Owned C/Swift bake output (core #39, in progress)

The opt-in baked owner retains pixels and float-filtered tangent records with its
borrowed UV/geometry owner. Sized input views, independent quotas and cooperative
cancellation have actual C/C++ contracts and strict Swift copy consumers. See
[verification](baked-mesh-verification.md) and ADR 0015. Publishing, automated
review and CI remain required before landing. Subsequent Apple exact-pin/SDK
integration must decode retained assets, preserve cancellation lifetime and feed
UV/tangent/material seams into rendering. PBR/transparency/AO, textured exports,
full M3/M4 and all later milestone acceptance remain open.


## Embedded-model geometry stage (core #41, in progress)

The reviewed bounded decoder now feeds a primary archive resolver and the actual
imported-model generator in the geometry-preview C APIs. Required failures are
separate from optional caches, decoded owners have an aggregate quota, and
same-UUID asset edits invalidate generated reuse. See ADR 0017 and
[verification](imported-model-generation-verification.md). This stage discloses
unapplied materials and retains UV/bake rejection. C/Swift material/image adoption,
reviewed Apple pinning, native lossless editing/file previews and textured exports
remain required; neither #41 nor M4 is complete.


The geometry-stage review correction retains exact triangle ownership through
Boolean cutting, recombination and cached copies before applying imported colors
and normals. Coincident imported parts, colorless component fallback, nested
group previews and public cached loads have dedicated regressions. Newly created
seam faces use computed fallback attributes; this does not establish full GLB
material transfer or close M4. Fresh exact-head review and hosted CI remain
required for core #43 before the next dependency stage.


Further geometry-stage review contracts preserve vertex-color seams through
cached corner attributes, node-level directional deformation and its normal
Jacobian, and component-scoped original source hints. Ownership retains opaque
original component keys so accepted brace aliases do not collide. These remain
geometry-stage acceptance; embedded materials, native asset adoption, rendered
file previews and full M4 acceptance are still open.


### Embedded geometry review hardening checkpoint

Core #43 now rejects unsafe position quantization with a typed core failure,
including extreme combined imported deformation and session recovery, and retains
per-component fallback colors when multiple imported instances share a part. Both
regressions failed before their fixes. Full Debug and ASan/UBSan suites pass
38/38 locally; SwiftPM and formatting/Qt boundary checks pass. Exact-head review
and hosted checks remain required before merging. These corrections do not
complete imported materials, Apple adoption, file previews or M4 acceptance.


### Owned imported-material inspection stage

Core #43 is merged at 030074d after exact-head automated review and all six hosted
checks passed. Geometry-preview acceptance does not close #41/M4. Core #46 now
implements an independent C material/image owner with original instance bindings,
explicit sampler/alpha factors, untrusted encoded storage and an output quota.
Empty material sets do not relax geometry-loading or graph-reference validation.
See ADR 0018 and imported-asset ownership verification. Full local Debug and
ASan/UBSan suites pass 39/39; SwiftPM and formatting/Qt checks pass. Boundary-only
coverage is 98.69% lines, 100% functions and 100% measured branches, with the
defensive catch-all exception handler uncovered. Automated review and hosted CI
remain required; Swift/ImageIO, imported UV/bake/PBR integration, the Apple pin,
file previews and textured export acceptance are still required.
### Local chart-isolated mip stage (#59)

The portable baker now has an opt-in owned mip chain with separate imported and
procedural-island ownership, actual per-map center/collar LOD admission, odd-area
linear-light filtering, independent straight alpha, scalar/normal semantics and
cumulative retained/work limits. Original procedural layouts are explicitly
borrowed for the call; absent metadata preserves LOD zero. See
[ADR 0060](adr/0060-chart-isolated-owned-mip-chains.md) for regression evidence and
validation limits. Prepared locally after #104; public review and serial delivery
remain pending behind core #102. Native metadata/ownership, Swift copies, renderer
adoption, textured export/re-import and broad M4 acceptance remain required.

### Native isolated mip ownership (#59)

An opt-in source-matched C loader now retains chart-isolated mip levels under the
native aggregate output quota. Procedural atlas layouts survive until portable
mip generation completes. Per-map counts and borrowed views expose actual levels;
the old loader remains base-only. Trusted imported/procedural fixture contracts
cover ownership, quotas, cancellation and geometry/UV/policy alignment. See
[ADR 0061](adr/0061-native-owned-atlas-mips.md). Swift copying, renderer partial-chain
sampling, textured export/re-import and independent M4 acceptance remain pending.
No production dependency pin changes in this stage.

### Default stitching output containment (#101)

Remove the private automatic Debug SVG writer while retaining connected-loop
geometry. A behavioral regression preserves the existence/time/bytes of the
global diagnostic path; ordinary normal and sanitizer suites pass. See
[verification](stitching-output-containment-verification.md). This is filesystem
containment preparation, not a worker sandbox or full M8 acceptance. The Apple
production pin and every existing quarantine remain unchanged.

### Portable worker framing foundation (#109)

A separate C++17 frame codec now validates versioned fixed-endian envelopes,
per-kind wire limits, exact spans and response identity. No process or engine is
launched by this library. See [ADR 0062](adr/0062-portable-worker-framing.md) for
measured tests/coverage and wire semantics. Semantic result codecs, parent queue
and revision retirement, worker/transport, cancellation and resource/exit/sandbox
acceptance remain required. This does not remove quarantines, advance an Apple
pin, establish containment or close M8/full parity.

### Worker base-mesh semantic codec (#110)

A separate version-one payload codec validates the existing untextured triangle
result, bounded source counts and warnings, with exact spans, finite/unit/range/area
checks and independently owned output. The licensed Capsule passes native-owner
release, framing/identity checks and bit-exact semantic transport. See
[ADR 0063](adr/0063-worker-mesh-payloads.md). Failure schemas, pending-state retirement,
worker transport/resource isolation, source-aware UV/material/rig/animation results
and full release acceptance remain pending. No production pin or quarantine change.

### Typed worker non-success payloads (#111)

A separate failure schema preserves all six existing non-success outcomes and
bounded original diagnostic bytes without assuming UTF-8, logging or replacing
causes. Real ordinary native-error framing/identity/ownership contracts pass. See
[ADR 0064](adr/0064-worker-failure-payloads.md). The next stage must exercise these
codecs in a real single-job helper and parent transport; queue/time/resource/exit
admission, cancellation, parent-death and sandbox/source-aware acceptance remain
required. No process containment, production pin change or full parity is claimed.

### Working single-job native helper (#112)

A real Unix helper now executes one admitted archive request and emits an owned
base-mesh or typed native failure, with complete-input EOF policy and binary output
separated from backend diagnostics. A portable operation adapter provides explicit
status mapping and validated consumer budgets. Fifteen ordinary subprocess cases
include Capsule/Sword parity, actual seam diagnostics and real read-error recovery.
See [ADR 0065](adr/0065-single-job-native-worker.md) for measured gates, coverage
gaps and exit semantics. Parent queue/revision/exit admission and transport, hard
resource/time/cancellation/sandbox/parent-death acceptance, source-aware results
and distribution remain required. This does not close M4/M7/M8 or change a pin or
quarantine.

### Parent response admission (#113)

A single-use portable pending owner retires every completion before parsing and
requires observed normal exit zero, active document/request/revision identity,
exact framing and semantic mesh/failure validation. Existing real subprocess
verification uses it. See [ADR 0066](adr/0066-worker-response-admission.md). Actual
pipe collection, queue/revision orchestration, deadlines/resources/sandbox and app
adoption remain required; production pin and quarantines are unchanged.

### Real Unix parent transport (#114)

A CMake development library directly spawns the trusted helper, concurrently sends
and drains bounded Unix channels, captures original diagnostic bytes and observes
termination. Actual licensed mesh and ordinary failure replies pass the existing
single-use admission gate. See [ADR 0067](adr/0067-unix-worker-transport.md). Queue
orchestration, deadline/termination/resources/sandbox and application adoption
remain incomplete. Production pins and quarantines are unchanged.


### Bounded worker request scheduling (#115)

Portable scheduling now connects one active header and one latest waiting archive
to the existing reply admission gate. Monotonic request identity, cancellation,
close, malformed/replayed/foreign completion rejection and transport-failure
retirement have pure tests and ordinary real Capsule worker integration. Normal
61/61 and sanitizer60/60 contracts pass. See [ADR0068](adr/0068-bounded-worker-scheduling.md).
This is externally serialized scheduling state; application I/O orchestration,
deadlines, hard cancellation, sandbox/resource/parent-death policy and production
adoption remain required. No production pin or quarantine changes; M8 remains open.

### Pure worker lifetime decisions (#123)

A local WorkerLifetime stage validates explicit elapsed-time budgets, retires
cancelled/late results and emits one stop/escalation/channel-close decision as
appropriate. It performs no process or signal action and does not change the
transport's blocking cleanup. Normal 62/62 contracts and scoped policy sanitizers
pass; [ADR 0069](adr/0069-worker-lifetime-policy.md) records full scoped policy
coverage and the missing OS/resource/sandbox/parent-death acceptance. M8, Apple
adoption and full parity remain open; no production pin changes.

### Opt-in supervised worker operation (#123)

The local supervisor integrates lifetime decisions with Unix collection while
preserving the legacy entry point. Ordinary supervised Capsule and pre-start
cancellation pass normal/sanitizer contracts; active OS stop/escalation remains
unforced under current restrictions. [ADR 0070](adr/0070-supervised-unix-worker.md)
records measured coverage gaps, exclusive reaping ownership, possible uninterruptible
OS reap, and missing resource/sandbox/parent-death containment. This does not close
#123/M8, enable production documents or change the Apple pin.

### Helper resource-policy implementation contract (#123)

[ADR 0071](adr/0071-helper-resource-policy.md) specifies a helper-only resource
adapter, pure profile validation, effective inherited-limit handling and fail-closed
setup errors before archive work. It separates virtual address limits from resident
memory, requires explicit sanitizer/profile compatibility and identifies descriptor
inheritance and launch-version gates. This is a proposed contract only: no resource
caps are installed, no production profile is selected, and no exhaustion or signal
probe is authorized. Filesystem/descendant/parent-death containment, active OS
lifecycle evidence and Apple adoption remain open; M8 is not complete.


### Pure worker resource-profile planner (#123)

[ADR 0072](adr/0072-worker-resource-planning.md) implements the pure policy portion
of ADR 0071: finite budgets, explicit capability/runtime compatibility and stricter
inherited-limit preservation. Final normal contracts pass 63/63; focused
ASan/UBSan and scoped planner coverage pass. No getrlimit/setrlimit adapter,
helper launch/profile contract, enforced cap or production adoption is included.
Actual startup viability and containment remain subsequent acceptance gates; this
does not close M8 or change a production pin or quarantine.


### Private helper resource setup adapter (#123)

[ADR 0073](adr/0073-helper-resource-setup.md) adds complete-profile validation before
ordered application, captured original OS errors and a checked private macOS/Linux
adapter. Final ordinary contracts pass 64/64 and focused ASan/UBSan passes using a
recording syscall backend. Real getrlimit/setrlimit wrappers compile but are not
executed in tests or wired into the helper; no caps are installed. Trusted launch
identity, startup viability, reviewed budgets, ordinary effective-limit evidence
and broader containment/Apple adoption remain required. M8 remains incomplete.

### Ordinary helper resource observation

ADR 0073 records fresh Capsule/Sword child CPU and peak-resident measurements
with exact binary/source/fixture and macOS provenance; both results pass native
float-bit verification. These two single-run observations install no caps and do
not establish production budgets, larger-document behavior, RSS containment or
full M8 acceptance. The initial denied timing query is recorded separately from
the successful normal wait4 observations, without a crash or exhaustion probe.


### Opt-in helper resource launch (#123)

[ADR 0074](adr/0074-opt-in-helper-resource-launch.md) adds bounded versioned launch
metadata, exact compiled-runtime checks and helper-only CPU/file/core/descriptor
installation plus readback before archive work. Ordinary Capsule/Sword fixtures
pass with explicit test-only settings in normal (66/66 contracts) and ASan/UBSan
(65/65) builds; rejected launch metadata installs no caps. Codec and setup policy
coverage is scoped separately from incomplete OS/main coverage. Virtual-address
activation remains rejected; no production profile, parent resource-report
admission, memory/sandbox/parent-death containment or Apple adoption is claimed.
All production pins/quarantines remain unchanged and M8/full parity remain open.


### Parent resource launch and admission (#123)

[ADR 0075](adr/0075-parent-resource-admission.md) adds a supervised parent operation
that emits the versioned profile and verifies observed exit/runtime/soft-hard caps
before returning owned completion data. Opaque diagnostic tails and original typed
metadata causes survive rejection; frame/revision admission remains independently
required. Normal contracts pass 67/67 and ASan/UBSan 66/66 using ordinary licensed
fixtures and pure rejection data. Metadata coverage is scoped at 100%; incomplete
OS/transport coverage and active lifecycle acceptance remain explicit. Production
profiles, trusted packaging, strong containment, source-aware results and Apple
integration remain open. No production pin or quarantine changes; M8 is incomplete.


### Snapshot XML writer mitigation (#83)

A local iterative writer replaces recursive component emission with bounded
hierarchy planning, exact escaped-size accounting and staged publication. The
non-serializing validator is the only target of invalid-metadata tests; the
quarantined writer fixture remains disabled. Ordinary writer compatibility is
tested separately. See ADR 0076. Delivery revalidation on reviewed master passed
68/68 ordinary CTest contracts, fresh scoped ASan/UBSan writer contracts and the
SwiftPM static-product build. Earlier 63/63 and scoped coverage results remain
historical evidence in the ADR. Core #131 merged after completed clean review and
all CI green; Apple dependency adoption remains required. This does not establish
universal XML validity or complete M1/M8 safety.

### FBX convention recovery (#99)

The saved pure time/angle helpers are restored as a source-audited foundation
against the current core, with independent value contracts and unchanged
reference truncation/tie semantics (ADR 0077). This does not restore serializers,
animation evaluation, Apple adoption or independent-consumer acceptance. Recover
the remaining archived export stages through separate audits and review gates;
M7 remains incomplete.

Rigid matrix validation and reference XYZ Euler extraction are recovered as pure
value operations under ADR 0078, with a standalone fixed matrix type and no rig
generation or pose evaluation. Remaining saved serializers and owners require
their own source audits, ordinary contracts and review gates before adoption.

The bounded move-only FBX property block is recovered under ADR 0079, with
byte-exact in-memory contracts and explicit retained-byte/count admission. Full
scene serialization, file publication and independent re-import remain open;
no rig, animation, GLB or Apple production path is enabled by this foundation.

Immutable owned FBX node trees are recovered under ADR 0080, with aggregate
depth/count/capacity and record-size contracts. Complete document serialization,
real scenes and independent consumers remain required before M7 acceptance.

Owned FBX7400 container encoding is recovered under ADR 0081, including checked
absolute offsets and a test-only known-positive reference-codec comparison.
Actual scene construction and independent geometry/material re-import remain
open; structural container agreement does not establish full export parity.

The static geometry scene builder is recovered under ADR 0082, with pure count/
staging admission, immutable source ownership and an optimization-safe optional
import verifier. Geometry-only output leaves materials, rig/animation, full
document/export orchestration and broad independent-consumer acceptance open.

The single-atlas static material extension is recovered under ADR 0083, with six
embedded caller-validated PNG channels, pure encoded-byte admission and an
optimization-safe optional importer verifier. Source baking/grouping, shader and
alpha compatibility, rig/animation and full native export acceptance remain open.


Static source-face selection is implemented under ADR0084 with independent
geometry ownership, unchanged shared vertex identities and replacement atlas UVs.
This enables later material grouping; original source geometry retention, C ABI,
atomic writes and native export integration remain open. No rig/animation or
quarantined GLB adoption is implied by this static subset foundation.


Original generated geometry can now be retained through an opt-in synchronous
output (ADR0085), avoiding reconstruction from float preview buffers. Material
bake/group correspondence, owned C ABI, independent generated-scene acceptance
and native UI/CLI output remain open; M7 is not complete.


Packed procedural atlas UVs now align with original double export geometry
through an opt-in same-generation hook (ADR0086), verified with a real Capsule
color bake. Actual baked-image encoding, imported/source-aware material groups,
C ABI, atomic destinations, UI/CLI and independent generated-scene acceptance
remain required before M4/M7 completion.


Baked RGBA images can now be encoded as bounded PNGs through system zlib
(ADR0087), with independent optimized-Python CRC/pixel/alpha/row checks. Actual
map-to-material orchestration, imported policy groups, C ABI, atomic destinations,
UI/CLI and generated textured-scene acceptance remain open. M4/M7 remain incomplete.


Baked maps now form owned static FBX materials under an aggregate PNG capacity
budget (ADR0088). One generated procedural Capsule scene independently re-imports
with original positions/normals/winding, packed UVs, material bindings and exact
color/metallic/roughness/emissive pixels. Source-aware policy groups, actual baked
normal/AO and renderer agreement, export C ABI, atomic output and native UI/CLI
remain open; this evidence does not complete M4/M7 or full editor parity.


The source-matched static FBX C API is implemented under ADR0089, with owned
bytes, explicit geometry/baked-atlas modes, scoped quotas, cooperative cancellation
and actual C/Swift6 actor-copy contracts. Its measured boundary coverage remains
below adoption goals because exception fallbacks/defensive guards are unforced.
Atomic output, app progress/commands, reviewed pin adoption and broad full-export
acceptance remain open. No quarantined path or full-parity claim is enabled.


A usable Qt-free headless static FBX command now reads .ds3 and atomically
publishes geometry or a procedural baked atlas (ADR0090). Both CMake and SwiftPM
products independently re-import through Assimp against original corners/pixels.
Native app adoption, full filesystem/error coverage, imported policies, other
formats, bulk batch and rig/animation/audio remain open; M7 is not complete.


Original double OBJ polygon encoding and transactional generation outputs are
implemented under ADR0091, with controlled independent quad/triangle re-import.
Public OBJ C API/CLI/app adoption remains open. Source inspection also found the
shared float-preview validity dependency; decouple it from original exports using
small owned geometry tests before claiming full precision/workflow acceptance.


ADR0092 separates original export preparation from float preview validity and
allocation. Static FBX API/CLI use this mode; controlled original-value tests and
independent baked CLI re-import pass. Public OBJ ownership/CLI and app adoption
remain open. This resolves the ADR0091 coupling without claiming M7 completion.


ADR0093 adds owned original OBJ C API output with ordinary lifetime/quota/cancellation tests. CLI dispatch, Swift owned-copy verification, independent generated-output import and app adoption remain open; M7 is incomplete.


ADR0094 delivers OBJ CLI dispatch and a Swift 6 owned-copy consumer, with independent generated OBJ re-import and atomic-output regression checks. Native editor export service/UI and full M7 acceptance remain open.


ADR0095 adds bounded serial headless batch export with cross-job source protection and recoverable per-job failures. Tests verify continued successful commits and explicit partial status on both command builds. Qt legacy argument expansion, native app adoption, WAV/GLB/rig/animation and full M7 acceptance remain open.


ADR0096 adds upstream single-source -o/-output argument expansion, bounded target admission and aggregate status compatibility through the shared serial export use case. Tests cover actual OBJ/FBX success and recoverable unsupported-format failure. Paste-XML/color-toggle/one-generation parity and the rest of M7 remain open.


ADR0097 adds serial ordinary static-export timing receipts with fixture/executable hashes and failure-aware sample admission. Fresh Debug contracts pass 100/100; six ordinary Release samples on the licensed Capsule and Sword fixtures qualify with stable hashes. These single samples include process startup and IO, not statistical production budgets. Phase/peak-memory/GPU/editor-latency, larger-document and cross-hardware acceptance remain required; M8 is incomplete.

### Generator-free inherited animation catalog (Apple #145)

ADR0098 splits declaration metadata from biped generator pointers through one
shared source table. Portable native targets now compile the actual 180-entry
catalog, preserving 176 non-alias choices across seven rigs, four legacy names,
timing, directional overrides and standard-bone queries. A standalone metadata
contract links no generator/rig/mesh code. All 101 ordinary Debug CTest contracts,
metadata-only ASan/UBSan and SwiftPM build pass; a build-only object checks original
dispatch declarations without linking or executing them. Parameter descriptors,
C/Swift owned-copy APIs, catalog-guided UI and Qt/native runtime interoperability,
sampling/playback/audio and complete M6 acceptance remain open. No production pin
or quarantine is changed; metadata success does not release core #95.


The catalog foundation delivery now includes the required qmake metadata registration and a failing-then-passing manifest regression. Fresh 102-contract, scoped metadata sanitizer and SwiftPM checks pass. Use this corrected foundation for publication; later stages extend its source-membership regression. Qt runtime acceptance remains open.


### Owned animation declaration transport (Apple #145)

ADR0099 adds an immutable owned C catalog with null/index-safe borrowed clip,
override and rig/bone views, plus Swift 6 copy-before-destroy/actor-transfer proof.
Shared diagnostics/mutex bodies move unchanged into generation-independent support
so the standalone metadata API links no mesh/rig/animation generator code. All
102 ordinary Debug CTest contracts, metadata API ASan/UBSan, C11 header, SwiftPM
build and strict Swift fixture pass. Production Swift SDK/UI adoption, parameter
descriptors, pose/rig/frame transport, Qt/native interoperability, runtime sampling,
playback/audio and full M6 acceptance remain open. No production pin/quarantine changes.

### Generator-free presets and reference dependency closure (Apple #145)

ADR0100 moves the inherited AnimationParams value and preset construction into
pure metadata headers/source, retaining exact method bodies and reference-class
forwarders. Native descriptor work can now obtain original string defaults without
sampler linkage. A red source-membership test caught omitted qmake biped metadata;
the current manifest registers it and the new preset sources. All 104 ordinary
Debug CTest contracts, scoped preset ASan/UBSan and SwiftPM build pass. Full Qt
build/execution remains unverified. Before publishing the earlier catalog foundation
PR, backport the metadata qmake registration and its focused manifest regression;
do not publish the known incomplete intermediate reference closure. Parameter
schema porting must replace unchecked inverse slider casts with checked conversion
before native adoption. Descriptor/C-Swift/UI/runtime/playback/audio and full M6
remain open; production pins and all quarantines remain unchanged.

### Checked portable animation parameter definitions (Apple #145)

ADR0101 moves the reference dynamic table into a value-only portable catalog while
keeping an application compatibility adapter. Source comparison preserves table
data/derivation and quoted labels, with explicit handling of the two custom scaled
fish controls. There are 1592 definition instances across the 180 current catalog
types. New native conversion validates finite coefficients/input, integer
representability and slider range, preserving truncation and quantization. It does
not normalize document values merely to display controls. All 105 ordinary Debug
CTest contracts, scoped native math ASan/UBSan and SwiftPM build pass. The reference
callback interface is compile-only; full Qt behavior remains unverified. C/Swift
descriptor transport, native UI adoption, common timing/audio controls, Qt/runtime
interoperability, sampling/playback/audio and complete M6 acceptance remain open.
No production pin or quarantine is changed. Earlier catalog publication still
requires its qmake dependency backport before that foundation PR ships.


ADR0102 adds owned C descriptor transport for all 1592 dynamic control definitions, preserving clip ABI and preset overrides. The Swift 6 smoke consumer copies strings/scalars before owner destruction and transfers immutable values across actors. Full Debug CMake/106 CTest, scoped ASan/UBSan and SwiftPM checks pass. Production SDK/UI adoption, separate common timing/audio widgets and full M6 runtime acceptance remain open; pin and quarantines are unchanged.


ADR0103 shares common duration/frame-count widget metadata with the reference UI and adds checked scalar timing edits. All 107 ordinary CTest contracts and scoped sanitizers pass. This does not evaluate frames or normalize stored documents. Production C/Swift/UI adoption, audio controls and M6 runtime acceptance remain open; production pin/quarantines are unchanged.


ADR0104 exposes common timing metadata and checked scalar edits through C, with strict Swift 6 copied-value/actor verification. All 108 ordinary CTest contracts, scoped ASan/UBSan and SwiftPM build pass. Production SDK/UI adoption and full M6 runtime/audio acceptance remain open; pin and quarantines are unchanged.


ADR0105 extracts the eight reference audio surface names/conversions into generator-free metadata, shares reference UI order and exposes owned C names verified by Swift 6 copies/actor transfer. All 109 ordinary CTest contracts, scoped ASan/UBSan and SwiftPM build pass. Unknown stored names remain raw document data. Audio synthesis/playback, production SDK/UI adoption and full M6 acceptance remain open; pin and quarantines are unchanged.


### Static spring-step admission mitigation (core #95)

ADR0106 derives a pure linear stability precondition for the reference spider
walk/run springs and rejects unstable intervals before reference pose allocation.
The helper has pure boundary tests and scoped complete coverage; reference callers
are syntax-checked only. No sampler execution or root-cause confirmation occurred.
Native sampling adoption, contextual diagnostics, forced-motion/output validation
and full M6 acceptance remain open. No production pin or quarantine changes.


### Independent worker cleanup diagnostics (#123)

ADR 0107 adds an optional caller-owned, nonthrowing report for exceptional owned-
child cleanup while preserving the original transport exception. Recording-only
contracts cover ordering and independent causes without process/signal delivery;
normal licensed transport and pre-start cancellation check empty/reset reports.
Fresh ordinary CTest passes 111/111, scoped recording-policy ASan/UBSan passes,
and the SwiftPM product builds. Actual exceptional OS cleanup remains unforced,
reap can still block, and strong containment/Apple integration remain open. No
production pin or quarantine changes; M8 and full parity remain incomplete.


### Bounded partial worker failure context (#123)

ADR 0108 retains the collector's bounded opaque reply/diagnostic buffers on error
without wrapping the primary exception. Optional outcome means actually observed
termination; separate cleanup observations and partial bytes cannot establish a
valid completion. Value-only ownership/unwinding tests and ordinary existing
transport checks pass; fresh CTest 112/112 and scoped capture ASan/UBSan pass.
Actual OS failure/termination paths and strong containment remain unforced/open.
No production pin/quarantine change or Apple adoption; M8 remains incomplete.


### Bounded worker UV metadata (#162)

ADR 0109 defines a standalone binary64 UV metadata codec preserving component
association independently of mapping validity, exact original keys, charts/image
identities and gradients under separate wire/logical/text/count policies. Small
value-only wire/rejection tests and a copied licensed Capsule owner pass; fresh
ordinary CTest 114/114 and scoped value-only ASan/UBSan pass. No worker opcode,
pixel transfer or Apple production adoption is enabled. Combined source-correlated
geometry/UV admission, materials/pixels and renderer integration remain required;
M4/M8 and full parity remain incomplete. All pins/quarantines are unchanged.


### Atomic paired worker geometry/UV response (#163)

ADR 0110 adds separate paired framing and aggregate wire/logical admission after
validation-only inspection of both complete schemas. Later geometry/UV rejection
returns no partial value; original typed causes and owned metadata survive. Fresh
ordinary CTest 115/115 and scoped paired/UV-value ASan/UBSan pass. No worker opcode,
native paired producer or Apple adoption is enabled; source-correlated operation,
request options, parent dispatch and renderer integration remain required. Count
matching is not source authentication. All pins/quarantines remain unchanged and
M4/M8/full parity remain incomplete.

### Development worker UV requests (#164)

A bounded versioned request codec now retains archive bytes, atlas options and
resolved image dimensions, rejects semantic UUID aliases, and validates every
record and logical quota before retained copies (ADR 0111). Fresh 116 ordinary
contracts and scoped value-only sanitizers pass. Same-owner native UV/geometry
production, dispatch/admission, ordinary process acceptance, containment and Apple
adoption remain required; this does not complete M4/M8. All quarantines and the
production dependency remain unchanged.

### Same-owner development worker UV generation (#164)

The worker producer now copies correlated geometry and atlas metadata from one
native UV owner with exact pre-copy UV/aggregate quotas and owner-destruction
validation (ADR 0112). Fresh 117 ordinary contracts pass; native-boundary
sanitizers cover the changed wrappers, with inherited engine coverage unclaimed.
Helper dispatch, parent admission, ordinary positive process acceptance,
containment and production adoption remain required. All quarantines remain.

### Development UV helper dispatch and admission (#164)

Opcode 2 now connects bounded UV requests, same-owner native generation and
paired response admission in the development helper (ADR 0113). Fresh normal
and supervised Capsule processes exit zero and admit correlated geometry/UVs;
pure malformed/stale/wrong-operation replies reject atomically. All 119 ordinary
contracts pass, with scoped sanitizer/module coverage evidence. Trusted helper
provenance, containment policy, broad fixtures, baking/renderer adoption and
reviewed production dependency integration remain required for M4/M8. No
production pin or quarantine changed.

### Development UV visual-family transport acceptance (#164)

Fresh normal helper jobs now verify Sword gradient endpoints/resolution,
RoundedCrossing mapped/unmapped behavior and synthetic image/alpha/original-key
preservation independently of complete binary equality. All 120 ordinary
contracts pass, with scoped wrapper sanitizer evidence; see
`worker-uv-visual-verification.md`. This does not verify pixels, baking, rendered
appearance, trusted-helper packaging or containment. `WorkerBuild.h` detects
instrumentation only; source/signature provenance remains an explicit #123 gate.
No production dependency or quarantine changed; M4/M8 remain incomplete.

### SwiftPM development helper packaging (#165)

An explicit `dust3d-worker` product now builds the same maintained engine/helper
sources used by CMake, with Unix resource-setup parity (ADR 0114). Source-audited
verifiers pass ordinary base/resource-profile, UV and visual-family jobs against
the actual SwiftPM executable; all 120 ordinary CMake contracts pass. This removes
a packaging obstacle, while trusted identity/signing, containment, Apple adoption,
baking and full M4/M8 acceptance remain required. No production pin or quarantine
changed; Linux/Windows runtime parity is not claimed by the macOS run.

### Descriptor-based single-job worker runtime (#166)

The CLI now reuses its existing bounded frame reader/dispatcher/writer through
borrowed descriptors, with actual request/operation context and original I/O
causes (ADR 0115). Fresh 121 ordinary contracts, scoped runtime sanitizers and
current SwiftPM helper compatibility pass. This prepares descriptor payload
channels for the peer-bound XPC control plane; it does not implement the service,
asynchronous parent streams, authenticated transaction completion, sandbox or
release adoption. An XPC callback must never be substituted for observed process
exit in the existing process admission contract. All quarantines/pins remain.

### Worker-host C/Swift runtime boundary (#167)

A separate C-compatible worker runtime product now reuses descriptor job I/O and
returns self-owned completion/error/context/diagnostic fields with caught C++
exceptions (ADR 0116). C11, C++ and strict Swift 6 owned-copy consumers pass; the
final inventory has 124 ordinary contracts. This prepares the Swift host without
changing its production pin. Parent bounded decoding, authenticated transaction
admission, the real XPC service, async lifecycle and release containment remain
required. Complete-frame status must never be treated as geometry success or
observed process exit. All quarantines remain.

### Incremental parent reply framing (#168)

The existing Unix parent's bounded framing is now reusable for descriptor hosts:
fixed-header validation precedes body retention; explicit EOF and sticky rejection
are available independently (ADR 0117). All 125 ordinary core contracts, scoped
pure sanitizers and rebuilt SwiftPM helper compatibility pass. No parent C API,
authenticated transaction, isolated XPC engine service or production adoption is
claimed. Those gates and full M4/M8 acceptance remain open; pins/quarantines stay
unchanged. Framing success must never stand in for observed process completion.

### Parent reply C ownership boundary (#169)

A separate opaque C owner now makes the shared bounded collector importable by
Swift, with explicit EOF, sticky typed diagnostics and copied-value consumers
(ADR 0118). All 127 ordinary contracts, scoped pure sanitizers and current
SwiftPM runtime/helper checks pass. Actual descriptor/XPC lifecycle, semantic C
admission, authoritative authenticated transaction completion and containment
remain open. A framed byte view cannot authorize publication or invent process
success; no production pin/quarantine changes or full M4/M8 acceptance follow.

### Semantic parent reply C values (#170)

EOF-framed replies now decode once through the shared base/UV/failure codecs into
independent C value owners, with aggregate logical quotas and original typed
rejections (ADR 0119). All 129 ordinary contracts, scoped pure sanitizers and
strict Swift6 copied-geometry/actor-transfer evidence pass. This removes another
Swift-host integration prerequisite while preserving process/authenticated
transaction/publication admission as separate required gates. Actual isolated
XPC service lifecycle, containment, trusted packaging and reviewed adoption remain
open; pins/quarantines and full M4/M8 acceptance are unchanged.

### Isolated XPC static mesh integration evidence (#171)

A freshly built private host/XPC service now runs one ordinary licensed Capsule
job in a real separate process through the C runtime, with development peer-hash
requirements, scalar control receipts and bounded parent C collection/semantic
copies (ADR0120). The focused job and all 130 ordinary contracts pass. This
verifies process separation on macOS arm64, while live pipes, cancellation,
authenticated transaction/publication policy, sandbox/resource bounds, release
trust and reviewed app adoption remain open. PID disappearance is not an observed
service exit status. No production pin/quarantine change or full M8 acceptance.

### Shared C request encoding and isolated UV acceptance (#172)

The Swift host fixture now builds base/UV frames using the maintained C encoder,
with a borrowed-archive codec path and copied identity/options (ADR0121). Both
isolated XPC operations pass: 240 vertices and 80 paired UV triangles, with all 133
ordinary contracts and scoped pure sanitizers green. This removes hand-written
Swift wire construction while preserving quotas and original UUID spellings.
Live pipes, bounded scheduling/cancellation, authenticated completion/publication,
sandbox/resource/release trust and reviewed app adoption remain open. All pins
and quarantines are unchanged; full M4/M8 acceptance remains incomplete.

### Sandbox-entitled healthy XPC acceptance (#173)

The development service verifies App Sandbox at signature inspection and runtime
self-entitlement boundaries, with exactly one read-only peer-file exception
(ADR0122). Healthy base/UV jobs and all 135 ordinary contracts pass. Pure policy
and XML-extraction regressions reject broadened metadata without denial probes.
This improves mitigation evidence while leaving release trust, effective-permission
review, live pipes, bounded scheduling/cancellation, authenticated publication and
reviewed Apple adoption open. No production pin/quarantine changes or full M8
acceptance follow from the entitlement receipt.

### Concurrent healthy XPC descriptor acceptance (#174)

Healthy base/UV jobs now stream concurrently through anonymous descriptors under
the existing sandbox/peer checks, joining bounded writing, actual EOF/semantic
copies and the separate control receipt (ADR0123). File reference cases remain;
all 138 ordinary contracts pass after owned endpoint cleanup/result composition.
No broken-peer/signal/pressure probes or production pin/quarantine changes.
Bounded admission, cancellation/failure diagnostics, authoritative publication,
release trust/resource review and app adoption remain open; full M8 is incomplete.

### One-slot native service admission (#175)

Immutable leases now bound application-native queued/running work before the
service calls the runtime, with stale completion protection and retirement/enqueue
ordering under one lock (ADR0124). Pure values/recorded actions and all 139 ordinary
contracts pass; healthy XPC cases require accepted admission. No native overlap,
load or rejection probe was performed. Per-connection ownership, cancellation,
resource/release trust and authenticated publication remain open. Pins/quarantines
and full M8 acceptance remain unchanged.

### Connection-owned XPC fixture lifecycle (#176)

Development XPC fixtures now claim one owning connection on its first method call,
after the framework peer check. Listener candidate issuance does not claim it;
foreign or unclaimed invalidation cannot retire it. Pure recorded tests cover
those decisions; native integration covers healthy jobs only. Reconnection,
cancellation, release trust and current-document publication remain pending. See
[ADR0125](docs/adr/0125-xpc-connection-ownership.md) and #176.

All 139 ordinary contracts pass (22.66 seconds). Full M8 acceptance remains open.

### Queued worker cancellation foundation (#177)

Queued development worker work can now be withdrawn by its owning connection and
lease before native entry. Recording-executor tests verify slot retention and
ordered cancellation/retirement; healthy native jobs remain unchanged. There is
no XPC cancellation command or running-native interruption yet. See
[ADR0126](adr/0126-queued-worker-cancellation.md) and #177.

All139 ordinary contracts pass22.51 seconds; full M8 acceptance remains open.

### Caller-routed queued cancellation (#178)

Development XPC serve now carries a nonzero monotonic caller request identity,
and queued cancellation routes by that identity and the owning connection. Pure
recorded tests reject stale/reused/foreign routing without native cancellation.
Healthy native serve checks pass; the remote cancellation method has not been
invoked in integration. Publication, channel teardown, deadlines and running-job
cancellation remain open. See [ADR0127](adr/0127-request-routed-queued-cancellation.md)
and #178.

### Persistent-service semantic admission (#179)

`PendingServiceReply` provides single-use persistent-service semantic admission
without fabricating child exit. It requires caller-owned connection/current-source
observations, actual EOF and a matching scalar receipt, then returns copied mesh,
UV or original failure data. It does not authenticate or publish. The C bridge,
XPC host and Apple broker adoption remain open; see
[ADR0128](adr/0128-persistent-service-reply-admission.md) and #179.

All139 ordinary contracts pass22.56s; current-source app publication remains unimplemented.

### Generation-bound C service admission (#180)

Use `dust3d_worker_service_reply_create` before service submission to capture the
expected connection generation. Feed actual framing bytes/EOF, then call
`dust3d_worker_service_admit` with authoritative current identity, observed peer
and control receipt. Copy decoded values before destroying owners. Ordinary
semantic decode rejects service collectors. Receipt opcodes are raw integers
validated before enum conversion. Neither API authenticates its caller inputs or
publishes app state; real XPC/Apple adoption remains pending. See
[ADR0129](adr/0129-service-admission-c-boundary.md) and #180.

All139 ordinary contracts pass22.70s; worker runtime15sources and helper build. Full editor/M8 acceptance remains open.

### Real healthy XPC service admission (#181)

The development XPC host now uses service-specific C admission for healthy base/UV
file/socket replies, with collectors created before submission, actual EOF and an
actor-owned OS peer snapshot. A retained multi-reader completion event joins the
reader and parent without moving opaque owners between queues. All140 ordinary
contracts pass; the four healthy isolated pilots verify the C acceptance marker.
This is immutable-fixture evidence, not mutable editor publication/reconnect or
release acceptance. See [ADR0130](adr/0130-xpc-service-admission-integration.md)
and #181.

### Bounded service source coordinator (#182)

The existing `RequestQueue` now supports `beginService`/`completeService` with a
connection generation frozen before submission and current-source retirement.
Wrong-mode/foreign completions retain active work; matching failed admission
releases capacity while preserving the latest waiting source. The child-exit
path stays separate. Scoped queue tests cover every implementation line/branch;
all 140 ordinary contracts pass. C/Swift runtime and host/editor adoption remain
pending. See [ADR0131](adr/0131-bounded-service-request-queue.md) and #182.

### Source-aware worker output route (#219)

The native worker now has a bounded source-budget request, generation dispatch,
paired geometry/source semantic admission for child/service/queue paths and
independent C output getters. Fresh known-positive Capsule runtime generation and
bounded pure codec/state/ownership tests pass; all 148 ordinary contracts pass.
See [ADR0137](adr/0137-source-aware-worker-runtime.md). This closes one source-output
gap, not Apple #279 or M8: owned Swift conversion, indexed/material/mip parity,
reviewed dependency adoption and native editor/host acceptance remain open. The
Apple production pin and all feature/fault quarantines remain unchanged.

### Indexed geometry/source ownership checkpoint (#223)

Indexed geometry and source hints now have atomic pair admission and an owned
same-generation native adapter. Source corner count matches triangle-index count,
not unique vertices. Pure export-budget arithmetic is tested without native
failure/resource probes; all 151 ordinary contracts pass. See
[ADR0139](adr/0139-indexed-source-generation.md). Runtime/C/Swift adoption,
material/mip/statistics parity and native editor acceptance remain required.
Apple #280 also records that prototype aggregate ceilings do not yet preserve
the editor's separate default geometry/source payload range. That range must be
preserved before default adoption; no silent tightening or parent-engine fallback.
No Apple production pin or feature/fault quarantine changes in this checkpoint.

### Indexed worker runtime checkpoint (#225)

Indexed/source and ordinary indexed operations now provide request construction,
generation, semantic admission and owned C vertex/index/source output. All 152
ordinary contracts pass. The existing editor's optional-source recovery on status
3/4 can later stay within the worker using the ordinary indexed route, preserving
the original issue. No retry controller or Apple backend/pin change is made here.
See [ADR0140](adr/0140-indexed-worker-runtime.md). Swift conversion, full rendering
data/statistics, supported-range parity (#280), source-aware XPC/editor acceptance
and later milestones remain open under Apple #279; no full parity claim.

### Baked texture value checkpoint (#227)

A portable owned codec now preserves five map roles, exact RGBA8 pixels, optional
map presence, base-inclusive mip chains and the producer's chart-safe LOD. Full
framing/shape/aggregate inspection precedes decoded arrays; the named pure codec
has 100% measured line/branch coverage. All 153 ordinary contracts pass. See
[ADR 0141](adr/0141-worker-baked-textures.md). Native copying, full matching
geometry/UV/tangent/policy/catalog pairing, request/runtime/Swift consumption and
actual editor acceptance remain open under Apple #279/#280. No platform decoder,
Apple pin/backend change or full-parity claim is introduced; M4–M8 remain open.

### Coherent source-baked output checkpoint (#229)

A source-baked worker value now retains matching expanded geometry, packed UVs,
tangents, optional alpha policies, model/component catalogs and exact texture/mip
chains. A preflighted copy reads one stable native owner; it does not infer source
correspondence from counts. All 155 ordinary contracts pass; named pure codec and
shared UTF-8/texture validation have full measured line/branch coverage. Defensive
native-copy guards remain below the adapter coverage target. See
[ADR 0142](adr/0142-source-baked-worker-output.md). Request/C/Swift transport,
imported-only and legacy material paths, real statistics/native metrics, supported
payload range and editor acceptance remain open under Apple #279/#280. No Apple
pin/backend or quarantine change; this does not prove full M4–M8 completion.

### Legacy baked output checkpoint (#231)

The existing procedural material path now has coherent worker values preserving
expanded geometry, full UV charts/gradients and associations, exact tangent frames
and optional base maps. Native copying preflights one baked owner using shared UV,
geometry and scalar validation. All 157 ordinary contracts pass; named pure codec
and tangent validation have full measured line/branch coverage. Native boundary
coverage remains partial and below the adoption target. See
[ADR 0143](adr/0143-legacy-baked-worker-output.md). This does not substitute a source
bake, activate a backend, change a production pin/quarantine or prove M4–M8 parity.
Imported authored material data, real metrics, payload-range preservation, complete
transport/Swift consumption and editor acceptance remain open under Apple #279/#280.

### Authored material facts checkpoint (#233)

A portable worker fact codec now retains exact imported PBR factors, optional
sampler/filter state, image declarations/encoded bytes and instance bindings.
Native copying shares pure preflight and owns all returned storage. All 159
ordinary contracts pass; the named pure codec has full measured line/branch
coverage, while native boundary coverage stays partial below the adoption target.
See [ADR 0144](adr/0144-imported-material-worker-facts.md). This is not image
decoding or full imported-render output: source geometry, decoded pixels, real
metrics, supported range and transport/Swift/editor acceptance remain open under
Apple #279/#280. No production pin/backend or quarantine change; M4–M8 remains open.

### Authored imported surface checkpoint (#235)

Matching geometry, original finite repeating UVs, per-corner binary64 straight
RGBA and numerical RGB encoding now have an owned portable value and codec, with
exact catalogs and native-owner preflight/copy. See
[ADR 0145](adr/0145-imported-surface-worker-output.md). Material-owner provenance,
platform image decoding in the worker, real metrics, transport/Swift integration
and supported-range parity remain separate Apple #279/#280 gates. No production
pin/backend or quarantine change; full M4–M8 acceptance remains incomplete.

### Joined imported rendering checkpoint (#237)

Surface-ordered material definitions and optional straight RGBA8 source samples
now have a joined worker value, retaining encoded bytes alongside pixels. Complete
inner/cross-catalog inspection precedes decoded arrays. See
[ADR 0146](adr/0146-imported-render-worker-values.md). A same-archive producer and
platform image decoding inside the Apple worker, real session metrics, transport,
Swift ownership and supported-range parity remain open Apple #279/#280 gates.
No production pin/backend or quarantine change; full M4–M8 remains incomplete.

### Imported render producer checkpoint (#239)

An explicit native producer now loads facts/surface from one unchanged archive and
prepares catalog-order definitions through an injected portable decoder boundary.
Native owners are released before decoding; receipt checks preserve limits and
original causes. See [ADR 0147](adr/0147-imported-render-producer.md). Actual platform
PNG/JPEG decoding inside the Apple worker, transport/Swift ownership, real session
metrics and supported-range parity remain open Apple #279/#280 gates. No default
backend, production pin or quarantine change; full M4–M8 remains incomplete.

### Worker platform decoder ownership checkpoint (#241)

C-compatible callback input, opaque image owners and bounded original diagnostics
now connect to the portable imported producer through an explicit C++ adapter.
See [ADR 0148](adr/0148-worker-image-decoder-ownership.md). Actual Swift/ImageIO
worker orchestration, runtime registration/transport, real metrics and supported
range acceptance remain open. No production pin/backend or quarantine changes;
full M4–M8 acceptance remains incomplete.

### Imported rendering runtime checkpoint (#243)

Explicit budgeted imported requests now reach worker jobs with an injected decoder,
then child/service/queue admission and independently owned C rendering output.
Successful absence and detailed original callback failures remain distinct. See
[ADR 0149](adr/0149-imported-render-worker-runtime.md). All 173 ordinary contracts
pass; scoped request/getter coverage is complete. Actual ImageIO inside the Apple
worker, Swift orchestration, source-baked/legacy transport, real persistent session
metrics, Apple #280 range parity and M4–M8 acceptance remain open. Production pin,
default editor backend and feature quarantines remain unchanged.

### Procedural worker image-input checkpoint (#245)

Bounded procedural image-reference admission and validated native metadata extraction
now retain exact ordered image identities, keys and encoded bytes before platform
decoding. Metadata inspection skips GLB decoding and is not generation validation.
See [ADR 0150](adr/0150-worker-procedural-image-inputs.md). All 175 ordinary contracts
pass; pure resolver coverage is complete and native defensive paths remain explicitly
unprobed. Distinct procedural/imported ImageIO policies, full baked-output producer
and runtime adoption, persistent session metrics, Apple #280 range parity and M4–M8
acceptance remain open. Production pin/backend and feature quarantines are unchanged.

### Legacy bake runtime checkpoint (#247)

Complete legacy rendering now traverses budgeted requests, a distinct procedural
provider, one native baked owner, child/service/queue admission and owned C
geometry/UV/tangent/base-map accessors. Host work caps remain separate from request
settings. See [ADR 0151](adr/0151-legacy-bake-worker-runtime.md). All 184 ordinary
contracts and scoped sanitizers pass; pure request/policy/preparation coverage is
complete while native error branches remain explicitly unforced. Actual worker
ImageIO/Swift orchestration, source-baked/mip transport, real persistent session
metrics, Apple #280 range parity and M4–M8 acceptance remain open. Production pin,
default backend and feature quarantines are unchanged.


### Source bake runtime validation checkpoint (#249)

The source-baked path now has explicit worker requests, complete native ownership,
child/service/queue semantic admission and C surface/catalog/mip output. Distinct
procedural/imported decoder roles share admitted encoded/pixel budgets; contextual
image failures preserve original diagnostics. The ordinary 191-test gate and
SwiftPM runtime build pass. See [ADR 0152](adr/0152-source-bake-worker-runtime.md).
Scoped sanitizer checks pass and measured coverage limits are recorded in the ADR;
automated review and all CI remain required. This does not close actual
ImageIO/Swift worker orchestration, persistent session/cache/native metrics,
Apple #280 supported ranges, visual/LOD acceptance or M4–M8 feature parity.


### Worker statistics foundation (#251)

Actual native cache/count/status/diagnostic snapshots can now be copied into bounded
owned worker values without replacing unavailable metrics with zeros. Pure codec
coverage is complete; ordinary sequential Capsule reuse/reset and scoped adapter
checks pass. See [ADR 0153](adr/0153-worker-generation-statistics.md). This does not
complete persistent worker lifecycle, same-call output correlation, host/Swift
transport, Apple #279/#280 or M4–M8 acceptance. The production pin is unchanged.

### Worker native session foundation (#251)

One worker-owned native session now retains actual document cache state across
indexed/source requests and owns each returned output. Statuses3/4 trigger ordinary
indexed generation in that same session; original source and fallback causes survive.
Final-call statistics failures remain separate from geometry. All 195 ordinary
contracts pass, with scoped positive native sanitizers and complete pure-result
policy coverage. See [ADR 0154](adr/0154-worker-native-session.md). Connection/host
lifecycle, request/revision correlation, C/Swift transport, Apple #279/#280 and full
M4–M8 acceptance remain incomplete. Production pin and quarantines are unchanged.

### Worker connection lifecycle (#251)

A host-owned document/connection wrapper now exclusively owns native cache lifetime,
rejects foreign or replayed requests, preserves ordering through reset and retires
on close. Pure scope and ordinary Capsule lifecycle tests pass; all 197 contracts
and the SwiftPM runtime build pass. See [ADR 0155](adr/0155-worker-session-lifecycle.md).
Actual authenticated host adoption, request/result transport, C/Swift ownership,
statistics cause mapping, Apple #279/#280 and M4–M8 acceptance remain open. Production
pin and quarantines are unchanged.

### Atomic worker session output (#251)

A portable result envelope now retains final sourced/ordinary/empty/failure outcomes,
original source/statistics issues and actual optional cache observations together.
Native statistics exceptions are explicitly mapped with bounded original causes.
All199 contracts and the SwiftPM runtime build pass; scoped sanitizer/coverage limits
and positive Capsule round-trip evidence are in [ADR 0156](adr/0156-worker-session-output.md).
Authenticated host adoption, correlated transport, C/Swift ownership, platform
decoding, Apple #279/#280 and M4–M8 acceptance remain incomplete. Production pin,
backend and feature quarantines are unchanged.

### Correlated worker session producer (#251)

Operation 9 now has an explicit host-owned C++ session producer with source-budget
requests and exact response identities. Diagnostic reservation and bounded original
causes prevent host policy from being ignored; ordinary Capsule reuse/ownership
and pure preflight tests pass. All 200 contracts and the SwiftPM runtime build pass;
see [ADR 0157](adr/0157-worker-session-operation.md) for scoped sanitizer/coverage
and conservative reservation limits. Legacy stateless/C/host admission rejects the
operation pending dedicated ownership support. Authenticated serving, C/Swift/XPC
integration, Apple #279/#280 range parity and M4–M8 acceptance remain open. Production
pin/backend and quarantines are unchanged.

### C session-output ownership (#251)

The complete session envelope now decodes into one immutable C root with borrowed
geometry/source/failure views, explicit statistics availability and copied actual
observations. Tiny pure contracts, an independent C11 consumer and ordinary Capsule
cache/ownership compatibility pass, alongside all 202 contracts and the SwiftPM build.
[ADR 0158](adr/0158-worker-session-output-c.md) records recovery-guard coverage gaps.
Raw body decoding does not authenticate or admit current publication. Dedicated
serving/admission, Swift/XPC ownership, platform decoding, Apple #279/#280 and M4–M8
acceptance remain incomplete. Production pin/backend and quarantines are unchanged.

### Dedicated session service admission (#251)

Session envelopes now pass the same shared current-source/connection/peer/EOF/receipt
gates as existing service output, with dedicated C and queue-owned result adoption.
Foreign/mode mismatches retain active work; matching stale, cancelled, closed or
invalid completion retires publication while preserving newer waiting work. All204
contracts and scoped sanitizer checks pass; [ADR 0159](adr/0159-worker-session-admission.md)
records selected-function coverage and unforced recovery guards. Host authentication,
native C serving/lifecycle, request factories, Swift/XPC integration, Apple #279/#280
and M4–M8 acceptance remain incomplete. Production pin/backend and quarantines are
unchanged.

### C connection-session ownership foundation (#251)

Exclusive C lifecycle ownership now wraps the existing document/connection cache:
configuration, create, live-generation reset, permanent idempotent close and destroy.
It preserves original scope/native diagnostic codes and truncation. All205 ordinary
contracts and scoped lifecycle sanitizer checks pass; [ADR0160](adr/0160-worker-session-c-lifecycle.md)
records measured coverage and unforced recovery paths. C serving/request creation,
Swift/XPC adoption, actual authenticated transport and Apple #279/#280 output/range
acceptance remain open. Production pin/backend and quarantines remain unchanged;
M4–M8/full-parity acceptance is still incomplete.

### Explicit C retained-session serving foundation (#251)

Operation9 request creation and channel serving now route through an existing
host-scoped C owner, sharing exact EOF framing and I/O diagnostics with stateless
jobs. Ordinary Capsule contracts show reuse across requests, rebuild after reset
and owned output after destruction. All206 ordinary contracts and scoped positive
sanitizer checks pass; [ADR0161](adr/0161-worker-session-c-serving.md) records measured
coverage limits. Swift/XPC lifecycle/decoder adoption, actual authenticated transport,
and Apple #279/#280 complete-output/range acceptance remain open. No production pin,
backend, quarantine or M4–M8 parity acceptance change follows.

### Swift session importer/ownership evidence (#251)

Explicit public module exports now expose the additive session headers under
SwiftPM and standalone CMake importing. A Swift6 strict consumer keeps native handles
inside one detached task, verifies ordinary reuse/reset/close and copies geometry,
selected source bytes and actual observations before actor transfer. All207 ordinary
contracts pass, plus an independent link/run against actual SwiftPM output.
[ADR0162](adr/0162-worker-session-swift-consumer.md) distinguishes this evidence from
actual authenticated XPC, complete output/range and app acceptance. Those #251 and
Apple #279/#280 gates, production pin/backend and quarantines remain unchanged.

### Lazy Swift session lifetime coordinator (#251)

The Swift worker prototype now binds one immutable host-selected scope lazily and
serializes native jobs/reset/close/destruction. Its native adapter preserves original
opaque diagnostics. Fake-only errors/lifetimes and ordinary Capsule reuse/reset/copy
contracts pass; all208 ordinary contracts pass. [ADR0163](adr/0163-worker-service-session-lifetime.md)
records exact coverage gaps. Authenticated XPC RPC/ordered-retirement integration,
full-output/range acceptance #279/#280 and M4–M8 parity remain incomplete. Production
pin/backend and quarantines remain unchanged.

### Ordered session cleanup before service retirement (#251)

The Swift scheduler now drains accepted work before exact-once owner cleanup and
passes the original cleanup Result to process retirement. The XPC fixture queues its
lazy coordinator close on this path. Fake success/error/foreign/duplicate contracts
and all208 ordinary tests pass; [ADR0164](adr/0164-worker-session-retirement.md) records
coverage and unforced boundaries. Explicit session RPC routing/authentication,
initialized-cache XPC cleanup, platform decoding/full-output/range gates and M4–M8
acceptance remain incomplete. Production pin/backend and quarantines are unchanged.

### Host metadata preflight before session bodies (#251)

Session serving can now validate independent host-admitted operation/document/request/
revision immediately after the header and before body retention or native generation.
The additive correlated C API and Swift prototype adapter use this gate; pure framing,
valid Capsule compatibility, C11, scoped sanitizer and all209 ordinary contracts pass.
[ADR0165](adr/0165-worker-host-request-preflight.md) records exact evidence/limits.
Authenticated session RPC routing, initialized-cache XPC cleanup and full-output/range
acceptance #279/#280 remain incomplete. Production pin/backend/quarantines are unchanged.


### Healthy authenticated session lifecycle evidence (#251)

Core #265 added retained-session XPC generation/close, actual reuse statistics and
copied output after retirement. Ordered reset now shares authenticated connection
scope and admission, with ordinary Capsule rebuild/nonreuse and geometry preservation
checks. All210 ordinary contracts pass; see [ADR0166](adr/0166-worker-session-rpc.md)
and [ADR0167](adr/0167-worker-session-rpc-reset.md). This is development-fixture evidence.
Apple adoption, platform decoder/full-output/range gates #279/#280, production signing
and M4–M8 acceptance remain incomplete. Production pin/backend/quarantines are unchanged.


### Worker Apple image callback foundation (#267)

A separate Apple-only callback product now reuses byte-recorded PNG/JPEG metadata
and sample layout, with Foundation-only metadata available independently of native
and platform code. Ordinary positive samples and scalar quota checks pass alongside
all212 contracts; [ADR0168](adr/0168-worker-apple-image-decoder.md) records measured
coverage and the required later removal of pinned Apple copies. Worker wiring and
Apple #279/#280 full-output/range acceptance remain open. Production pin/backend and
all quarantines are unchanged; no GLB path or malformed native codec probe is added.


### Healthy worker-side procedural PNG integration (#267)

The development sandboxed service now enables only the procedural platform callback.
An existing licensed image-bearing archive passes small explicit baking, actual EOF,
authenticated-peer observations and parent queue admission with copied maps after
retirement. All213 ordinary contracts pass; [ADR0169](adr/0169-worker-procedural-image-xpc.md)
records limits. Imported providers remain disabled; full rendering/range acceptance
#279/#280, app adoption and distribution remain open. Production pin/quarantines unchanged.


### Checked preview range ledger (#270)

A pure scalar planner now separates native indexed/source charges, serialized bytes,
retained logical records and auxiliary diagnostics/statistics. Tiny real encoder
comparisons and large count-only examples pass, including explicit indexed budgets
above the256MiB default. All214 ordinary contracts pass; [ADR0170](adr/0170-worker-preview-range-accounting.md)
records scoped sanitizer/coverage evidence. This is accounting, not transport activation.
Bounded transfer, complete semantic admission and Apple #279/#280 acceptance remain open;
production pin/backend and all quarantines are unchanged.


### Bounded preview framing foundation (#270)

A distinct fixed-header chunk format and pure sequence state now preserve captured
identity and canonical offsets, include physical header overhead and require actual
EOF after the terminal marker. Pure tiny/scalar tests and all215 ordinary contracts
pass; [ADR0171](adr/0171-worker-preview-chunk-framing.md) records schema and coverage.
Actual readers/writers, complete body semantics and C/Swift ownership remain next;
no native dispatch, cap change, app activation or full range acceptance is claimed.
Production pin/backend and all quarantines remain unchanged.


### Bounded preview byte handling (#270)

Incremental collecting-thread endpoints now handle split headers/bodies with one
policy-sized workspace, exact counts, sealed decode/sink failures and retained first
causes. Tiny round-trip/semantic separation and all216 ordinary contracts pass;
[ADR0172](adr/0172-worker-preview-byte-stream.md) records coverage and remaining OS
channel/body/ownership gates. This does not enable native dispatch or default app
adoption. Full #279/#280 acceptance, production pin/backend and quarantines unchanged.


### Owned preview archive requests (#270)

The D3PC archive collector compares frozen request controls and the shared fixed
source prefix before retaining one contiguous archive, and exposes borrowed bytes
only after exact framing and actual EOF. Native maximum ranges are checked as scalars;
legacy whole-envelope caps stay unchanged. All 217 ordinary contracts pass;
[ADR0173](adr/0173-worker-preview-archive-ownership.md) records scoped coverage and
remaining real-channel, output-semantic, native/C/Swift and Apple adoption gates.
This does not complete Apple #279/#280 or M4–M8. Production pin and quarantines remain
unchanged.


### Archive channels and C/Swift copies (#270)

Bounded Unix descriptor adapters now send and collect segmented archive requests,
retaining original typed causes and requiring actual EOF before a C owner is returned.
The licensed small Capsule fixture survives channel closure, C destruction after copying
and Swift actor transfer; the actual SwiftPM library consumer also passes. All 220
ordinary contracts pass. [ADR0174](adr/0174-worker-preview-archive-channels.md) records
coverage below adoption targets and unforced OS/fault boundaries. Complete wide output
semantics, native dispatch, authenticated/current-source admission and Apple adoption
remain open. No production pin, active route/cap or quarantine changed; M4–M8 remain open.


### Native-budget session semantic streaming (#270)

An opt-in incremental codec preserves the existing session payload layout while checking
records against independent native indexed/source budgets. It retains typed private values
without a duplicate whole wire body or count-sized upfront arrays, and moves ownership once
only after complete semantics and actual EOF. Shared legacy validation and existing caps
remain intact. All 221 ordinary contracts and the actual SwiftPM-library codec consumer
pass; [ADR0175](adr/0175-worker-preview-session-codec.md) records scoped coverage and the
membership regression. Actual native full-budget generation/copy, output channels, C/Swift
owners, authenticated/current-source admission and Apple adoption remain open. No production
pin, active route/cap or quarantine changed; M4–M8/full parity remain incomplete.

### Native-budget ordinary indexed copy (#270)

The opt-in copy boundary validates borrowed native geometry against independently
captured native policies, membership and shared value checks before retaining arrays.
It preserves exact attributes/warnings without legacy count caps or a whole encoded
validation buffer. Existing small Capsule ownership/copy checks, full221CTest, actual
SwiftPM consumer and scoped sanitizer evidence are recorded in ADR0176. Source-pair
copy, full-budget native generation/session dispatch, output channels, C/Swift admission
and Apple adoption remain open. Production pin/routes and quarantines are unchanged;
M4–M8 and full parity remain incomplete.

### Native-budget indexed/source pair copy (#270)

The source-pair copy now checks independently captured controls, native record/text
charges and every original identity/influence/corner before output retention, then uses
the accepted ordinary geometry preflight. Same-generation provenance and owner lifetime
remain caller contracts. Full221CTest, actual SwiftPM consumer, scoped sanitizer and
measured boundary coverage are recorded in ADR0177. Native full-budget session dispatch,
channels, C/Swift admission and Apple adoption remain open; pin/routes/quarantines are
unchanged and M4–M8/full parity remain incomplete.

### Borrowed native-budget preview sessions (#270)

Opt-in native/connection session methods now generate from borrowed complete archive
ownership under original native indexed/source policies, without a legacy input envelope
or consumer copy cap. Same-worker fallback, actual optional cache observations, typed
copy/native receipts and replay/lifetime admission are preserved. Existing small positive
session/ordinary fixtures, pure fake error outcomes,222CTest and actual SwiftPM consumers
are recorded with scoped sanitizer/coverage limits in ADR0178. Output channels, C/Swift
ownership, authenticated/current-source publication and Apple adoption remain open; baked
and imported profiles need separate integration. Pin/routes/quarantines and M4–M8/full
parity status remain unchanged.

### Session-output channels and C ownership (#270)

Bounded borrowed output channels now preserve complete session semantics through actual
EOF, then move into the existing immutable C output owner without legacy text/count caps
or a duplicate whole wire body. Typed framing/range/codec/I/O receipts retain original
causes.225CTest, actual SwiftPM C/C++/strict Swift consumers and copied Sendable actor
lifetime verification are recorded with scoped sanitizer/boundary limits in ADR0179.
Runtime serving, independent process/peer/current-source admission, production SDK/backend
and baked/imported integration remain open. Pin/routes/quarantines and M4–M8/full parity
remain unchanged.

### Segmented native preview serving (#270)

The explicit library serving entry now connects captured archive framing/actual EOF,
connection admission, native-budget generation/copy, archive release and segmented output.
Replay consumption, same-worker cache/fallback, actual optional observations and typed
receipts are preserved. Healthy licensed Capsule C/C++/strict Swift copied-value cases,
227CTest and scoped sanitizer/coverage limitations are recorded in ADR0180. Production
worker endpoint/SDK, independent process/peer/current-source admission, reviewed backend
adoption and baked/imported profiles remain open. Pin/routes/quarantines and M4–M8/full
parity status are unchanged.

### Shared Swift preview profile (#270)

The opt-in Swift backend now dispatches legacy and segmented preview jobs through one
connection-owned C session and replay history, preserving distinct original receipts.
The healthy cross-profile regression verifies real cache reuse, geometry, reset/rebuild
and copied Sendable lifetime.227CTest and actual SwiftPM consumer evidence/limits are
recorded in ADR0181. Explicit XPC selector/typed RPC receipts, independent peer/process/
current-source admission, production SDK/backend and baked/imported adoption remain open.
Apple pin/routes/quarantines and M4–M8/full parity status remain unchanged.

### Segmented preview XPC endpoint (#270)

The development service/peer now transfer segmented previews on the same native session
as legacy requests, preserving original preview/boundary diagnostic domains. A metadata-only
one-shot gate checks actual EOF, independent OS-peer/lifetime and current identity before
copied publication. The healthy existing Capsule fixture verifies cross-profile reuse and
legacy rebuild after reset;228CTest and scoped pure Swift coverage are recorded in ADR0182.
Production active/waiting scheduling and mutation acceptance, public SDK/worker trust, app
backend adoption and baked/imported profiles remain open. Pin/active app routes/quarantines
and M4–M8/full parity status remain unchanged.

### Segmented preview publication queue (#270)

The metadata queue now retains one active transport identity and newest waiting work,
rejects foreign/replayed submissions atomically, and retires obsolete/closed publication
without native interruption. Healthy XPC integration closes channels before matching
completion; pure tests prove waiting replacement/retention, cancellation/no resurrection,
original receipt errors and scope/replay boundaries.229CTest, actual SwiftPM-linked XPC
host and scoped coverage are recorded in ADR0183. Production asynchronous SDK/source
ownership and mutable-document adoption, trust/packaging and baked/imported profiles
remain open. Pin/active app routes/quarantines and M4–M8/full parity are unchanged.

### Immutable preview source ownership (#270)

The source coordinator now follows queue-authoritative active/waiting identities, retaining
immutable archive/policy snapshots through drain and releasing displaced/retired owners.
Pure ARC and actor tests prove source lifetime/isolation and failed-submission preservation;
the healthy XPC path consumes one captured archive/control snapshot.230CTest, actual
SwiftPM-linked XPC host and scoped coverage are recorded in ADR0184. Public asynchronous
SDK/worker packaging/trust, mutable-document backend adoption, high-resource acceptance
and baked/imported profiles remain open. Pin/active app routes/quarantines and M4–M8/full
parity remain unchanged.

### Importable Swift client foundation (#270)

The accepted scheduling/source code now lives once in native/WorkerClient and is exposed
as Dust3DWorkerClient, a SwiftPM product without native-engine/Qt/transport dependencies.
Public API XCTest cases import the real module, and the healthy XPC host imports a
separate client library instead of compiling prototype copies.231CTest, actual SwiftPM
client/runtime consumer, symbol inspection and scoped coverage are recorded in ADR0185.
Production XPC adapter/worker packaging/trust, asynchronous mutable-document adoption,
resource acceptance and baked/imported profiles remain open. Pin/active app routes and
quarantines remain unchanged; M4–M8/full parity remain incomplete.

### Public XPC connection adapter (#270)

The shared XPC protocol and async actor-owned connection now live in Dust3DWorkerXPC,
without native-runtime/engine/Qt dependency or QA identity/trust defaults. Plain options
preserve original budgets and receipt domains. One-RPC correlation, fixed generation/PID,
once-only completion and idle invalidation are fake-tested; the healthy existing host
now uses a C-conversion-only facade over this adapter on one persistent connection.
Eighteen package tests,231CTest, actual SwiftPM consumers and separate fake/healthy
coverage limits are recorded in ADR0186. Production service/bundle/trust, mutable SwiftUI
backend, resource acceptance and baked/imported profiles remain open. Pin/active app
routes/quarantines and M4–M8/full parity remain unchanged.

### Service library extraction (#270)

The reusable `Dust3DWorkerService` product now owns canonical admission/scheduling,
native session lifetime and XPC handlers. Explicit host requirements, service-side
sandbox checking and original retirement callbacks replace test-only ownership;
QA pins/logging/process exit stay in the launcher. ADR0187 records 22 public package
tests,231 CTest contracts and the actual-product healthy session/normal retirement.
Production executable/bundle/trust distribution, async Apple backend, native-range
resource acceptance and M4–M8 remain open. Pin48b0ce2d/quarantines are unchanged.
