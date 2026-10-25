# Generator-free preset verification

The new pure helper API preserves inherited numeric-string/default behavior while
removing the descriptor port's dependency on the sampler translation unit.

- Missing helper header fails before implementation; the reference manifest test
  separately fails for the genuinely missing biped metadata source registration.
- AnimationParams and both preset method bodies compare unchanged with their old
  definitions. Reference class methods forward to the pure helpers.
- The ordinary contract covers all 180 inherited types, timing, durationSeconds/
  frameCount, numeric overrides, independent maps, directional and alias behavior,
  unknown timing/empty-parameter fallbacks and boolean/missing-field semantics.
- Actual quarantine guard passes. Existing test sources match audited efa4d4a3;
  all 102 prior CTest commands remain unchanged, with exactly two reviewed cases added.
- Full Debug CMake build and 104 ordinary CTest contracts pass in 19.90 seconds.
- Scoped preset ASan/UBSan passes on ordinary metadata; symbol inspection finds
  no sampler, rig or mesh algorithms in the standalone contract.
- SwiftPM builds Dust3DEngine in 0.68 seconds in the same shared build directory.
  C++/Swift manifest formatting passes. No production pin changes.

The qmake manifest now lists required metadata/default definitions. Full Qt
compile/link/run remains unverified. The earlier c7d1d681 catalog delivery must
include the metadata-only qmake registration/focused manifest regression before
publication. Its old intermediate dependency omission must not ship separately.

Not verified: unchecked legacy inverse slider callbacks, arbitrary-input validation
through legacy tolerant getters, parameter descriptors, C/Swift descriptor copy,
UI adoption, sampling/poses/playback/audio and full M6. No invalid inverse cast,
fault/crash/resource probe or protected-app control was performed. Existing
quarantines stay in force. No broad inherited C++ coverage claim is made.


## Generator-free defaults delivery validation

Prepare this stage on PR #154 at `429a8300`, preserving the accepted catalog
foundation and its existing reference source-membership contract. That contract
now also checks animation_defaults.cc; no duplicate qmake source or membership
test is added. Runtime and new test sources match corrected `19c853c1`. The
AnimationParams type and both default function bodies retain their original tokens.
Reference methods forward to the new portable functions rather than maintaining
a second table. Inherited tolerant getters remain compatibility behavior, not
input validators, and no unknown stored document value is normalized.

Fresh Debug CMake build and all 104 ordinary CTest contracts pass in 19.41 seconds.
The 103 previous commands are unchanged, with one fixed-defaults contract added
and the existing manifest check extended. Scoped metadata ASan/UBSan and SwiftPM
(7.79 seconds) pass; isolated defaults symbols contain no sampler, rig or mesh
implementation. New C++/manifest formatting and the source quarantine guard pass.
The reference dispatch remains compile-only; Qt and animation sampling are not run.

These results cover declared timings, string-formatted defaults, aliases,
directional overrides and independently owned return values. They do not establish
raw-input validation, native parameter UI, runtime pose/playback/audio parity or
full M6 acceptance. Production pin and all quarantines remain unchanged. Require
completed clean exact-head automated review and all CI green before merge.
