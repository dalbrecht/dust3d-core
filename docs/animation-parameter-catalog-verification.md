# Checked animation parameter catalog verification

Tracking: [Apple #145](https://github.com/dalbrecht/dust3d-swiftui/issues/145).
Scope is reference dynamic control metadata and new checked native conversion.

| Type / parameter | Slider bounds | Divisor / multiplier | Nominal default / index |
| --- | --- | --- | --- |
| BirdGlide / wingSpreadAngle | 0–300 | 667 / 1 | 0.15 / 100 |
| FishDie / hitFrequency | 20–300 | 100 / 8 | 8 / 100 |
| FishDie / spinDecay | 10–300 | 100 / 4 | 4 / 100 |
| BipedIdle / breathingAmplitudeFactor | 0–300 | 100 / 1 | 0.65 / 65 |
| QuadrupedRunBackward / travelAngleDegrees | −180–180 | 1 / 1 | 180 / 180 |
| InsectWalk / gaitSpeedFactor | 1–8 | 1 / 1 | 1 / 1 |

Forward conversion divides the index then multiplies. Inverse conversion divides
by multiplier then multiplies by divisor, checks finite/integer representation,
truncates toward zero and checks widget bounds. Definitions are validated before
arithmetic. Only the guarded result is cast to int. The metadata does not promise
an exact numerical round trip: a 0.15 nominal default and 100/667 projection differ.
Displaying a projected control is not permission to rewrite retained document text.

Evidence:

- Missing header and absent qmake source membership fail before implementation.
- Token/quoted-label comparison preserves original table data and derivation,
  allowing the documented adapter substitutions and two explicit custom scales.
- The ordinary native contract covers 1592 definition instances across 180 types;
  all current defaults project inside their widget ranges. Unknown schemas remain
  empty; variants, overrides, direct/nondecimal/custom scales have independent examples.
- New guarded functions reject NaN/infinity, unrepresentable finite values,
  out-of-range indices and invalid definitions. Exact int endpoints and fractions
  whose truncated integer is representable succeed. No invalid legacy cast is run.
- Quarantine guard passes. Existing runtime commands remain unchanged except the
  added native contract; reference source-membership content adds the new catalog.
- Full Debug CMake build and all 105 ordinary CTest contracts pass (19.97 seconds).
- Scoped new native schema/math ASan/UBSan passes. Its binary contains no sampler,
  rig, mesh algorithm, reference getAnimationParameterDefs or std::function callback.
- Reference callback interface compiles only as an unlinked object; no Qt execution.
- SwiftPM product builds in 0.97 seconds; source/manifest formatting passes.

Not verified: full Qt build/run/cache behavior, invalid reference callback handling,
common duration/frame/audio controls outside this table, C/Swift descriptor copies,
native UI/lossless interaction acceptance, pose/runtime/playback/audio and full M6.
No broad inherited C++ coverage, production pin change or quarantine release is claimed.
Earlier catalog publication must include its qmake dependency backport first.
