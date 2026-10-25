# ADR 0135: Atomic worker geometry and source hints

Issue: #215, following #213/#214 and Apple output-parity tracking #279.
Status: full local gates passed; public review pending.

## Decision

Pair the existing expanded MeshPayload and SourceHintPayload as one owned value.
This is a standalone portable codec, not a new request/reply operation or C ABI.
Generator routing, indexed/material/mip parity, owned C/Swift output and default
editor backend activation remain separate reviewed work. The Apple pin is unchanged.

Add source inspection that shares complete validation with source decode and
returns only counts/logical charge. Append validated nodeCount to the existing
C++ mesh inspection summary. Source clients rebuild with the changed C++ summary;
the public C layout/version does not change and no binary C++ ABI claim follows.
Pairing reads validated summaries rather than duplicating inner wire field parsing.

Require one source corner per expanded geometry vertex and one source identity
per geometry source-node count. Inner source kind/range/weight/identity rules remain
unchanged, including byte-distinct aliases, opaque future kinds and weights above
one. Count correspondence does not authenticate the producer or prove algorithmic
provenance. Trusted request/source/connection admission is still required.

## Format and admission

Use the existing big-endian wire helpers. The 24-byte outer prefix contains u16
version=1, u16 prefix size=24, u32 reserved=0, u64 mesh bytes and u64 source bytes.
Two complete inner payloads follow in that order. Missing prefixes, unsupported
versions/flags, truncated/trailing spans and mismatched counts reject atomically.

Inner quotas remain independent. Aggregate wire quota is at most 256 MiB; logical
decoded quota defaults to 128 MiB and has the same supported ceiling. Checked
subtraction precedes additions/conversions. Decode validates both complete inner
spans, aggregate charge and correspondence before either owned decoder constructs
output. A source rejection cannot return partial geometry. Inner typed errors and
original diagnostics propagate unchanged.

Encoding validates the fixed minimum before temporary geometry encoding, then
measures validated source values and aggregate quotas/counts before source/final
encoding. Temporary encoding segments, root objects, capacities, validation work
and allocator overhead are excluded from logical decoded charge. This is not a
process-RSS or CPU bound. Valid inspection does not build decoded arrays/text;
ordinary typed exception diagnostics may still allocate on rejection.

## Evidence and limits

After CMake regeneration, the initial test failed for missing WorkerSourcedMesh.h.
Fresh focused tests pass for owned round trips, exact aggregate quotas, both
correspondence mismatches, invalid framing, quota boundaries and original mesh/
source errors. Source regression tests also exercise shared inspection counts,
charges and rejection causes. No new test parses an archive or runs generation.

Fresh full build and all 146 ordinary CTests pass (37.15 seconds). Changed-source
hashes remain unchanged across that gate. SwiftPM runtime packaging, C++ formatting,
strict Swift manifest formatting, whitespace and source quarantine checks pass.
The existing SwiftPM native-builder deprecation warning remains.

Scoped fresh ASan/UBSan passes for both pairing and source regression contracts;
leak detection is disabled on the supported host, so no leak-check claim follows.
Pairing coverage is 75/75 lines, 50/50 regions, 8/8 functions and 40/40 branch
outcomes. Refactored source coverage is 254/255 lines, 205/206 regions, 18/18
functions and 181/186 branch outcomes. Its existing address-space rejection remains
unexercised on this 64-bit host. These are scoped codec metrics, not full-engine,
native end-to-end source pairing, output-parity or containment evidence.

Preserve #67/#69 GLB, #83 writer, #84 fault, #95 animation and Apple #199 quarantines.
No new malformed-engine, crash, allocation-failure, signal, broken-pipe, native
cancellation or resource-pressure probe was added. Completed exact-head automated
review and every CI check remain required before merge.
