# ADR 0142: Preserve coherent source-baked worker rendering output

Status: Accepted implementation, pending reviewed delivery.

## Context

Core #227 transfers texture bytes but cannot bind them to a source bake's final
geometry, packed mappings, policies or catalogs. Source baking may retain a
triangle order different from a separate ordinary/indexed generation. Counts alone
do not establish that those independently generated values correspond. Apple #279
requires full output parity before editor worker activation; Apple #280 separately
requires preserving the editor's supported payload range.

## Decision

Core #229 adds `SourceBakedPayload`: expanded geometry, final per-triangle surface
metadata, exact model/component catalogs and five-role baked textures/mips. The
native copy function accepts one live immutable borrowed source-baked owner and
copies all families from that owner. It does not attach foreign source hints or
infer provenance from matching counts. Geometry, vertex attributes, warnings,
counts, keys, mapping/tangent bits, policies and pixels preserve their order and
representation. No native pointers escape.

The complete tuple uses a 32-byte explicit big-endian prefix: UInt16 version 1,
UInt16 prefix size 32, UInt32 reserved zero, then UInt64 byte lengths for geometry,
surface metadata and textures. Those exact spans follow in that order, without
padding. Geometry and textures retain their existing inner schemas.

Surface metadata uses a 40-byte prefix: UInt16 version 1, UInt16 prefix size 40,
UInt32 reserved zero and four UInt64 counts: triangles, models, components and
aggregate key bytes. It is followed by one 96-byte record per expanded triangle:

| Offset | Field |
| --- | --- |
| 0 | Six binary64 packed UV coordinates, corner/axis order |
| 48 | Three binary32 tangent direction components |
| 60 | Binary32 tangent handedness |
| 64 | Binary64 alpha cutoff |
| 72 | UInt64 model index; UINT64_MAX means unknown |
| 80 | UInt64 component index; UINT64_MAX means unknown |
| 88–91 | UInt8 UV-known, policy-known, double-sided, tangent-known flags |
| 92 | UInt32 alpha mode |

Then each model key has UInt32 byte length plus UTF-8 bytes. Each component has
UInt32 membership kind, UInt32 key byte length and UTF-8 bytes. No native structs
or container layouts are serialized. Models and components have independent key
namespaces. Keys are nonempty, at most 38 bytes, valid UTF-8 without NUL, and unique
by original bytes within each catalog. Unicode-equivalent spellings remain distinct.
The existing source-hint UTF-8 implementation is shared in `WorkerUtf8.h`; source
hint error codes and accepted character rules stay intact, with one consolidated
invalid-UTF-8 diagnostic. No inherited identity normalization is introduced.

Known flags must be 0 or 1; component membership is original 1 or generated 2.
Packed UVs are finite and within 0–1; unknown UVs are zero. Tangent components are
finite, handedness is ±1, and a known tangent requires known UVs and squared unit
length within 0.0001. Known policy preserves alpha mode 0/1/2, finite nonnegative
cutoff (including values above one) and sidedness. Unknown policy retains its raw
alpha fields as opaque data, matching the existing Swift boundary's optional
policy semantics. Unknown catalog references use the wire-sized sentinel; known
references are independently bounded. Valid negative-zero scalar bits are retained.

Whole-packet inspection validates every inner payload, exact framing, metadata,
geometry/triangle correspondence and aggregate quotas before decoded output arrays.
Duplicate-key sets are bounded validation workspace, not retained output. Native
copying additionally preflights native payload/count consistency, all geometry and
triangle semantics, stable catalog spans, tight borrowed image views and aggregate
wire/logical charges before retained copying. Fixed borrowed texture slots share
the existing owned/wire texture validation and copy plan; a separate set of native
mip rules is not maintained. The native owner must remain stable throughout.

Logical charges include inner vertex/triangle/level records, catalog string
objects, exact key bytes plus terminators, warning bytes and pixels. Root objects,
capacity, encoded inner/outer wire temporaries, validation sets, native workspace,
CPU and RSS are excluded. The default aggregate decoded quota remains 128 MiB;
prototype limits do not establish Apple #280 parity. No global ceiling is widened.
This is an internal source-matched C++ value contract, not a stable binary ABI.

No new generation/request opcode, C decoded alternative, Swift consumer, platform
image decoder, Apple pin or backend change is included. Synthetic decoded pixels
in the healthy fixture isolate the existing native image boundary and do not prove
PNG decoding. Imported-only authored samplers/factors/images and legacy procedural
chart metadata remain distinct paths. Real session/cache statistics, native payload
metrics, supported-range preservation, authenticated transport and editor acceptance
remain required before adoption. Full M4–M8 completion is unproven.

## Validation and limits

- Separate TDD reds identify the missing pure and native-copy headers. Pure tests
  cover coherent round trips, independent lifetime, every truncation of a small
  packet, inner layout/key spans, typed errors, exact/short quotas, known/unknown
  fields, invalid references, alpha/tangent/UV boundaries and exact Unicode keys.
  UTF-8 tests cover scalar widths, invalid continuations, overlong encodings,
  surrogate scalars, truncation and out-of-range code points. Borrowed texture tests
  exercise exact measures, independent pixel copies, absent color, missing pixels
  and excessive level counts. These cases do not execute the native engine.
- One fresh known-positive licensed ImportedSurface fixture uses the same ordinary
  1×1 decoded input and 512-pixel/two-level settings as the existing mip contract.
  It compares every geometry attribute, warning/count, catalog, triangle mapping,
  policy, tangent and map/mip byte, then releases native owners and round-trips
  the owned wire value. Copy-only quota cases reuse that valid owner without
  regeneration; null-owner rejection occurs before native accessors.
- Fresh full CMake build and **155/155 ordinary CTests pass in 49.02 seconds**,
  including existing healthy XPC fixtures in their authorized execution context.
  Changed-source hashes stay stable. Source quarantine, formatting and whitespace
  checks pass. SwiftPM runtime packaging passes in 27.91 seconds; the existing
  native-builder deprecation remains.
- Refreshed scoped ASan/UBSan passes both pure families and the healthy native-copy
  fixture, leak detection disabled. Selected adapter/codec sources are instrumented;
  linked inherited engine libraries are not. No leak or whole-engine claim.
- Named coverage reports: `WorkerSourceBaked.cc` **300/300 lines, 222/222 regions,
  29/29 functions and 190/190 branch outcomes**; `WorkerUtf8.h` **37/37 lines and
  32/32 outcomes**; `WorkerBakedTextures.cc` **190/190 lines, 137/137 regions,
  17/17 functions and 114/114 outcomes**. Native-copy boundary coverage is
  **134/146 lines (91.78%), 116/130 regions (89.23%), 8/8 functions and 69/98
  outcomes (70.41%)**. Defensive owner/storage guards remain unprobed and below
  the adapter adoption target. No malformed-native-input/fault probes or artificial
  branches are introduced to raise that figure. Reports use matching executables
  and profiles; unrelated partial runtime/engine coverage is not total coverage.
- Temporary source and build directories disappeared during interruption. Current
  source was recovered from durable checkpoints and this task's own write-command
  artifacts, then rebuilt and validated afresh. Durable ignored `.workspaces/core`
  and `.workspaces/build` now hold the single checkout and shared build root; former
  temporary paths are aliases, not duplicate builds. No UI change or screenshot.

Core #67/#69 GLB, #83 writer, #84 fault, #95 animation and Apple #199 rig-pose
quarantines remain. No new malformed-native-input, crash, allocation-failure,
signal, broken-pipe, native cancellation or pressure/exhaustion probes, stale
binaries, app control, global SVG, protected-QA or OS-container changes. Completed
clean exact-head automated review and all CI green remain required before merge.
