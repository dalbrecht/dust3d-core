# ADR 0109: Carry bounded owned UV metadata through a separate wire schema

Status: Local implementation validated; serial public review pending.
Tracking: [core #162](https://github.com/dalbrecht/dust3d-core/issues/162).

## Scope and wire contract

The base worker mesh has expanded geometry only. Add a standalone UV metadata
codec without changing the current frame operation, helper dispatch or Apple pin.
The caller supplies the nonzero geometry triangle count; frame/document/revision
admission and combined geometry correlation remain a later operation contract.

Version one uses a 56-byte prefix: 16-bit version and prefix length, 32-bit zero
flags, then five unsigned 64-bit fields (triangle/component/chart/gradient counts,
aggregate component-key text bytes) and binary64 packed texture size. All fields
are big-endian. Triangle records are 64 bytes: six binary64 UV coordinates, known
and association bytes, six zero reserved bytes, then a uint64 component index.
Chart records are 120 bytes: left/top/width/height and RGBA binary64 values, flipped
and association bytes, six zero bytes, uint64 component index, then image length
byte, up to 38 identity bytes and zero padding to 40 bytes. Gradients are 80 bytes:
uint64 chart index, two RGBA binary64 arrays and uint64 resolution. Component keys
follow in original order, each with a uint32 byte length and exact UTF-8 bytes.
There is no native struct dump, platform size field, pixel buffer or warning text.

Known mapping and source association are independent. Unknown/ambiguous association
requires UINT64_MAX; component association references an original key index. Known
UVs never prove unique source ownership. Unknown mappings retain zero coordinates.
Original opaque keys retain exact spelling, including distinct Unicode spellings;
only exact duplicate keys reject. Optional image identities retain supported UUID
spelling/case. Component keys are not forcibly parsed or canonicalized as UUIDs.

## Admission and ownership

Validate supported positive policies and counts before text scanning. Checked
multiply/add charges bound complete wire spans and logical records before field
reads. On supported platforms, the hard wire cap fits size_t by compile-time proof.
The key loop reserves four prefix bytes per remaining key plus unconsumed declared
text; this invariant bounds prefix/length reads without redundant unreachable
checks. Validate all metadata and aggregate text/logical quotas before constructing
retained output. Small chart-string and duplicate-key validation workspace remains
separate from retained payload quotas. No process/RSS or allocator-capacity claim.

Reject unsupported version/prefix, nonzero reserved/padding bytes, truncation,
trailing data, mismatched geometry count, unknown flags/associations, invalid
indices, nonfinite/out-of-unit UV/colors/chart bounds, impossible mapped-without-
chart state, invalid texture scale, invalid gradient references/zero resolution,
invalid UTF-8/control bytes, invalid image identities and inconsistent key spans.
Chart extents permit only 1e-9 sum tolerance for normalized packing roundoff.
Packed texture size is retained, not silently capped at the requested texture
size. Gradient resolution is an opaque positive descriptor here; no allocation or
sampling uses it, and future pixel consumers must enforce their own work budgets.

The encoder validates before allocating returned wire storage. The decoder checks
complete spans and semantics before allocating retained arrays/strings, then copies
all values into independent ownership. Error types retain static original causes
without embedding document text. No partial result is returned on rejection.

## Evidence

The first contract failed on the missing header. Independent value fixtures assert
prefix offsets, explicit binary64 bytes including negative zero, association
sentinels, chart/key layout and owner independence. Small malformed metadata and
exact/just-under independent quotas cover Unicode, flags, ranges, references,
counts, reserved spans, duplicate text and unsupported policies. No native invalid
input or resource-exhaustion workload is used. Unreachable arithmetic/span checks
were replaced with compile-time/loop invariants; no dead coverage branches added.

A fresh licensed Capsule UV owner is copied to the new metadata values, destroyed,
encoded/decoded independently, then the source/wire are cleared. All corner double
bits, key spellings and complete metadata serialization survive owner release.
This proves that ordinary owner workflow only, not arbitrary document parity or
combined geometry/frame authentication. No UV worker opcode or runtime is enabled.

Source quarantine guard and registered-source audit pass. Fresh full CMake build
and 114/114 ordinary CTest contracts pass 21.84 seconds. Fresh scoped strict-warning
ASan/UBSan passes the value-only codec tests. Scoped LLVM coverage reports
WorkerUv.cc at 343/343 lines, 299/299 regions, 22/22 functions and 272/272 branch outcomes.
This does not instrument the inherited engine/native-owner integration. The
unchanged SwiftPM product compatibility build passes incrementally in 0.09 seconds;
worker codecs are development CMake infrastructure. Changed-file formatting and
diff checks pass. Header API documentation only changes after the gate, with
comment-stripped code equality recorded; other gated source hashes are unchanged.

## Remaining work and constraints

Add a source-correlated geometry/UV operation, then material/image/pixel payloads,
parent semantic admission and actual renderer integration through reviewed stages.
Strong sandbox/memory/descendant/parent-death limits, trusted helper packaging,
active lifecycle acceptance and full M4/M8 remain open. Preserve core #67/#69 GLB,
#83 writer, #84 faults, #95 animation and Apple #199 rig-pose quarantines. No crash,
signal-delivery, deliberately broken-pipe, allocation-failure or resource-exhaustion
probe, stale executable, production pin change or protected QA control occurs.
