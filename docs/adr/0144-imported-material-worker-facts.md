# ADR 0144: Preserve authored imported material facts for workers

Status: Accepted implementation, pending reviewed delivery.

## Context

Apple's imported renderer retains authored material factors, sampler absence and
filter presence, original encoded image declarations/bytes, and part/component
bindings. Baking these values into pixels would lose intent and change the
existing imported-only path. Core #233 supplies an owned portable fact contract;
full imported surface geometry and decoded pixels remain a separate integration
step under Apple #279. Supported-range parity remains gated by Apple #280.

## Decision

Add `ImportedMaterialsPayload` with ordered material definitions and bindings.
Preserve exact opaque UTF-8 identities, binary64 PBR factors/cutoff, alpha mode and
sidedness, optional sampler/filter presence, original GL constants, declared
PNG/JPEG encoding and exact encoded bytes. A binding stores a material index,
part identity and optional component identity. Missing component means an
uninstanced imported part; it is not an empty or synthesized identifier.

The explicit big-endian version-one prefix is 40 bytes: UInt16 version 1, UInt16
prefix size 40, UInt16 material stride 128, UInt16 binding stride 24, then UInt64
material count, binding count, aggregate key bytes and aggregate encoded bytes.
Each variable material starts with a 128-byte fixed record, followed by its exact
model key and encoded image bytes:

| Offset | Field |
| --- | --- |
| 0 | Four binary64 base RGBA factors |
| 32 | Binary64 metallic |
| 40 | Binary64 roughness |
| 48 | Three binary64 emissive factors |
| 72 | Binary64 alpha cutoff |
| 80 | UInt32 alpha mode |
| 84, 88 | UInt32 magnification/minification values |
| 92, 96 | UInt32 wrap S/T |
| 100 | UInt32 model-key byte length |
| 104 | UInt64 encoded-image byte length |
| 112 | Presence bits: sampler 1, magnification 2, minification 4, image 8, double-sided 16 |
| 113 | Encoding: absent 0, PNG 1, JPEG 2 |
| 114–127 | Reserved zero bytes |

After all materials, each binding has UInt64 material index, UInt32 part length,
UInt32 component length, UInt8 component-present flag and seven reserved zero
bytes, followed by part and optional component key bytes. No native structs or
container layouts are serialized. Valid negative-zero and original double bits
are retained. Identity equality uses bytes, not Unicode or UUID normalization.

Base/emissive colors, metallic and roughness are finite within 0–1. Alpha mode is
0/1/2, cutoff is finite and nonnegative (values above one remain valid), and
sidedness is 0/1. Sampler flags are independently validated before conversion:
absent sampler fields are zero; present wrapping is 10497/33071/33648; explicit
magnification is 9728/9729; explicit minification is 9728/9729/9984–9987. Missing
filters retain zero raw values and optional absence rather than inferred defaults.
Image and sampler presence agree. Present images have a known declaration and
readable nonempty encoded storage; absent images have no declaration or storage.
Encoded bytes are opaque source data, not proof of valid or decoded pixels.

Keys are nonempty, at most 38 UTF-8 bytes, without NUL. Material keys are unique.
Every binding references an existing material. One part cannot reference different
models, component bindings are globally unique, and a part cannot mix instanced
and uninstanced bindings or have repeated uninstanced bindings. Record order does
not determine validity. Original binding/model indices are retained.

Owned, wire and native adapters share a pure internal reader/validator. Raw views
retain full-width C flags so invalid values cannot silently become optional
absence. A reader guarantees immutable, actually readable borrowed spans for the
synchronous inspection/copy call. Count/minimum-storage policy precedes borrowing
rows; complete semantics, key lookup workspace and aggregate budgets precede
retained arrays/text/image copying. Wire framing and text accounting jointly
establish the encoded byte total; no redundant unreachable totals branch is kept.
The copy function independently inspects and rewinds, accepting no trusted caller
measurement. No arbitrary pointer-validity claim is made for forged reader spans.

Native copying uses one stable imported-assets owner. It checks native count/
retained-payload consistency, bounds C-string reads and maps known MIME declarations
without decoding. Shared pure preflight validates every view before retaining
copies. Native handles and buffers can be released after return. New code remains
C++17 without Qt, Apple image APIs or new dependencies. This internal C++ contract
requires matching source clients and does not establish a stable binary ABI.

Logical charge is material/binding record objects, exact key bytes plus each
retained model/part/component terminator, and encoded bytes. Roots, capacity,
validation lookup sets, native/decoder workspace, CPU and RSS are excluded.
Defaults are 64 MiB logical/encoded totals, 16 MiB per image, 8 MiB text, 100,000
materials and one million bindings; byte policies cannot exceed the existing
256 MiB worker ceiling. No existing family ceiling is widened. Prototype bounds
do not prove supported-range equivalence to native or Swift ownership budgets.

There is no image decoding, geometry generation, imported-surface/decoded-pixel
pairing, request opcode, C decoded alternative, Swift consumer, Apple pin or
backend activation here. Real native/session metrics, complete authored render
output, payload-range preservation, transport and editor acceptance remain open.
The original document and its opaque archive assets are unchanged.

## Validation and limits

- TDD reds identify missing pure/native-copy headers. Pure tests exercise factor
  and optional-sampler semantics, every supported minification filter, absent
  versus explicit filtering, exact Unicode aliases, opaque image bytes, binding
  references/uniqueness/instancing/order, empty procedural facts, ownership,
  exact/short/tiny quotas, malformed framing/reserved fields and all truncations
  of a small valid packet. Stable scalar readers test raw flag/storage consistency
  without native handles or parsers. No fake image is claimed decoded.
- Native tests load the existing licensed ImportedSurface and Capsule archives
  through the existing material-inspection API only. They compare every factor,
  identity, sampler/filter flag/value, image declaration/byte, part/instance key
  and model reference; verify procedural empty facts; release owners and perform
  independent wire round trips. Copy-only quota checks reuse the valid owner,
  and missing-owner rejection occurs before native accessors. Neither native test
  generates meshes or decodes pixels.
- Fresh full CMake build and **159/159 ordinary CTests pass in 49.38 seconds**,
  with all nine changed source/build-spec hashes stable. Source quarantine,
  formatting and whitespace checks pass. SwiftPM runtime packaging passes in
  0.94 seconds; the existing native-builder deprecation remains.
- Refreshed scoped pure and healthy native-copy ASan/UBSan passes, leak detection
  disabled. Selected new sources are instrumented; linked inherited engine code
  is not. No leak or whole-engine coverage claim.
- Named pure codec coverage is **358/358 lines, 292/292 regions, 38/38 functions
  and 244/244 branch outcomes**. Native-copy boundary coverage is **68/76 lines
  (89.47%), 35/44 regions (79.55%), 11/11 functions and 15/26 outcomes (57.69%)**.
  Defensive native storage/text guards and the native JPEG MIME branch remain
  unprobed in these fixtures and below the adoption target. Pure sampler/image
  rules are covered independently. No unsafe/native fault probes or artificial
  branches are introduced to inflate coverage.
- All builds and caches use the one physical user-owned cache folder outside
  cloud-managed Documents, through existing aliases. Durable source/recovery
  artifacts remain preserved. No UI change or screenshot.

Core #67/#69 GLB, #83 writer, #84 fault, #95 animation and Apple #199 rig-pose
quarantines remain. No new malformed-native-input, crash, allocation-failure,
signal, broken-pipe, native cancellation or pressure/exhaustion probes, stale
binaries, app control, global SVG, protected-QA or OS-container changes. Completed
clean exact-head automated review and all CI green remain required before merge.
Full M4–M8 parity is unproven.
