# ADR 0171: Bounded preview chunk framing

Status: proposed for reviewed delivery. Tracks #270 and Apple #280.

## Decision and wire format

Introduce a separate opt-in D3PC version1 framing format; leave all legacy frame
versions/opcodes/caps unchanged. Use a fixed80-byte big-endian header and a frozen
host-selected chunk payload size1..65536, with an explicit addressable whole-transfer
bound supplied independently. No allocation is derived from the declared total.

| Offset | Bytes | Value |
| --- | --- | --- |
| 0 | 4 | D3PC magic |
| 4 | 2 | Version1 |
| 6 | 2 | Kind: begin1, data2, end3 |
| 8 | 2 | Direction: archive1, session2 |
| 10 | 6 | Reserved zero |
| 16 | 16 | Parent document identity |
| 32 | 8 | Parent request identity |
| 40 | 8 | Parent revision |
| 48 | 8 | Captured connection generation |
| 56 | 8 | Payload offset |
| 64 | 8 | Exact total payload bytes, repeated unchanged |
| 72 | 4 | This chunk payload bytes |
| 76 | 4 | Reserved zero |

Begin has offset/body zero and a positive total. Data offsets are aligned to the
frozen chunk size; each body is exactly min(chunk size, remaining bytes). End has
offset==total and no body. Canonical chunks make physical overhead deterministic:
ceil(total/chunk)+2 headers, including begin/end. Count/header/byte arithmetic is
checked independently; whole payloads can exceed legacy frame capacity without a
transfer-sized allocation in these APIs. No global payload limit is increased.

PreviewChunkSequence binds independently captured document/request/revision/generation/
direction and policy. It accepts begin, exactly contiguous data and one end marker.
Actual observed body size must match each header. Only actual EOF after end completes
framing; all errors, early/repeated EOF, replay, trailing input and mismatches seal the
state. It is a collecting-thread state object with no I/O, payload retention or peer
inspection. Observed byte counts/EOF must come from the actual future reader.
Correlation is not authentication; framing completion is not semantic/current-source
admission or permission to publish application state.

## Verification

The pure contract started red on missing API. Fixed header offsets/round trips,
reserved/version/kind/direction/identity checks, canonical sizes, physical accounting,
replay/gaps/changed totals/short bodies/terminal-plus-EOF states pass. Tiny existing
session bytes still undergo their independent semantic codec. A full native archive
plus24-byte request prefix is represented using scalar headers/counts only; no large
buffer or native archive/parser/generator executes. Distinct addition/multiplication
physical-byte overflow cases are scalar-only. No native malformed-input/fault probe.

Fresh CMake build and215/215 ordinary CTest contracts pass (38.92s), actual SwiftPM
runtime build0.95s, pinned clang-format18, strict Swift and quarantine guard pass.
Five source hashes match. Fresh scoped ASan/UBSan passes with protocol archive only,
no engine library, leak detection disabled and existing codecs uninstrumented.
New framing coverage156/156 lines,126/126 regions,17/17 functions and102/106 branch
outcomes96.23%. Some header alternatives/32-bit policy guards remain unexercised;
this is not whole-protocol/core coverage or high-resource acceptance.

## Remaining implementation

Do not expose this route to native generation or default app adoption yet. Next add
actual bounded byte readers/writers, owned request/session body handling and complete
semantic admission under the range ledger, including independent native/source/retained
budgets, warnings/text, original fallback diagnostics and optional real statistics.
Freeze capabilities from host policy; do not auto-downgrade to parent native work or
legacy narrower caps. Include IPC/channel/copy retention separately from these physical
chunk counts. Stream publication still requires authenticated peer, real EOF, valid
control receipt, current-source queue admission and complete immutable C/Swift owners.

Production pin48b0ce2d and all quarantines are unchanged. No large allocations/pressure,
crash/fault/signal/broken-pipe/native-cancellation, denied-peer or GLB probes. Apple
#279/#280 and M4–M8 remain open. Clean completed exact-head automated review and ALL CI
remain mandatory before merge.
