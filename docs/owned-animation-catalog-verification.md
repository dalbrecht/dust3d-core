# Owned animation catalog verification

Tracking: [Apple #145](https://github.com/dalbrecht/dust3d-swiftui/issues/145).
The API transports inherited declarations; it does not evaluate them.

The public C owner independently copies 180 ordered clips (including four legacy
aliases), numeric overrides and seven rig/standard-bone lists. Its records expose
borrowed strings only until destroy. Clip and parameter accessors return 0 and
clear provided output on invalid owner/index; rig/bone accessors return NULL/zero.
Null destroy is harmless. No document/asset input or generator is involved.

Evidence:

- Missing API declarations fail the first compile before implementation.
- C API contract walks all current records, validates scalar/identity views and
  tests null owners/destinations, ordinary index boundaries, clearing and independent
  owner/copy lifetime. It does not dereference released storage or inject failures.
- Existing C++ tests match the audited c7d1d681 parent. New C/Swift tests are source
  reviewed; the optional session verifier only appends the new metadata fixture.
- Shared diagnostic/mutex bodies compare unchanged with their prior definitions.
- Quarantine guard, full Debug CMake build and all 102 ordinary CTest contracts
  pass (19.76 seconds). Scoped API ASan/UBSan passes using fresh ordinary metadata.
- C11 warnings-as-errors public-header syntax, new C++ formatting, Swift formatting
  and shell syntax checks pass.
- SwiftPM Dust3DEngine builds in 7.42 seconds. The strict Swift 6 importer fixture
  compiles with complete concurrency and warnings-as-errors, copies all fields
  before owner destruction, checks independent loads and transfers values across
  an actor. It uses the current header and fresh archive via an explicit smoke
  module map, linking the already-declared C++/zlib dependencies.
- Standalone C API test symbols contain no mesh, rig or animation generator code.
  This symbol claim applies to that isolated contract, not the full SwiftPM archive.

Not verified: construction/allocator failure paths (not forced), full inherited
C++ coverage, the entire optional Swift session verifier, production Apple SDK/UI
adoption, Qt execution, parameter schema/ranges/units, biped transition tags,
rig matrices, pose/frame evaluation, playback or audio. Those remain M6 acceptance
work. No production pin, quarantined path, protected app or global SVG is changed
by the metadata implementation. Ordinary legacy core fixtures retain their existing
behavior; no global SVG restore was attempted.


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


PR #153 subsequently merged at `c8a3da3a` after completed clean exact-head review
and all six CI checks green. Full-tree equality before/after ancestry-only rebasing
proves source identity. A transient Git pack-index warning was followed by passing
connectivity/full integrity checks and verification of every gated source hash;
no repair or pruning was needed. No native binary executes after rebase; only
this delivery note changes afterward.
