# ADR 0176: Native-budget ordinary indexed copy

Status: proposed for reviewed delivery. Tracks #270 and Apple #279/#280.

## Decision

Add `copyNativePreviewIndexedMesh` for a live immutable native indexed owner and
independently captured archive/native policies. Keep existing methods, legacy
wire/count limits, opcodes and active routes unchanged.

Validate captured controls before native access. Check nonempty borrowed storage,
membership against captured archive bytes, native record charges and addressability
before retaining arrays. Validate warning UTF8, every vertex/index and triangle area
directly against borrowed records. Construct independent owned values after preflight,
without a full encoded mesh buffer for validation. Caller retains the immutable owner
through return; no borrowed data escapes.

Indexed zero/default256MiB and positive addressable policies retain native semantics.
Warning storage is separate from native record charges and preserves the UInt32 wire
field, without the incidental4096-byte default. Native warning storage is trusted
immutable NUL-terminated storage, not an arbitrary foreign pointer. Current warning
literals and the archive-derived membership bound are documented in ADR0175.

Source-pair copying, native full-budget generation/session dispatch, output channels,
C/Swift adoption and authentication remain open. This boundary does not close #270.
Capacities, roots, native lookup workspace and RSS are outside record accounting.

## Evidence and limits

The new API first failed to compile in the existing licensed small Capsule contract.
Passing checks compare canonical geometry bytes, exact native-budget acceptance,
one-byte-smaller copy-only rejection and owned lifetime after native/archive release.
Maximum-addressable explicit policy uses only the small actual mesh. Inconsistent
captured membership and invalid-control ordering reject without changing native input.
No large allocation or new native fixture is used.

Fresh build and221/221 CTest pass (35.12s). Actual SwiftPM runtime builds (2.64s) and
the updated contract links/runs against that actual static product. Both C++ formatters,
strict package formatting, quarantine guard and nine unchanged source/build hashes pass.
Scoped ASan/UBSan instruments this translation unit and contract; linked engine/other
libraries are uninstrumented and leak detection is off. Only the existing small positive
native fixture and ordinary copy-policy checks run.

New function29/30 regions,18/22 branch outcomes; vertex conversion closure1/1 region.
Residuals guard inconsistent opaque-owner storage and the alternate membership
disjunction. Do not forge opaque handles or inject native faults to cover them. Shared
pure validators retain protocol/scalar coverage. The translation unit also contains
legacy execution/exception paths outside this focused contract; this is not full-core
coverage, whole-engine sanitizer or full-range runtime/performance acceptance.

Apple pin48b0ce2d and core67/69GLB,83writer,84fault,95animation and Apple199rig-pose
quarantines remain unchanged. No malformed-native-input, crash/fault/allocation-failure,
signal/broken-pipe, cancellation, pressure or resource-exhaustion probes were run.
