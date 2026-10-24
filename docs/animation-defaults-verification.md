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
