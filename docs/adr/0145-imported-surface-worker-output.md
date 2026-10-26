# ADR 0145: Authored imported surface worker output

Status: Accepted for internal values; transport and editor adoption remain pending.

## Context

Issue #235 follows material facts (#233). The authored imported rendering path also
needs matching geometry, original UVs and vertex colors. Packed source-baked UV
rules cannot represent repeating authored coordinates, and mesh preview colors
cannot substitute for the original per-corner binary64 color channel.

## Decision

`ImportedSurfacePayload` owns expanded geometry, aligned source records and exact
model/component catalogs. Each triangle preserves six binary64 UV coordinates,
twelve straight RGBA channels, RGB numerical encoding (sRGB 0 or linear 1), UV
presence and optional model/component references. Known UVs are finite and require
a model; coordinates may be outside 0–1. Unknown UVs are zero. Colors are finite
0–1, alpha remains linear and unassociated, and no transfer conversion occurs.
UINT64_MAX means unknown identity. Catalogs are independently unique nonempty
non-NUL UTF-8 keys of at most 38 bytes, without normalization. Component membership
is original 1 or generated 2; membership does not establish Boolean provenance.

The explicit big-endian outer prefix is 24 bytes: version 1 and prefix size as
u16 fields, reserved u32 zero, then u64 geometry length at 8 and metadata length
at 16. Geometry retains its existing schema. Metadata prefix is 40 bytes:
u16 version 1, u16 size 40, reserved u32 zero; u64 triangle/model/component/text
counts at 8/16/24/32. Each 168-byte triangle contains six IEEE binary64 UVs at 0,
twelve binary64 RGBA channels at 48, u64 model/component references at 144/152,
u8 UV presence at 160, zero reserved bytes 161–163, and u32 encoding at 164.
Model entries are u32 byte length plus key bytes. Component entries add u32 kind
before the same key representation. Order and accepted scalar bits survive.

Complete framing, correspondence, scalar, catalog and aggregate inspection precedes
decoded output arrays. Validation sets are bounded workspace. Native copying uses
one immutable `Dust3DImportedSurfaceMesh` owner, preflights geometry via the shared
mesh inspector and metadata via the same pure rules, then copies independently.
No pointers escape and no native generation occurs inside the copy function.
The caller must retain a stable owner for the entire call. Original mesh failures
remain typed causes. Encoded temporary buffers and native generation workspace are
not retained-output quota charges.

Independent metadata limits default to 128 MiB logical bytes, 256 MiB wire,
one million triangles, 100,000 models/components and 8 MiB text. Aggregate logical
limit defaults to 128 MiB. Charges include logical geometry/records/catalog entries,
identity bytes and terminators; exclude roots, capacities, allocator overhead,
validation workspace and process RSS. These are prototype policies, not proof of
Apple supported-range parity (#280).

## Validation and limits

TDD began with missing-header failures for pure and native-copy APIs. Tests cover
round trips and lifetime, authored UVs outside 0–1, negative zero, encoding, exact
Unicode aliases, optional references, catalog consistency, malformed pure framing
and independent quotas. Native tests use existing positive ImportedSurface and
Capsule fixtures, compare every authored record/catalog, release the owner and
round-trip the result. Copy-only quotas reuse each healthy owner. Initial native
test setup omitted required options; the test was corrected before successful runs.

All 161 ordinary CTests pass (37.60 seconds), with nine source/build-spec hashes
unchanged across the final build/gate. SwiftPM runtime builds (0.74 seconds;
existing native-builder deprecation remains). ASan/UBSan pass on bounded pure
cases and selected new native-copy code with existing positive fixtures. Leak
detection is disabled; linked inherited engine code is not fully instrumented.
Pure codec: 277/277 lines, 208/208 regions, 28/28 functions and 176/176 branch
outcomes. Native copy: 82/86 lines, 60/64 regions, 5/5 functions and 38/44 branch
outcomes. Defensive missing native storage/identity/payload cases remain unprobed;
this does not establish full engine coverage or safety acceptance.

This stage adds no opcode, C decoded kind, Swift consumer, backend activation or
production pin change. Combining surface and material owners needs explicit
same-archive provenance; matching counts alone is insufficient. Platform PNG/JPEG
decoding must remain inside the eventual Apple worker, outside portable C++.
Real session metrics, transport/Swift ownership, supported-range parity, all
quarantined features and full M4–M8 acceptance remain open.
