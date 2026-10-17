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
