# ADR 0146: Joined imported render worker values

Status: Accepted for internal values; producer/transport/editor adoption pending.

## Context

Issue #237 joins the imported surface (#235) and material fact (#233) values to
represent the existing authored rendering path. Apple returns definitions in
surface model-catalog order and retains encoded bytes alongside decoded samples.
Separate valid payloads alone do not prove their identities or image presence match.

## Decision

`ImportedRenderPayload` owns a surface, an ordered material definition catalog and
one optional `ImportedRenderImage` per model. Definitions preserve the full factors,
sampler intent and encoded PNG/JPEG bytes. Images contain width, height and tightly
packed straight RGBA8 in source scanline order. No material factors, orientation,
RGB conversion or premultiplication are applied by this codec. Transparent RGB is
retained. Encoded bytes are opaque; supplied pixels are not proof of decoding.

The material catalog must have no instance bindings and must match the surface
model count, exact UTF-8 identities and order. An image is present exactly when its
definition has an encoded image. Present dimensions are positive, tight byte count
is width × height × 4, checked without overflow and bounded by the 256 MiB hard
payload ceiling. Absent records have zero dimensions and byte length. An empty
catalog remains representable; deciding whether the archive uses imported rendering
is a producer responsibility.

Complete inner inspection precedes cross-catalog checks and image validation.
New internal catalog visitors independently inspect their entire input and expose
borrowed stable identities/image presence. The join keeps only bounded borrowed
catalog workspace before allocating decoded surface/material/image arrays. The
input span must remain immutable and readable for the call. Callbacks are required
internal adapters and may propagate original errors. No pointer escapes decoded
results. Original surface/material errors remain distinct typed causes.

The big-endian prefix is 40 bytes: u16 version 1, u16 prefix size 40, zero reserved
u32; u64 surface length at 8, material length at 16, image count at 24 and aggregate
pixel bytes at 32. Body is the complete existing surface schema, existing material
schema, then image records in catalog order. Each record has u32 width, u32 height,
u64 pixel length and exact pixels. Complete cursor consumption plus outer size and
record count proves the pixel aggregate algebraically; no redundant comparison is
used to create unreachable validation branches.

Default limits are wire 256 MiB, aggregate logical 128 MiB and decoded pixels
64 MiB, each independently configurable within positive 256 MiB bounds. Inner
surface/material limits remain independent. Logical charge is both inner logical
charges plus one `optional<ImportedRenderImage>` per model and exact pixels.
Roots, capacities, allocator overhead, temporary encoded buffers, validation
workspace, platform decoding and process RSS are excluded. These limits do not
establish supported-range parity with Apple (#280).

## Verification and remaining gates

TDD began with missing-header failure. Bounded pure tests cover storage lifetime,
encoded bytes beside transparent straight RGBA, absent/multiple/empty catalogs,
exact quotas, original ordering, unexpected bindings, dimensions, image presence,
truncation, inconsistent framing and inner/cross-catalog mismatches. No new native
archive or image decoding probes run in these tests.

All 162 ordinary CTests pass (37.81 seconds), with eight source/build-spec hashes
unchanged across the final full gate. SwiftPM runtime builds (1.02 seconds); the
existing native-builder deprecation remains. ASan/UBSan pass with leak detection
disabled. Named joined codec coverage is 173/173 lines, 119/119 regions, 11/11
functions and 102/102 branch outcomes. This is not whole-engine coverage or a leak
claim. Existing inner codec regressions and ordinary ownership fixtures also pass.

No request opcode, C decoded kind, platform decoder, native joined producer, Swift
consumer, backend activation or production pin change is added. A future producer
must load surface and definitions from one unchanged archive; identity equality
and counts cannot authenticate provenance. PNG/JPEG decoding must run inside the
eventual Apple worker, outside portable C++. Real generation-session metrics,
supported-range parity, transport/Swift ownership and full M4–M8 acceptance remain
open. All quarantines remain intact.
