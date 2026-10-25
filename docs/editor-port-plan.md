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
