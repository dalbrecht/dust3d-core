# ADR 0134: Owned worker source-hint values

Issue: #213, for Apple output-parity tracking dalbrecht/dust3d-swiftui#279.
Status: full local gates passed; public review pending.

## Decision

Add a standalone portable source-hint value codec to the maintained worker protocol
library and SwiftPM runtime product. It does not change the request/reply operation
set, C ABI, generator or Apple pin. Pairing with geometry, generation adapters,
decoded C owners and Swift adoption remain subsequent reviewed stages. No new
dependency, Qt or Apple type is introduced.

Preserve exact UTF-8 node/part text, row order and aliases; do not normalize UUIDs
or Unicode. The 1024-byte per-field policy is broader than current native 36/38-byte
keys and retains their valid spellings. Empty/invalid UTF-8 and NUL reject; existing
C-string source access cannot represent embedded NUL. Synthetic codec fixtures do
not establish native acceptance of arbitrary non-UUID identifiers.

Source weights are finite and nonnegative, including preserved negative-zero bits.
They may exceed one and are not normalized. These are inherited provenance hints,
not normalized skeletal weights. Known kind/range rules match MeshSourceHints:
node/ambiguous corners reference one matching influence; unknown corners have none
or one unknown influence; weighted corners are nonempty. Weighted kind is invalid
on influences. Node influences need a valid identity; unknown/ambiguous use the
UINT64_MAX absent-identity sentinel. Other numeric kinds remain opaque with bounded
ranges/references, matching the existing Swift representation.

## Version-one source payload

All integer fields and IEEE-754 binary32 weight bits are big endian through the
existing WorkerWire helpers. No C/C++ object layout is serialized.

| Part | Fields |
| --- | --- |
| 40-byte prefix | u32 version=1; u32 reserved=0; u64 identity count, corner count, influence count, identity-text bytes |
| Each identity | u32 node length; u32 part length; exact node bytes; exact part bytes |
| Each 24-byte corner | u32 kind; u32 reserved=0; u64 first influence; u64 influence count |
| Each 24-byte influence | u32 kind; u32 reserved=0; u64 identity index; binary32 weight; u32 reserved=0 |

The corner count is nonzero and divisible by three. Unknown-only output can have
zero identities/influences. Future geometry pairing must additionally prove the
corner count matches that geometry; this codec alone makes no pairing claim.

Default logical decoded quota is 64 MiB; wire ceiling is 256 MiB. Default count
caps are one million identities, three million corners, eight million influences
and eight MiB text. Supported policies remain bounded by the global 256 MiB
ceiling and record sizes; individual text fields are at most 1024 bytes. Checked
subtraction precedes multiplication/addition and address-space conversion.

Decoded logical charge includes record objects, text and two terminators per
identity. Root objects, capacities, validation workspace and RSS are excluded.
The decoder validates complete framing, reserved fields, UTF-8, all influence
references/weights and corner rules before constructing returned strings/vectors.
Typed SourceHintPayloadError retains category and original cause. C++ allocation
exceptions remain the future C adapter's containment responsibility.

## Evidence and limits

The initial test failed to compile for the missing header. Fresh focused contracts
then passed for owned round trips, byte-distinct Unicode aliases, weights above
one/negative zero, unknown/ambiguous/future kinds, exact quotas, UTF-8 and bounded
layout/range rejection. These tests invoke codecs only, not engine input parsing.

Source quarantine checks pass. Fresh full CMake build and all 145 ordinary CTests
pass (final 33.78 seconds); source hashes are unchanged across the final gate.
SwiftPM Dust3DWorkerRuntime packaging also passes. New C++ formatting, strict
Swift manifest formatting and whitespace checks pass. The existing SwiftPM native
builder deprecation warning remains.

Fresh scoped ASan/UBSan passes for this codec and its pure tests; leak detection
is disabled on the supported host, so no leak-check claim follows. Scoped coverage
is 244/245 lines, 203/204 regions, 16/16 functions and 181/186 branch outcomes.
The address-space rejection is unexercised on this 64-bit host under the much
smaller supported quotas; no artificial allocation/failure probe was added.
This is not full-engine coverage, output parity or containment evidence.

Preserve #67/#69 GLB, #83 writer, #84 fault, #95 animation and Apple #199 quarantines.
No new native malformed-engine, crash, allocation-failure, signal, broken-pipe,
cancellation or pressure probe was introduced. Completed exact-head automated
review and every CI check remain required before merge. Material/mip output,
transport trust, helper adoption and complete editor acceptance remain open.
