# Retained cache payload verification

Tracking: core issue #21; ADR 0006. Internal C++ accounting only; no new dependency,
CI workflow, public C ABI or Apple dependency pin change.

## Scope

RetainedPayloadBudget checks byte arithmetic before multiplication/addition,
retains committed usage after rejection and exposes sticky failure. Owner visitors
cover Uuid and snapshot metadata; part/component/Boolean caches; wrapping surfaces,
quad diagonals, UVs, source/weight maps and folds; mesh geometry, normals and spatial
indices; and expanded/indexed opaque output buffers.

Vector capacity counts even after logical clearing or when weight flags are
inactive. Owned pointers include their pointees. SolidMesh source vectors and
spatial-tree box views are borrowed and excluded there, avoiding duplicate charges.
C ABI function-pointer deleters refer to code and do not allocate owned payload.
Unsupported dynamic ownership and stateful allocator/policy shapes require an
explicit visitor/audit. New owner fields break structured-binding compilation
until accounted for.

This budget excludes allocator/container-node bookkeeping, temporary generation
workspace and process RSS. Small-string inline capacity is conservatively counted.
It is not a promise of a hard whole-process memory ceiling.

## Tests and evidence

The first build failed for the missing API. A further failing compile reproduced
unsupported C ABI function-pointer deleters before their borrowed classification
was added. Tests cover zero/exact/insufficient/SIZE_MAX arithmetic, cleared retained
capacity, nested face headers versus indices, metadata and spatial-index rejection,
wrapping/source/UV/imported maps, inactive weight capacity, partial Boolean entries,
mirror maps, copied geometry after source destruction and complete owner cleanup.

Real Capsule and Sword contexts remain inspectable after MeshGenerator destruction;
Sword exercises actual Boolean-combination cache ownership. Exact measured budgets
fit, and one byte less rejects without changing source geometry. Local accounted
payloads were 95,427 and 714,643 bytes respectively; these are observations on this
machine, not cross-platform golden sizes or process-memory measurements.

Test-only ordinary C++ new observations record zero allocations during real cache,
nested metadata, rejected-buffer and C ABI owner inspection. They do not intercept
every possible C/platform allocator. The production implementation uses const
inspection and no allocation/copying policy accessors.

Standalone instrumented new accounting scope measures 169/169 lines, 38/38
functions and 29/32 branches (90.62%). This does not describe inherited engine
coverage. Full Debug and ASan/UBSan suites pass all 27 contracts, the actual SwiftPM
engine product builds, and new C++ formatting, manifest lint and native dependency
boundary checks pass. macOS leak detection is unavailable; hosted Linux keeps
its configured sanitizer settings.

## Remaining work

Persistent caching is disabled. A reviewed session must apply semantic invalidation,
account all retained state, evict on limits, handle mirrors/assets/failures and
share serialization with legacy C entrypoints. Cached-versus-fresh modifier,
profile, binding, reorder/history and large-model performance comparisons remain
required. Source IDs, M3–M8 feature parity and release memory isolation remain pending.
