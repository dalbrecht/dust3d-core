# ADR 0114: SwiftPM development worker product

Status: prepared packaging stage for #165; no Apple adoption or signing claim.

## Decision

Expose `dust3d-worker` as a SwiftPM executable product, alongside `dust3d-export`
and the static `Dust3DEngine` library. Its `Dust3DWorker` target selects 13 existing
helper/protocol/mesh/UV/resource-setup sources from `native/Engine`. It depends on
the maintained engine target and shares its C++17 build. No adapter is copied,
no dependency is added, and selected worker sources do not overlap the engine
source inventory. Enable the same Unix resource-setup definition used by CMake.
The product targets the project's supported macOS/Unix environment; Windows and
Linux runtime acceptance are not established by this macOS run.

An explicit product removes a CMake-only packaging obstacle for future Apple
integration. It does not authenticate a helper executable, sign/notarize an app,
apply a filesystem sandbox or establish production containment. `WorkerBuild.h`
detects instrumentation only. Keep trusted-helper and containment requirements
under #123, and retain the production dependency pin and all quarantines.

## Evidence

Before the manifest change, the shared-scratch command failed with `no product
named 'dust3d-worker'`. Afterward SwiftPM compiled all 13 selected worker sources
and linked the product in 2.92 seconds. A strict-format trailing-comma correction
left the parsed package graph identical; a subsequent product build passed in
0.08 seconds. No runtime or test source changed during the builds.

Freshly source-audited CMake verifiers ran against this SwiftPM executable:
existing ordinary base/resource-profile transport contracts, UV dispatch/admission
and Sword/Boolean/image visual-family contracts all passed. The packaged helper
therefore performs real normal process work; its build result alone is not the
acceptance claim. Existing controlled tiny-archive rejection cases remain normal
typed failures, with no new fault, signal, crash or resource-exhaustion probes.
All 120 ordinary CMake contracts pass (20.61 seconds). The existing export product
still builds (0.04 seconds incremental). Strict Swift formatting and whitespace
checks pass. macOS linkage lists only libc++, zlib and libSystem, with no Qt.
No parser/ownership implementation changed in this packaging stage; previous
scoped sanitizer limits remain, and no whole-engine instrumentation claim follows.

The shared build root and compiler caches are reused. Source inventory, parsed
manifest, binary/source hashes, link inventory and gate logs are saved separately
from disposable CMake products. A binary hash is local build evidence, not a
signature or authorization to run unrelated or stale products.

## Development use

Set `DUST3D_BUILD_ROOT` to the existing shared build root. From this repository:

```sh
swift build --scratch-path "$DUST3D_BUILD_ROOT/build" --product dust3d-worker -j 2
swift build --scratch-path "$DUST3D_BUILD_ROOT/build" --show-bin-path
```

Use the resulting executable through the framed development transport. Input is
one complete frame followed by EOF; stdout is a binary reply and stderr is bounded
untrusted diagnostic data. No text CLI, save operation or production app bundling
is added here. Ordinary/admitted data and resource metadata still require parent
validation against authoritative request identity and actual terminal outcome.


## Serial delivery verification

Replayed after #192 with #190's aggregate key/chart-image text quota and #191's
checked admission fixture access preserved. The current inventory contains 121
ordinary contracts, including the accepted pure quota regression; all pass in
21.10 seconds after a fresh clean CMake build and eleven pure preflight checks.
The parsed manifest selects 13 helper sources disjoint from 34 engine sources.
The SwiftPM helper product builds in 3.11 seconds. Fresh CMake UV dispatch and
visual-family verifiers pass against that exact SwiftPM helper, with normal
successful child completion. These current packaged-runtime pilots use healthy
Capsule/Sword/Boolean/image cases only. Earlier base/resource-profile evidence
above belongs to the original prepared stage. No additional native rejection,
failed-peer, cancellation, crash, signal or resource-pressure pilot is introduced.

The export compatibility product builds, strict Swift formatting and whitespace
checks pass, and recorded executable-source hashes remain unchanged through the
gates. Link inventory contains libc++, zlib and libSystem without Qt. Existing
native build-system deprecation and duplicate static resource-library warnings
remain disclosed. This packaging change adds no parser/ownership implementation;
previous scoped sanitizer limits remain and full engine coverage is unclaimed.
Production pin, signing/containment gates and all quarantines are unchanged.
