# ADR 0103: Share common timing widget limits without evaluating frames

Status: Local metadata/edit-validation foundation; native adoption pending.
Tracking: [Apple #145](https://github.com/dalbrecht/dust3d-swiftui/issues/145).

The reference duration and frame-count widgets are separate from dynamic sliders.
Move their unchanged inclusive bounds, decimal precision and step increments into
CommonTimingDefinition in the portable parameter catalog. The reference widget
uses these values, preventing independently maintained limits. Initial widget
values and per-clip defaultTiming remain distinct and unchanged. No new source
registration or dependency is required; the existing parameter catalog is already
registered in qmake, CMake and SwiftPM.

checkedTiming validates explicit edits only: both inputs must be finite and inside
widget bounds, then frame count must be integral before integer conversion. Its
maximum is 4999, safely inside C++ int representation. It returns the duration
unchanged: two display decimals are not permission to round a stored 1.125 value.
Unknown/raw document values must continue to survive inspection and save. No
existing document parser or reference load callback invokes this validator.

The missing API contract failed to compile before implementation. Tests inspect
actual reference limits and check valid endpoint/interior values, exact duration
preservation, typed finite/range/fractional rejection. These are scalar values;
no frames, rigs, mesh or audio are generated. All prior test sources and all 106
runtime commands are unchanged; the new scalar contract is the only added case.
The initial graph comparison ran before the cleaned build finished restoring
executables and failed; the post-build graph comparison passes exactly.

Fresh Debug CMake build and all 107 ordinary CTest contracts pass (19.56 seconds).
Scoped scalar ASan/UBSan and SwiftPM portable product build pass; formatting and
quarantine source guard pass. Full Qt widget compilation/execution is unverified,
as are whole-core coverage and production C/Swift SDK/UI adoption. Timing does
not establish frame evaluation, playback, audio or M6 acceptance. Production pin,
quarantines and protected applications remain unchanged; no SVG restore runs.


## Common timing delivery validation

Replay the corrected timing stage on the locally validated owned-controls delivery
`2cafe4df`, retaining accepted parent fixes and the single reference manifest
contract. Runtime/test source matches corrected `114e8e68`. Shared constants retain
the exact reference widget limits, steps and display precision. checkedTiming
rejects nonfinite, out-of-range and fractional frame counts before its bounded
integer cast; duration values are never rounded to display precision. Tests are
scalar-only and never allocate frames, invoke samplers or evaluate poses.

Fresh Debug build and all 107 ordinary CTest contracts pass (21.47 seconds), with
all 106 prior commands unchanged and one inspected scalar contract added. Scoped
metadata ASan/UBSan and SwiftPM Dust3DEngine (0.73 seconds) pass. Formatting,
quarantine guard and isolated symbol checks pass; no sampler, rig, mesh or
reference callback implementation appears in the standalone contract. Full Qt
build/runtime, C/Swift timing transport, native controls and M6 runtime acceptance
remain pending. No production pin or quarantine changes. This stage remains local
until its parents merge with clean completed exact-head review and all CI green.
