# ADR 0143: Preserve the legacy baked rendering path for workers

Status: Accepted implementation, pending reviewed delivery.

## Context

Apple's procedural `MeshWithBakedTextures` renderer consumes expanded geometry,
UV charts/gradients and image/component associations, per-triangle tangent frames
and optional base maps. Source-baked policies/catalogs are a different contract.
Substituting a source bake or transferring only pixels would drop legacy metadata.
Apple #279 requires all supported material paths before worker activation; #280
separately requires preserving the editor's supported payload range.

## Decision

Core #231 adds `LegacyBakedPayload` with geometry, full existing `UvPayload`,
tangents and five-role base texture values. Geometry/UV/texture schemas stay
unchanged. Tangents retain original direction/handedness/mapping and correlate
with each validated UV triangle. Unknown tangent directions must be finite but
need not be unit length; known tangents require known UVs and squared unit length
within 0.0001. Handedness is ±1, and mapping flags are 0 or 1. Negative-zero bits,
keys, triangle/chart/gradient ordering, warning text and vertex attributes remain
unchanged. The shared scalar validator is also used by source-baked metadata.

The outer explicit big-endian version-one prefix is 48 bytes:

| Offset | Field |
| --- | --- |
| 0 | UInt16 version 1 |
| 2 | UInt16 prefix size 48 |
| 4 | UInt32 reserved zero |
| 8 | UInt64 geometry byte length |
| 16 | UInt64 UV metadata byte length |
| 24 | UInt64 tangent/triangle count |
| 32 | UInt64 texture byte length |
| 40 | UInt32 tangent stride 24 |
| 44 | UInt32 reserved zero |

The exact geometry, UV, tangent and texture spans follow in that order. Each
24-byte tangent record contains three binary32 direction components, binary32
handedness, UInt8 known flag and seven reserved zero bytes. No native structs or
containers are serialized. All inner spans and records, count/UV correspondence,
base-only texture policy and aggregate quotas validate before decoded arrays.
The UV accessor reads only fixed triangle spans within an already inspected table.
Existing inner error families propagate without generic replacement.

This path supports one base level per present map, matching the current legacy
SDK. Higher levels reject even if a texture packet could describe them. The shared
texture validator establishes a zero sampling cap for the mandatory one-level
color map; it is not inferred from dimensions. Source-baked mip output retains its
separate contract and remains supported. No hidden filtering or representation
substitution is introduced.

Native copying reads one immutable borrowed `Dust3DBakedMesh` owner. Its UV child,
geometry, tangents and map views are preflighted together. Shared native UV
inspection now validates all triangle/chart/gradient/key/atlas semantics before
retained copying, using existing pure validators and bounded key/string workspace.
Native UV conversion is shared between inspection and copying. A new common
expanded-geometry inspector validates counts, text, finite attributes and triangle
area before allocations, and is reused by ordinary, source-baked and legacy
copying. Source-baked count/quota error families stay intact; mapped quota errors
retain the original shared cause. Other original mesh errors propagate.

The legacy bake payload accounts tangents/maps separately from its owned UV mesh.
Its minimum native payload check therefore charges tangent records and pixels,
while geometry/UV have their own preflight policies. Whole-copy logical/wire
quotas include every family before retained output. A copied result is independent
after owner destruction. Borrowed owners/spans must remain stable throughout.

Logical charge includes inner records/text/pixels and `sizeof(TextureTangent)`
per triangle. Root objects, capacity, encoded wire temporaries, bounded validation
sets/strings, native workspace, CPU and RSS are excluded. Default aggregate
logical quota stays 128 MiB; prototype limits do not establish Apple #280 range
parity. No ceiling, public C layout or existing opcode is changed. Internal C++
clients rebuild against the matching source revision; this is not a stable ABI.

No request/C decoded alternative, Swift consumer, platform image decoder, Apple
pin/backend activation or parent-engine fallback is added. Imported-only authored
samplers/factors/images, real session/native metrics, supported range, complete
transport and editor acceptance remain required. M4–M8 completion stays unproven.

## Validation and limits

- Separate TDD reds identify the missing codec and native-copy headers. Pure
  contracts cover full charts/gradients/keys, tangent bits and UV correspondence,
  optional maps, independent ownership, exact/short/tiny quotas, typed inner
  failures, flags/reserved bytes, truncation/trailing spans and rejection of
  otherwise valid mip packets on the base-only legacy path.
- One fresh licensed Capsule fixture uses the same ordinary eight-pixel settings
  as the existing bake contract. The test compares retained geometry attributes,
  original counts/warning, UV/tangent bits, chart/gradient fields when present,
  exact component/image keys and all map bytes, then destroys the owner and
  round-trips the copied value. Copy-only quota checks reuse that valid owner;
  null-owner rejection occurs before native accessors. This does not validate a
  new decoder or malformed native input.
- Five focused legacy/source-baked/UV regressions pass. Fresh full CMake build and
  final **157/157 ordinary CTests pass in 35.30 seconds**, with all 19 changed
  source/build-spec hashes stable. Source quarantine, formatting and whitespace
  checks pass. SwiftPM runtime packaging passes in 2.24 seconds; the existing
  native-builder deprecation is unchanged.
- The initial full run passed 155/157: two newly owned XPC fixtures failed signing
  before launch because FinderInfo was attached to bundle directories under
  cloud-managed Documents. No source change or signing-policy relaxation repaired
  that run. The entire single build folder was moved to the user-owned cache
  outside Documents, retaining artifacts/caches and stable aliases. All four
  healthy XPC fixtures then passed normal signing/sandbox/peer checks, followed
  by the complete successful gate. No xattrs, quarantine flags, entitlements or
  trust settings were removed or weakened. Failed fixture artifacts remain.
- Refreshed scoped ASan/UBSan passes pure legacy cases, healthy legacy copying and
  the existing healthy source-baked copying regression after shared preflight
  extraction. Leak detection is disabled; linked inherited engine/protocol archive
  code is not fully instrumented. No leak or whole-engine claim.
- Named pure coverage: `WorkerLegacyBaked.cc` **151/151 lines, 96/96 regions,
  17/17 functions, 70/70 branch outcomes**; shared tangent validator **13/13 lines
  and 18/18 outcomes**. Focused native boundary reports: legacy copy **67/74 lines
  (90.54%), 62/69 regions, 5/5 functions, 40/56 outcomes**; common geometry inspector
  **30/36 lines (83.33%), 32/38 regions, 3/3 functions, 16/28 outcomes**. The focused
  whole `WorkerUvOperation.cc` report is **105/209 lines, 70/144 regions, 9/15
  functions and 39/100 outcomes**; that file includes existing generation/dispatch
  paths not invoked by this copy fixture. Defensive guards and the native gradient
  converter remain unprobed here. These partial native reports remain below the
  adoption target; no artificial branches or unsafe probes are added to raise them.
- One physical shared build root is `~/.cache/dust3d-native-build`. The ignored
  `.workspaces/build` and previous temporary build path alias that same folder.
  Source checkouts and durable recovery bundles remain preserved. No UI change.

Core #67/#69 GLB, #83 writer, #84 fault, #95 animation and Apple #199 rig-pose
quarantines remain. No new malformed-native-input, crash, allocation-failure,
signal, broken-pipe, native cancellation or pressure/exhaustion probes, stale
binaries, app control, global SVG, protected-QA or OS-container changes. Completed
clean exact-head automated review and all CI green remain required before merge.
