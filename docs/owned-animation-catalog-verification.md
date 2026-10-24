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
