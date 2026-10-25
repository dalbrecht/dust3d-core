# Owned animation control transport verification

Status: Local C/Swift transport; production adoption pending.
Tracking: [Apple #145](https://github.com/dalbrecht/dust3d-swiftui/issues/145).

Extend the opaque animation catalog with an owned copy of each ordered dynamic
control definition. A separate control descriptor preserves the existing clip
structure and keeps preset overrides distinct from editable schema. Names and
labels are borrowed until catalog destruction; Swift copies all strings/scalars
before releasing the owner. No STL or callback crosses the C ABI.

Checked forward/inverse operations use the portable conversion functions. They
clear outputs and diagnostics, reject invalid owners/indices/output pointers,
and retain the original typed conversion cause in the C diagnostic. Integer
representation is checked before casting; widget range is checked after
representable truncation. Nominal defaults remain distinct from quantized values;
merely inspecting a descriptor must never rewrite a stored document value.

The missing-API compile regression preceded implementation. The new ordinary
contract covers all 180 clips/1592 definitions, independent owners, copied labels,
null/index boundaries, output clearing, endpoint/default conversion and rejection
by the new checked API. Invalid inputs never reach legacy reference callbacks.
Swift copies descriptors into immutable Sendable values and verifies independent
loads after owner destruction and actor transfer.

Full Debug CMake build and all 106 ordinary CTest contracts pass (19.98 seconds).
Scoped API ASan/UBSan, C11 header syntax, formatting, SwiftPM build (7.71 seconds)
and strict Swift 6 complete-concurrency/warnings-as-errors consumer pass. The
isolated C API contract has no mesh/rig/sampler/reference callback symbols. This
claim does not extend to the full SwiftPM archive. Inherited SwiftPM C++ precision
warnings remain; no whole-core coverage or distribution ABI claim is made.

No allocator failures were injected. The full optional Swift session verifier,
Qt execution, production SDK/UI adoption, common timing/audio controls, runtime
sampling/playback/audio and full M6 acceptance remain open. Production pin and
all quarantines are unchanged. No protected app or global SVG restoration ran.

Validation logs, source hashes and the initial failing compile are retained in the local owned-animation-controls checkpoint. Existing runtime commands remain unchanged, with one additional reviewed control API contract. No executable ran after checkpoint publication.


## Owned control delivery validation

Prepare the corrected owned descriptor API on core #156 at `63448227`, retaining
the accepted OBJ scene-binding verifier fix and the single reference-manifest
contract. Runtime and new test sources match corrected `5a67d3e3`. The C owner
copies the ordered descriptor vectors and strings. Borrowed views remain valid
until destruction; Swift copies every scalar/string before deferred destruction.
Checked conversions clear outputs and diagnostics, validate owner/indices, call
the guarded metadata API and preserve original conversion causes at the C ABI.

Fresh Debug CMake build and all 106 ordinary CTest contracts pass (23.30 seconds).
All 105 prior registered commands remain unchanged, with one inspected C API
contract added. Scoped metadata ASan/UBSan, C11 header syntax, C++ and Swift
formatting pass. SwiftPM builds the current Dust3DEngine archive in 7.75 seconds;
a fresh strict Swift 6 complete-concurrency/warnings-as-errors consumer verifies
1592 copied controls, independent owners, lifetime and actor transfer. Isolated
contract symbols contain no sampler, rig, mesh or reference callback implementation.
No legacy invalid cast, forced allocation failure or quarantined runtime is tested.

Production pin and quarantines remain unchanged. This is descriptor transport,
not editor UI adoption, Qt runtime verification, animation evaluation/playback,
audio or full M6 acceptance. Require accepted parent #156, completed clean
exact-head automated review and every CI check green before delivery merge.
