# ADR 0141: Preserve baked texture values for worker transfer

Status: Accepted implementation, pending reviewed delivery.

## Context

Apple #279 requires full rendering output before worker backend activation. Indexed
geometry and source hints alone cannot preserve materials. The current source
baker exposes five tightly packed straight RGBA8 map roles, optional map presence,
base-inclusive mip chains and a chart-safe maximum sampling LOD. Reconstructing
that cap from dimensions would permit sampling across chart boundaries. Image
preparation currently uses Apple APIs, which cannot enter the portable core.

## Decision

Add the internal portable `WorkerBakedTextures` value/codec under core #227. Roles
retain order: color, normal, metallic, roughness, emissive. Color is mandatory;
other roles may be absent. Each present chain has 1–14 levels including its base.
Dimensions are 1–8192, tightly packed `width * height * 4` bytes. Subsequent levels
halve each dimension with floor/minimum one; no level follows 1×1. Unequal chain
lengths are allowed only when all support the producer's supplied cap. Pixel bytes,
including RGB beneath zero alpha, and finite nonnegative binary64 cap bits are
preserved. No filtering, normalization, orientation, alpha or color conversion.
Color/emissive retain their source encoding; data maps remain linear as supplied.

Version one uses explicit big endian fields, never serialized native structs:

| Offset | Field |
| --- | --- |
| 0 | UInt16 version 1 |
| 2 | UInt16 prefix size 64 |
| 4 | UInt32 reserved zero |
| 8 | UInt64 aggregate pixel byte count |
| 16 | UInt64 IEEE binary64 sampling-cap bits |
| 24 | UInt32 role count 5 |
| 28 | UInt32 aggregate level count |
| 32 | Five UInt32 per-role level counts |
| 52 | Twelve reserved zero bytes |

Then, in role/level order, each level has a 16-byte prefix: UInt32 width,
UInt32 height, UInt64 byte count, followed by exact pixels. There is no padding.
All counts, dimensions, shape, cap, aggregate quotas and exact framing validate in
fixed stack storage before decoded pixel/level arrays. Nonfinite cap bit patterns
are rejected before floating-point evaluation, including signaling NaNs. Independent
wire, pixel and logical quotas reject with original typed texture errors.

Logical charge is `levelCount * sizeof(TextureLevel) + pixelBytes`. Root objects,
container capacity, transient wire/native workspace, CPU and RSS are excluded.
Default pixel/logical quotas are 128 MiB; all configurable byte quotas are positive
and at most the current 256 MiB worker ceiling. These prototype bounds do **not**
establish supported-range parity with the editor; Apple #280 remains an adoption
gate. No global ceiling or existing family policy changes.

This stage does not bind textures to geometry, UVs, tangents, triangle policies or
model/component catalogs. It adds no native copy/generation, request opcode, C/Swift
consumer, PNG/JPEG decoder, Apple dependency pin or backend switch. A future Apple
worker may prepare decoded inputs with platform APIs inside its own boundary;
portable C++ remains free of Apple/Qt types. Complete rendering correspondence,
real persistent-session/cache statistics, payload-range preservation and authenticated
editor acceptance remain required under Apple #279/#280. Existing codec/opcode
versions are unchanged; this schema is not a published stable binary ABI.

## Validation and limits

- TDD red is a missing codec header. Bounded pure values cover all five roles,
  optional absent maps, odd/skinny mip dimensions, ownership after input release,
  fractional and negative-zero caps, transparent RGB, exact/short quotas, invalid
  count/dimensions/shape/cap, signaling/nonfinite cap bits, reserved fields, every
  truncation of a small valid packet and trailing bytes. No native engine executes.
- Initial coverage identifies redundant post-bound count validation; it is removed
  rather than introducing an unreachable test branch. Tiny-wire and skinny-chain
  cases complete meaningful boundary coverage. The final named codec measures
  **164/164 lines, 122/122 regions, 13/13 functions and 104/104 branch outcomes**.
  This does not measure inherited engine or total runtime coverage.
- Fresh final full CMake build and **153/153 ordinary CTests pass in 42.40 seconds**.
  Changed-source hashes remain stable across the gate. The prior pre-refactor full
  run also passed all 153 contracts. Existing healthy XPC fixtures run in their
  established authorized context; no new native/XPC probe is introduced.
- Refreshed scoped ASan/UBSan pure codec passes, leak detection disabled. SwiftPM
  runtime product builds in 0.98 seconds; existing native-builder deprecation is
  unchanged. C++/Swift manifest formatting, source quarantine and whitespace pass.
  All builds/caches remain within the single shared build root. No UI change.

Core #67/#69 GLB, #83 writer, #84 fault, #95 animation and Apple #199 rig-pose
quarantines remain. No new malformed-native-input, crash, allocation-failure,
signal, broken-pipe, native cancellation or pressure/exhaustion probes, stale
binaries, app control, global SVG or protected-QA/OS-container changes. Completed
clean exact-head automated review and all CI green remain required before merge.
