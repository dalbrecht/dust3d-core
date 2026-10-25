# ADR 0105: Separate audio surface identities from sound synthesis

Status: Local metadata/transport verified; production adoption and audio runtime pending.
Tracking: [Apple #145](https://github.com/dalbrecht/dust3d-swiftui/issues/145).

Move the existing SurfaceMaterial enum and exact name conversion functions from
sound_generator.h/.cc to surface_material.h/.cc, retaining original copyright/MIT
notices and enum order. Function bodies compare unchanged apart from whitespace.
The old header includes the new one, preserving existing source declarations.
No synthesis parameters, noise generation, PCM or WAV code joins native targets.

surfaceMaterials returns a fixed array in the original eight-item reference UI
order. The reference combo now uses that list instead of maintaining string rows.
Inherited name parsing remains exact and falls back to Stone for unknown strings;
this is an execution default, not permission to rewrite unknown stored text.
Play Sound remains a preview setting, initially false; it is not invented as a
stored document parameter. Source strings and selection behavior stay unchanged.

The owned animation catalog copies the eight names while holding its existing
construction lock. C accessors validate owner/index and borrow names until owner
destruction. Swift copies each UTF-8 string into immutable Sendable arrays before
release, compares independent catalogs and verifies actor transfer. Metadata does
not assert audio generation is available.

CMake, SwiftPM and qmake explicitly register only the new metadata implementation.
Standalone catalog API contracts gain that dependency; no synthesizer is linked.
The reference source-membership regression extends to the new qmake source.
Missing-header/API compilation failed before implementation. The new contract
checks source enum/name order, exact parsing/fallback, copied C names, null/index
boundaries and copied values after owner destruction. Existing C++ tests remain
byte-identical; the existing source-membership and Swift fixtures are extended.
All 108 previous runtime commands are unchanged, plus one reviewed metadata case.

Fresh Debug CMake build and all 109 ordinary CTest contracts pass (19.41 seconds).
Scoped metadata ASan/UBSan, C11 public-header syntax, formatting, SwiftPM build
(7.61 seconds) and strict Swift 6 complete-concurrency/warnings-as-errors consumer
pass. The isolated contract contains no SoundGenerator, synthesis-parameter,
noise, animation generator, rig or mesh implementation symbols. This symbol claim
is scoped to the isolated contract, not the full SwiftPM archive. Reference sound
source syntax checking passes; no sound object is linked or executed by that check.

Full Qt widget build/runtime, full optional Swift session verifier, production
SDK/UI adoption, audio generation/output/synchronization, runtime animation and
complete M6 acceptance remain open. No whole-core coverage claim. Production pin
and all quarantines stay unchanged; no protected app control, failure injection or
global SVG restore occurs. Use the corrected earlier catalog foundation69f82f50
when publishing the stack, preserving/extending its reference dependency test.


## Audio surface metadata delivery validation

Replay corrected `b56b8817` on the locally validated timing C API delivery.
Runtime and new test sources match that corrected stage; accepted parent notes
and the scene-binding verifier fix are retained. Independent token comparison
preserves both inherited surface conversion function bodies and the enum. The
shared eight-name list retains reference widget order. Reference sound source is
syntax-checked only; native targets explicitly include the portable surface file.

Fresh Debug build and all 109 ordinary CTest contracts pass (20.34 seconds).
All 108 prior registered commands remain unchanged, with one inspected metadata
contract added and existing reference membership guard extended. Scoped metadata
ASan/UBSan, C11 syntax, C++/Swift formatting and quarantine checks pass. Fresh
SwiftPM archive builds in 8.12 seconds, followed by a strict Swift 6 complete-
concurrency/warnings-as-errors consumer verifying copied surface strings after
owner destruction and actor transfer. Isolated symbols contain no synthesizer,
sampler, rig, mesh or reference callback implementation. No audio generation,
playback, fault/crash probes, Qt execution or protected-app control occurs.

This stage remains local until its parents merge with completed clean exact-head
automated review and all CI green. Production pin and all quarantines stay
unchanged. Editor catalog adoption, runtime sampling/playback/audio and full M6
acceptance remain open; metadata transport is not full feature parity.


Core #159 merged at `2f3b7168` after completed clean exact-head automated review and all six CI checks green. Full-tree equality across ancestry-only rebase and matching gated source hashes preserve the tested implementation. No native binary executes after rebase; only this delivery note changes afterward.
