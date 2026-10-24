# ADR 0104: Expose scalar timing metadata and validation through C

Status: Local transport verified; production dependency/SDK/UI adoption pending.
Tracking: [Apple #145](https://github.com/dalbrecht/dust3d-swiftui/issues/145).

Add value-copy CommonTimingDefinition/CheckedTiming equivalents to the C header.
The metadata accessor needs no catalog owner or allocation. The edit validator
contains exceptions, initializes optional diagnostics, clears failed outputs and
retains the original finite/range/integral error cause. Successful edits clear
prior diagnostics and preserve duration precision. No pointer ownership crosses
this boundary; existing clip/control layouts and functions remain unchanged.

These functions do not parse documents, allocate frames, sample animation or
validate old stored values on display. The reference widget limits are shared,
while per-clip defaults and document text retain their separate meanings.

A missing-API compile regression preceded implementation. The scalar C contract
checks copied limits, null outputs, optional diagnostics, typed cause text,
failed-output clearing, success after errors, endpoint scalars and unchanged
1.125-second edits. The Swift 6 smoke fixture copies limits into immutable
Sendable scalars, uses checked timing, compares independent loads after catalog
owner destruction and transfers the complete catalog value across actors.

Fresh Debug CMake build and all 108 ordinary CTest contracts pass (19.89 seconds).
All 107 prior runtime commands are unchanged; only the reviewed scalar C contract
is added. Existing tests remain byte-identical except the extended Swift fixture.
Scoped ASan/UBSan, C11 public-header syntax, formatting, fresh SwiftPM build
(7.67 seconds) and strict Swift 6 complete-concurrency/warnings-as-errors consumer
pass. Isolated C contract symbols contain no generator, rig, mesh or reference
callback implementations. This does not describe the full SwiftPM archive.

No allocation failures were forced. Full optional Swift session verification,
Qt execution, production SDK/UI adoption, common audio controls, runtime animation,
playback/sound synchronization and complete M6 acceptance remain open. No whole-
core coverage claim is made. Production pin and all quarantines are unchanged;
no protected app control or global SVG restore occurs.

The corrected catalog-foundation-delivery branch remains the publication source
for the earlier metadata extraction. Preserve its qmake registration/regression
when transplanting later stages; this local branch retains the original stack.
