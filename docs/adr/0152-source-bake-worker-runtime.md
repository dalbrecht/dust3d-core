# ADR 0152: Source-baked worker rendering

Status: Implementation in validation; Apple adoption pending.

## Context

Issue #249 connects the coherent source-baked codec to worker production. Imported
rendering and legacy baking remain distinct operations. The source baker needs
procedural PNG semantics and imported PNG/JPEG semantics, exact material identities,
packed surface policy, and actual chart-isolated mip levels from one native owner.

## Decision

Opcode 8 is `generateSourceBaked`; prior opcodes retain their values. Its version-1
request is a 96-byte big-endian source prefix followed by the complete legacy
request envelope and opaque archive. The source prefix contains version u16 at 0,
prefix size u16 at 2, zero reserved bytes at 4–15, and ten u64 fields at 16–88:
material native bytes, sampler native bytes, atlas native bytes, output native bytes,
chart count, raster samples, mip-enabled flag, maximum levels, copied output bytes,
and output wire bytes. Both prefixes count toward input wire admission.

Positive byte policies only tighten receiver quotas. Zero selects maintained
defaults. Explicit dimensions, grid, chart/raster work and mip-level settings are
validated independently against host caps. Current framing and C semantic limits
still do not establish Apple #280 supported-range parity.

The producer resolves procedural references and complete imported material facts
from the same immutable archive. It releases the material native owner before
platform callbacks. All required declared imported images remain in preparation;
this path does not compact to the imported-render surface catalog. Combined encoded
budgets, required provider availability and the native model-image descriptor cap
preflight before either decoder. Procedural decoding precedes imported decoding,
sharing one remaining decoded pixel quota. Receipts require tight straight RGBA8
and native-compatible dimensions. Decoder roles stay distinct; portable code does
not perform platform decoding.

Owned pixels and identities back borrowed native views for the synchronous bake.
The producer copies geometry, packed UV/policy/tangents, exact catalogs, maps and
actual mip levels/sampling cap from one native source-baked owner before release.
Component kinds describe membership and do not establish Boolean provenance.
Base-only and opt-in mip modes remain explicit.

Child, service and queue admission use a distinct source-baked variant and
independent aggregate/inner policies. C decoded kind 9 exposes copied surface
records and owner-scoped borrowed catalog/texture spans. Swift must copy all spans
before destroying the decoded owner. Storage counts are not native cache metrics.

Failure schema v3 adds a separate resource role and exact UTF8 identity without
rewriting opaque original cause bytes. Existing failures without context retain
v1/v2 encoding. Context counts toward wire quota; any necessary cause truncation is
explicit. Original diagnostic domain/code/status remain separate from outcome.

## Verification and limitations

The final complete ordinary build and 191 CTests passed (44.91 seconds), followed
by the SwiftPM runtime product (1.54 seconds). The known SwiftPM native-builder
deprecation warning remains. Exact clang-format 18 checks and strict C11 public API
compilation passed. A manifest of 41 changed source/build files remained unchanged
through final gates. Positive native fixtures use existing Capsule/procedural
archives at 8×8. Fake providers do not prove ImageIO fidelity or mixed imported
visual acceptance. Synthetic service observations do not prove authentication.

Eight scoped ASan/UBSan programs passed after refreshing the changed adapter.
58 adapter/protocol/job/runtime objects are instrumented; linked native engine
libraries are not. Leak detection is disabled. This does not establish whole-engine
sanitizer, leak, RSS, time or cancellation-latency guarantees.

Matching per-binary coverage is preparation 104/104 lines, 105/105 regions, 5/5
functions and 80/80 branch outcomes; request 70/70 lines and 35/38 outcomes; native
producer 83/103 lines and 24/32 outcomes. Other scoped observations are operation
39/96 lines, failure codec 115/127 and baked C accessors 98/120. These are not
whole-suite coverage or compliance with core/application targets. Residual native
error/exception paths remain unforced. An initial aggregate report with a helper
counter mismatch was discarded in favor of warning-free matching binary profiles.
No native fault probes were added to increase coverage.

Production pin 48b0ce2d remains unchanged. GLB, writer/fault, animation and rig-pose
quarantines remain. No new malformed-native-input, native fault, pressure or crash
probes are introduced. Actual Apple decoding/orchestration, persistent sessions,
cache/native metrics, range reconciliation and final visual acceptance remain open.
Completed automated review and all CI remain required before merging.
