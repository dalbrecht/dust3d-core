# ADR 0099: Own declaration transport independently of generation

Status: Local C API/interoperability foundation; review and M6 acceptance pending.
Tracking: [Apple #145](https://github.com/dalbrecht/dust3d-swiftui/issues/145).

Dust3DAnimationCatalog owns copied ordered clip records and rig/bone records.
The loader accepts no user document, takes the shared engine mutex and copies the
fixed inherited catalog into local RAII storage before publishing one opaque owner.
It validates nonempty/NUL-free identities and finite positive timing/finite overrides.
C++ construction errors are caught; original causes reach the optional fixed
error buffer. No deliberate construction/allocation failure is injected. The
owner is immutable after publication; callers retain it until all readers finish.

clip_at/parameter_at fill fixed-layout C record views with borrowed strings and
scalar fields. Invalid owner/index/output rejects without unchecked indexing;
non-NULL record output clears first. Rig/bone accessors follow the same bounds
policy, returning NULL/zero. Strings expire on destroy, so Swift copies everything
before releasing the owner. No STL types, allocators or C++ exceptions cross the ABI.
This prototype follows exact-revision consumption and makes no stable distribution
ABI promise. The fixed compile-time catalog has no caller-controlled input size;
this API is not a process memory cap or a runtime generator acceptance surface.

The shared setError and engineMutex definitions previously lived in the mesh bridge
translation unit. Their unchanged bodies move into EngineSupport.cc, with thin
internal declarations. This preserves one shared lock and existing error behavior
while permitting the catalog bridge to link independently. The standalone C API
contract compiles only that support, the catalog bridge and two declaration sources.
Symbol inspection finds no MeshGenerator, RigStructure, RigAnimationClip or
AnimationGenerator. SwiftPM's complete static product can contain other core code;
the importer smoke invokes only metadata queries, not those algorithms.

TDD began with missing owner/view/function declarations. The ordinary contract
checks all 180 clips, seven rigs, aliases, finite values, null destinations/owners,
index boundaries/output clearing, independent native owners and copied lifetimes.
Existing C++ tests remain unchanged from the previously audited c7d1d681 parent;
the new C API contract and Swift fixture were inspected. The old optional Swift
verifier is unchanged except for an appended metadata compile/run block. Extracted
support helper bodies compare token-identically to their prior definitions.

The quarantine guard passes. Full Debug CMake build and all 102 ordinary CTest
contracts pass in 19.76 seconds. C11 header syntax, C++ formatting and Swift/shell
checks pass. Scoped API ASan/UBSan lifetime/bounds tests pass. SwiftPM builds the
library in 7.42 seconds; a strict Swift 6 warnings-as-errors fixture validates every
copied record and transfers post-destroy values across an actor. Initial importer
commands needed correction for the default build-system module-map location and
the existing zlib link dependency; no source/binary safety boundary was bypassed.
The successful command uses the current public header, an explicit smoke module
map and the freshly built default-system archive in the shared folder. The full
optional session verifier was not rerun. No C++ coverage or fault-path claim follows.

Production SDK/UI adoption, parameter descriptors/units, pose-entry/exit tags,
rig/pose/frame transport, Qt/native runtime interoperability, sampling/playback/
audio and complete M6 acceptance remain open. Core #67/#69/#83/#84/#95 and Apple
#199 quarantines, production pins and protected apps remain unchanged.


## Owned catalog delivery validation

Prepare this stage on PR #153 at `ae20c3d8`, retaining its qmake dependency fix,
reference source-membership regression and reviewed OBJ scene-binding correction.
Runtime and new test sources match the saved corrected `33ad3f93` checkpoint. The
shared diagnostic and mutex bodies compare byte-for-byte with the parent definitions.
The new owner copies fixed declaration data under that same construction lock;
getters validate owner/index and clear invalid destination views.

Fresh Debug CMake build and all 103 ordinary CTest contracts pass in 19.69 seconds.
The 102 previous commands are unchanged; one reviewed ordinary ownership contract
is added. Scoped C API ASan/UBSan passes without failure injection or freed-pointer
reads. C11 public-header syntax, new C++ formatting, Swift formatting and optional
verifier shell syntax pass. SwiftPM builds in 7.82 seconds. A strict Swift 6
complete-concurrency/warnings-as-errors consumer copies every field before native
owner destruction, compares independent loads and transfers immutable values across
an actor. Its explicit module map names the current public header and links the
fresh archive. The full optional Swift verifier is not rerun.

Isolated C API contract symbols contain no animation generator, rig or mesh
implementation; that does not describe the full engine archive. Construction
allocation/exception branches remain unforced. Full inherited coverage, Qt runtime,
production SDK/UI adoption, dynamic parameter schema and animation sampling,
playback/audio remain open. No production pin or quarantine changes are made.
