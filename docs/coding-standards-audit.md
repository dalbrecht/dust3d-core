# Coding standards integration and project gaps

The requested [c65llc/coding-standards](https://github.com/c65llc/coding-standards) repository is pinned as the `.standards` submodule at `20ebe761ab14a2f7aa0e523d2058cd2345068cd7`. The project uses its actual config assembler to inject common, Swift, and app guidance into `AGENTS.md`. `.standards.yml` records the languages and role; the project-specific section is preserved by `make sync-standards`. The shared standards checkout is not modified.

The assembler is used directly rather than the broad setup script: setup installs hooks, Git aliases, Cursor commands, and unrelated agent settings even for a Codex-only invocation. Those side effects are unnecessary for this app. The integration is reproducible with `git submodule update --init .standards` followed by `make sync-standards`.

## Gaps and resolutions

| Finding in the pinned standards | Project resolution |
| --- | --- |
| No C++ language standard, content block, detector mapping, lint rules, or C/C++ ABI guidance | Preserve Dust3D C++17, `.cc`/`.h`, existing naming, and the root `.clang-format`. Add RAII, bounds, ownership, exception, and thread-safety requirements in the project section. Enforce formatting of new bridge files with the existing config. |
| Swift concurrency guidance is generic and does not cover Swift 6 isolation or Swift/C++ ownership | Compile in Swift 6 mode. Use a background actor for file access/generation, a mutex for inherited C++ globals, main-actor UI/Metal ownership, and copied immutable mesh values. No unchecked `Sendable` declarations. |
| Generic app rules prescribe immutable state and even Dart `const` constructors | Treat the mesh as an immutable snapshot and keep mutable presentation state in a scene-owned `@Observable` store. Apply native SwiftUI conventions instead of non-Swift rules. |
| Mandatory screenshot comparison assumes supplied route/state reference images | No design reference was supplied for this new viewer. Validate the running UI and document the evidence; do not invent a comparison baseline. |
| 100% core and 95% module coverage do not distinguish a large inherited engine from new adapters or GPU integration | Measure and report coverage separately. Exercise adapter failure paths and real mesh generation. Do not claim the inherited engine or Metal/native UI meet those thresholds; legacy geometry coverage and GPU automation remain gaps before a production release. |
| Apple build guidance assumes XcodeGen/Tuist and device signing, without a SwiftPM macOS GUI path | `Package.swift` is the declarative source of truth. Stage and ad-hoc sign the local app bundle; distribution needs a dedicated signing configuration. No generated `.xcodeproj` is maintained. |
| SwiftLint is prescribed but not bundled with Xcode | Provide `.swiftlint.yml`; the required local gate uses Xcode's `swift-format`, the compiler, and tests. SwiftLint remains an optional additional tool; it is not falsely reported as run. |
| Generic Git/agent rules assume `main`, their branch prefixes, automatic PR creation, and repeated suggest-mode approval | Upstream is `master`; Codex uses `codex/`. The user's implementation authorization covers local work. Publication/merge are not inferred from a local build request. |
| No graphics/native file format/security boundary guidance | Bound archive/XML allocations, validate arithmetic and geometry, document C-handle lifetimes, reject unsupported imported models, preserve diagnostics, and record inherited algorithm resource limits. |

## Existing C++ alignment

The original `.clang-format` is unchanged: four-space indentation, WebKit braces, left-aligned pointers, existing member naming, and no column limit. `make lint` checks only the new C++ adapter, avoiding unrelated formatting churn in inherited code and third-party libraries. C++ remains C++17. All native dependencies are already vendored in the repository; no new geometry library is introduced.

`make ci` runs formatting/shell syntax checks, the full native debug build, XCTest with coverage, and source/linkage checks for Qt. Coverage is reported rather than misleadingly gated against the entire inherited engine. See the native README for prototype limitations and the verification record for measured results.

The new Swift document library measures 98.51% line coverage. Domain and file-service coverage are 100%. The inherited C++ engine and Metal/UI need separate coverage and verification scopes; they are not claimed to meet those thresholds. Existing C++ warnings include signed one-bit UV flags and deprecated formatting in the inherited archive writer. The project C++ supplement is in `docs/cpp-standards.md`.

## Suggested improvements to shared coding-standards

Add a C++ standard and `cpp` language detection/block mapping; describe RAII and C ABI ownership, enum/status errors, sanitizers, fuzzing, and numerical validity. Add a Swift 6/native desktop supplement covering actors, Metal callbacks, SwiftPM app bundles, local versus distribution signing, and document access. Define separate coverage scopes for new code, adopted legacy code, and GPU/UI boundaries, and make the setup script honor the selected agent without installing unrelated tools or overwriting hooks.
