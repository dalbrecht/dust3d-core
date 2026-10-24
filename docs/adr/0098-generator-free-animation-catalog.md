# ADR 0098: Separate inherited clip declarations from generator linkage

Status: Local portable metadata foundation; review and M6 runtime acceptance pending.
Tracking: [Apple #145](https://github.com/dalbrecht/dust3d-swiftui/issues/145).

The inherited catalog queried biped additionalClips(), whose records also retain
generator function pointers. Linking that path would bring sampling implementations
into a metadata query. We instead move the original 48 declaration rows, unchanged
in order and tokens, into clip_definitions.inc. It is shared maintained source,
not generated output or a second copied catalog. clip_catalog.cc expands each row
into the original ClipDefinition with its function pointer. clip_metadata.cc
expands the same row into an immutable ClipMetadata without that pointer or rig
headers. animation_catalog.cc reads the latter; all base entries, legacy aliases,
directional construction and standard-bone behavior remain unchanged.

CMake and SwiftPM explicitly compile only animation_catalog.cc and clip_metadata.cc
for native products. The metadata contract compiles those same two files as a
standalone executable, without engine/rig/mesh dependencies. Symbol inspection
confirms no generator, RigStructure, RigAnimationClip or MeshGenerator symbols.
A separate CMake OBJECT target compiles the original dispatch translation unit to
check declaration/function signature drift. Its object is never linked into a
native product or test, and no generator is run. This narrow inherited-source
refactor remains separable from future adapter additions for upstream maintenance.
Existing copyright/MIT notices and the original dispatch API are preserved.

The catalog is immutable data with process-static storage. C++ static initialization
provides construction synchronization; no inherited UUID/global generation state
is touched. This is not a C ABI or Swift ownership promise. Future C entry points
must retain exception containment, bounds and borrowed lifetime rules and serialize
as required by project standards. Numeric metadata defaults do not prove generator
support, sampled pose correctness, parameter-unit acceptance or audio behavior.

Missing-header tests failed before implementation. The new always-on contract
checks 180 entries, non-alias counts by rig, four aliases, valid generator identities,
finite timing/overrides, independent base/additional/directional/alias examples,
48 shared biped declarations, stable storage and known/unknown bone lookup.
Original rows were token-compared with the prior source. A syntax-only check and
the build-only dispatch object pass. Existing test sources remain byte-identical
to the previously audited ba82aaea parent; the new contract was inspected in full.
The quarantine guard passes. All 101 ordinary Debug CTest contracts pass in 19.86s.
The metadata-only ASan/UBSan contract passes; SwiftPM builds Dust3DEngine in 23.41s.
After adding the build-only object guard, the complete CMake build passes and all
101 runtime test commands are unchanged. This is not a release/sampling/GPU/whole-
core coverage claim. No parser or native ownership boundary is added.

See ../animation-catalog-metadata-verification.md for inventory and remaining work.
Core #67/#69, #83, #84, #95 and Apple #199 quarantines remain unchanged. No sampling,
fault/crash/resource probes, protected app control or production pin change occurs.
