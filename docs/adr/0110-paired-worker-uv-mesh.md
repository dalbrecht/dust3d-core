# ADR 0110: Atomically admit paired geometry and UV metadata

Status: Local implementation validated; serial public review pending.
Tracking: [core #163](https://github.com/dalbrecht/dust3d-core/issues/163).

## Contract

Add a standalone response value containing the existing base mesh and UV schemas.
Version one has a 24-byte prefix: uint16 version and prefix length, uint32 zero
flags, then uint64 geometry and UV byte lengths, all big-endian. Exact complete
geometry bytes precede exact complete UV bytes. No native struct layout or pointer
crosses the boundary. The active frame opcode/helper dispatch remains unchanged.

Separate inner policies retain their original typed causes. Aggregate wire and
logical-output budgets are independently bounded. Validate positive supported
profiles before encoding. Complete fixed/subspan checks reject overflow, truncated
or trailing data before inspection. Geometry's nonzero complete triangle count
must match UV metadata exactly. Equal counts do not authenticate source provenance;
a later operation must construct both values from the same native owner and use
parent frame/document/request/revision/process admission before publication.

## Validate before retained allocation

Expose inspectMeshPayload and inspectUvPayload, sharing each decoder's original
semantic validation, to return validated counts/logical charges without returned
arrays. Base geometry now validates every triangle before reserving decoded
vertices. UV inspection preserves its full span/field/key/image/gradient checks
and bounded duplicate-key/chart-string validation workspace. These inspections
must read stable input throughout the call; their returned descriptions are not
leases or authentication receipts for subsequently modified buffers.

Paired decoding validates both full subspans and their aggregate logical charge
before constructing either returned array. A later UV rejection never returns a
geometry-only result. Original MeshPayloadError/UvPayloadError types propagate;
framing/aggregate policy failures use UvMeshPayloadError. Allocation errors also
propagate with ordinary RAII cleanup and no partial return. Final decoding reuses
the independent inner decoders after aggregate admission, so validation is repeated
rather than exposed as an unsafe unchecked public decoder.

Encoding validates/encodes each component into temporary owned spans, inspects
the pair and aggregate charge, then allocates the returned envelope. Intermediate
encoded spans are workspace, not a promise that peak memory equals the aggregate
output quota. Logical decoded charge counts records/text, excluding roots,
capacity, allocator overhead and validation workspace. No RSS/process-cap claim.

## Evidence

The first paired contract failed on the missing API. Independent prefix fixtures
assert both lengths and layout. Small value-only contracts cover exact and just-
under aggregate wire/logical budgets, nested policies, version/flags/span failures,
triangle mismatch, original cause preservation and independent returned ownership.
Added later-record cases reject a degenerate second geometry triangle and invalid
second UV record, confirming inspection does not stop after the first record.
No malformed values reach an engine or a worker process.

Fresh source quarantine guard/audit records 115 ordinary contracts, adding only the
paired value test. Fresh full CMake build and 115/115 CTest contracts pass 20.90
seconds, retaining legacy worker and licensed Capsule UV-owner contracts. Fresh
scoped ASan/UBSan passes both paired-value and the existing full UV-value harnesses,
including the refactored inspection paths. Scoped WorkerUvMesh.cc coverage measures
64/64 lines, 45/45 regions, 7/7 functions and 36/36 branch outcomes. It does not claim
whole mesh/UV/engine coverage. The unchanged SwiftPM product compatibility build
passes incrementally in 0.09 seconds. Changed C++ formatting/diff checks pass.
Header API documentation only changes after the gate, with comment-stripped code
equality proof; other gated runtime/test/manifest hashes remain unchanged.

## Remaining gates

No new worker opcode, UV request options, native paired producer, pixel transfer,
parent dispatcher or renderer integration is enabled. Those remain reviewed stages
before actual source-aware worker adoption. Strong memory/sandbox/descendant/
parent-death containment and trusted packaging remain open. Keep core #67/#69 GLB,
#83 writer, #84 faults, #95 animation and Apple #199 rig-pose quarantines and the Apple
production pin. No crash, signal-delivery, broken-pipe, allocation-failure or
resource-exhaustion probe or protected app control occurs. M4/M8/full parity remain
incomplete.
