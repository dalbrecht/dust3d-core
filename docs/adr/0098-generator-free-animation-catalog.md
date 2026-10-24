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


## Standalone delivery dependency correction

The original local catalog extraction omitted the qmake source registration for
biped/clip_metadata.cc. Existing reference callers require that definition. The
corrected catalog-foundation-delivery branch adds the source and metadata/shared
declaration headers without importing later preset or descriptor implementations.
A focused manifest regression first fails against the extraction and passes after
the registration fix. All existing tests remain byte-identical and all 101 prior
runtime commands remain unchanged; the manifest-only contract is the 102nd case.
Fresh Debug CMake build/102 CTest contracts pass (19.31s), scoped metadata ASan/UBSan
passes, and SwiftPM builds in 7.48s. No Qt executable was built/run; this verifies
source membership and portable linkage, not Qt runtime behavior. Production pin,
quarantines and protected applications are unchanged. No executable ran after
checkpoint publication. This correction supersedes the known broken intermediate
for standalone publication; later preset/schema stages must keep this registration
and extend its existing regression rather than add a duplicate test target.


## Delivery with the reference dependency fix included

The public delivery preparation combines the original catalog extraction with
the qmake metadata source/header registration and its failing-then-passing manifest
regression. It is based on PR #152 at `92cb45a2` and retains the reviewed OBJ scene
binding correction and prior delivery notes. Runtime and new test source files
match the corrected `69f82f50` checkpoint byte-for-byte. Independent token comparison
again confirms all 48 original biped rows, including generator tokens and pose tags,
retain their declaration order and values. The reference dispatch object compiles
for declaration drift checks but is never linked into a native product/test.

Fresh Debug CMake build and all 102 ordinary CTest contracts pass in 19.98 seconds. The 100
previous runtime commands are unchanged; fixed declaration metadata and reference
source membership are the only new contracts. Scoped metadata ASan/UBSan passes;
isolated metadata symbols contain no animation generator, rig or mesh implementation.
SwiftPM Dust3DEngine builds in 0.63 seconds. New C++/manifest formatting and the quarantine
source guard pass. No sampler or Qt executable is run, and no production pin changes.

These checks establish catalog declaration compatibility, not rig matrices, poses,
frame evaluation, playback, audio, production SDK/UI adoption or full M6 acceptance.
The next stage adds owned C transport under the existing exception/lifetime rules.
Completed clean exact-head automated review and all CI green remain mandatory
before merging this metadata foundation.


PR #152 subsequently merged at `ebcd8813` after completed clean exact-head review
and all six CI checks green. Full-tree equality before/after the ancestry-only
rebase proves source identity. No native binary executes after that rebase; only
this delivery note changes afterward.
