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


## Timing API delivery validation

Replay the corrected scalar C/Swift API on the validated common-timing delivery,
retaining accepted parent documentation, scene-binding verification and the single
reference manifest contract. Runtime/new test source matches corrected `a0782d1e`.
The C API copies static limits, clears outputs/diagnostics and catches guarded
conversion errors while preserving their original cause. No frame allocation,
legacy callback or animation evaluator executes.

Fresh Debug build and all 108 ordinary CTest contracts pass (20.28 seconds): all
107 prior commands unchanged, one inspected scalar C API contract added. Scoped
C API ASan/UBSan, C11 header syntax, C++/Swift formatting and quarantine checks
pass. Fresh SwiftPM archive builds in 8.31 seconds; a strict Swift 6 complete-
concurrency/warnings-as-errors consumer verifies copied timing limits, unchanged
1.125-second duration, independent owner lifetimes and actor transfer. Isolated
contract symbols contain no sampler, rig, mesh or reference callback implementation.
No production pin/quarantine changes or broad runtime/coverage acceptance claim.
This stage remains local until its parents merge after completed clean exact-head
automated review and all CI green. Audio metadata, editor adoption and full M6
runtime acceptance remain pending.


Core #158 merged at `40f98cee` after completed clean exact-head automated review and all six CI checks green. Full-tree equality across ancestry-only rebase and matching gated source hashes preserve the tested implementation. No native binary executes after rebase; only this delivery note changes afterward.
