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
