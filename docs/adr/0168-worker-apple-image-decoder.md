# ADR 0168: Apple worker image callback and shared metadata

Status: proposed for reviewed delivery. Tracks #267 and Apple #279.

## Decision and source ownership

Add Dust3DWorkerAppleImages, an Apple-only Swift6 product supplying synchronous
imported PNG/JPEG and procedural PNG callbacks for the existing C ABI. No mutable
context is retained, input spans are callback-borrowed and any returned native image
transfers to the caller. Envelope/policy checks precede pixel decoding and output
allocation; actual image shape/provider spans are checked before sample reads.
Output is tight straight RGBA8 in encoded row order without transforms, color
conversion, premultiplication or material-factor multiplication. Procedural keyed
2/4/8-bit grayscale zeros transparent RGB; imported pixels retain it. Preserve
existing palette and nearest16-to8 behavior. Original typed causes and stable numeric
codes distinguish envelope, layout, platform and quota failures; native creation
receipts are returned unchanged. Logical quotas exclude ImageIO workspace/RSS.

Foundation-only Dust3DImageMetadata contains PNG/JPEG envelope inspectors with no
engine/platform dependency. ImageIO/CoreGraphics appear only in the Apple callback
product, never portable C++ headers/targets. System frameworks add no external package.
A custom image codec was rejected because it would diverge from Apple pixel policy;
importing the Apple app package into core would create a dependency cycle.

The two metadata files and ImageSampleLayout are byte-identical MIT extractions from
accepted Apple revision2d24e720; provenance.json records source paths/hashes and a CTest
check prevents silent local drift. The old pinned Apple app must temporarily retain
its copies. A separately authorized future dependency PR must reexport shared metadata
from its document layer and use the platform module in infrastructure, removing old
implementations. Do not make the document layer depend on ImageIO or the engine.
This is staged consolidation, not permission to maintain divergent decoders.

## Evidence and limits

The positive callback regression started red on the missing decoder API. Existing
licensed 2x2 PNG/JPEG and independently encoded MIT Apple grayscale/palette/16-bit
samples pass straight alpha, hidden RGB, row order, opacity, palette and rounding
checks. Valid encoded PNGs with smaller scalar quotas reject before ImageIO and retain
typed original diagnostics; no malformed native codec inputs/faults are induced.

Fresh CMake build and212/212 ordinary CTest contracts pass (34.69s), including the
sample/provenance checks. Strict Swift6/warnings-as-errors and formatting lint pass.
Actual SwiftPM product build passes (1.63s), and a separate consumer imports/links
its public module/archive and passes the same samples. Eleven source hashes match.
Scoped Swift ASan passes with leak detection disabled and native libraries uninstrumented.
Coverage: callback142/166 lines85.54%,49/78 regions62.82%; sample layout59/61 lines96.72%,
27/36 regions75%. Swift emits no branch outcomes. Unforced native/platform failure
guards remain; this does not establish whole-core coverage or hardened codec behavior.

This stage supplies a tested library, not default XPC/app adoption. Actual worker
wiring, material/full-output integration, Apple #279/#280 ranges, distribution and
M4–M8 remain open. Existing8192-side metadata caps are retained, not claimed as range
parity. Production pin48b0ce2d and all quarantines remain unchanged. No GLB/native
archive generation, crash/fault/signal/cancellation/pressure probes were added.
Completed clean exact-head automated review and all CI remain mandatory before merge.
