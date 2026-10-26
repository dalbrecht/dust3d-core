# ADR 0151: Complete legacy baking through the worker runtime

Status: Accepted for explicit internal use; Apple adoption pending.

## Context

Issue #247 connects procedural image preparation (#245) to the complete legacy
rendering path. The existing editor distinguishes legacy baking, authored imported
rendering and source-baked mixed materials. Substituting one would lose UV, tangent,
material or pixel behavior. Platform decoding must happen inside the Apple worker.

## Decision

Opcode 7 is `generateLegacyBaked`; opcodes 1–6 retain their values. A version-1
request has a 128-byte big-endian prefix: version u16 at 0, prefix u16=128 at 2,
reserved zero u32 at 4, archive length u64 at 8, then fourteen u64 fields at 16–120.
Their order is native UV bytes, native bake bytes, per-image encoded bytes,
aggregate encoded bytes, copied image-input bytes, decoded pixel bytes, copied
output bytes, output wire bytes, atlas size, color size, normal size, material size,
spatial-grid entries and asset-name bytes. The exact opaque archive follows.
Full framing, settings and quotas validate before retained archive copying.

Zero byte quotas select maintained defaults; positive quotas up to 256 MiB only
tighten receiver admission. Inner geometry/UV/texture quotas also tighten to the
outer copied/wire policy. Archive logical charge excludes the fixed options/root,
capacity, workspace and RSS. Current frame and C semantic limits still do not
establish Apple #280 supported-range parity.

Dimensions and grid are explicit bake settings, separate from byte quotas and host
work caps. Request settings replace receiver defaults without treating those
defaults as undocumented maximum dimensions. Zero selects native defaults, bounded
by independently configured host caps. Positive settings cannot exceed those caps;
they reject before native work. Maintained host maxima are 8192 per dimension and
16,000,000 grid entries. Native zero defaults resolve to atlas/color 4096, normal
2048, material 1024 and grid 1,000,000, each respecting its host cap. Hosts cannot
use a request to increase their own work caps.

The producer inspects one immutable archive through validated metadata extraction,
then prepares selected procedural image facts through a distinct decoder interface.
All counts and encoded totals preflight before the first callback. Callback receipts
are checked for complete tight straight RGBA8 shape, dimensions and aggregate pixel
quota in an independently testable preparation stage. Original exceptions survive.
Callbacks execute after the reader releases the engine lock. Independent pixel
owners remain alive through borrowed native bake-image views; canonical 38-byte
identities fit the fixed native ID buffer with its terminator. The serialized native
C baker supplies one owner; complete geometry, UV triangles/charts/gradients/keys,
tangent frames and optional base maps are copied from that owner before release.
No imported or source-baked substitution occurs.

`dust3d_worker_serve_with_decoders` accepts separate imported and procedural callback
slots. An all-zero slot is unavailable; present slots and the enclosing exact-sized
configuration validate before I/O. Imported callbacks preserve authored transparent
RGB. Procedural callbacks implement the existing Qt-compatible PNG grayscale
transparency rule. The role is known from the callback slot; no platform policy is
inferred from the image owner alone. Both share bounded image ownership/receipt
validation and borrowed encoded spans without an intermediate encoded copy. Existing
single-imported-decoder and ordinary entry points retain their contracts. A legacy
job without referenced images needs no provider; a required unavailable provider
produces an explicit correlated failure, never a parent fallback.

Child, service and queue admission retain existing identity, EOF and completion
gates before decoding a complete legacy payload. C admission independently clamps
outer and inner ownership policies. The distinct decoded kind exposes existing
geometry/UV getters plus new tangent, map-level, pixel and sampling-cap getters.
Map roles are color, normal, metallic, roughness and emissive. Legacy maps have at
most one level; absent maps remain absent. Getters clear required outputs on invalid
kind/index/range and borrow immutable spans only through owner destruction. Swift
must copy before release. Pixel totals are actual admitted storage, not fabricated
native generation/cache metrics.

The C request factory owns complete framing, catches exceptions and preserves
original typed diagnostics. The generic factory uses the same default codec and
rejects unrelated UV options. Request output wire quota also tightens to the caller's
wire policy. Framing completion can carry an operation failure and is never geometry
success or process/authenticated-publication admission by itself.

Detailed failure schema v2 appends domains 8 legacy bake, 9 procedural inputs,
10 baked textures, 11 UV, 12 legacy producer and 13 mesh. Existing domains and v1
encoding retain their values/bytes. Callback domain, signed code, original status,
explicit truncation and opaque cause bytes survive. Native statuses and available
bounded native diagnostic text survive; the legacy native C API does not expose
whether it truncated text before the worker received it. Worker-observable further
truncation is explicit. No full original-native-text guarantee is inferred.

## Verification and limits

TDD began with missing request/producer/operation/admission/C API declarations.
Pure tests cover complete framing, all fourteen option ranges, exact/short budgets,
independent byte tightening, explicit high supported settings and lower host caps.
Tiny fake providers cover wrong pixel shapes, exceeded receipts, preflight ordering,
exhausted tiny quotas and original errors without native generation or image decoding.
C tests cover child/service/queue ownership, complete UV/tangent/maps, original opaque
failures, caller copies after owner destruction, bounds/nulls and aggregate rejection.
Strict C consumers compile and exercise the new factory/configuration/getter types.

Ordinary Capsule and ProceduralImage fixtures with fake providers and explicit 8x8
bake settings verify the native producer and C-factory-to-descriptor-job path through
owned regular files in the shared build root. No real PNG correctness, Qt rendered
comparison, new malformed-native-input, crash, allocation-failure, signal, broken-pipe,
cancellation or resource-pressure probe is added. All existing quarantines remain.

All 184 ordinary CTests pass (43.09 seconds), with 42 source/build/test hashes stable
across the final gate and scoped sanitizer runs. SwiftPM runtime builds (2.55 seconds),
retaining the native-builder deprecation. clang-format 18/22, strict manifest lint
and diff checks pass. Fresh selected codecs, bridges, admission, producer and job
code pass ASan/UBSan with leak detection disabled. Combined request/policy/image
preparation coverage is 150/150 lines, 121/121 regions, 14/14 functions and 90/90
branch outcomes. Baked C getters reach 41/41 lines, 36/36 regions, 5/5 functions and
25/26 outcomes. Positive native producer coverage is 40/55 lines (72.73%), 18/28
regions, 1/2 functions and 8/14 outcomes. Native failure/exception branches remain
unforced and below infrastructure adoption targets. No whole-engine/leak/RSS claim
follows, and no quarantined probe is used to improve coverage.

Actual ImageIO inside the Apple worker, full source-baked/mip transport, real
persistent session/cache/native metrics, Swift orchestration, supported-range
parity and full M4–M8 acceptance remain open. Production pin, default editor backend,
feature quarantines, protected QA state and global SVG are unchanged. There is no
distribution ABI, process containment or full native-editor parity claim.
