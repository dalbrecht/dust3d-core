# ADR 0062: Version-one portable worker framing

Status: implemented locally for #109; public review and hosted CI pending.

## Context and decision

ADR 0025 requires a bounded worker process before reconsidering the quarantined
GLB catalog. Define its framing separately from engine execution, platform process
transport and semantic result admission. The C++17 `dust3d_worker_protocol` CMake
library depends only on the standard library. It does not link the engine, expose
a new C ABI or enter the SwiftPM engine source list. Files live beside other native
boundary code so the existing native formatting/Qt checks cover them.

No process is launched. No catalog, worker executable, sandbox, Apple pin change,
resource enforcement or re-enablement is implied. The only operation tag currently
recognized is `generateMesh`; recognition does not execute it. Unknown versions,
kinds, operations and reserved fields fail closed.

## Wire format

Never dump an in-memory struct or native enum. All integers use big-endian order.
The version-one header is exactly 64 bytes:

| Offset | Bytes | Meaning |
| --- | --- | --- |
| 0 | 8 | ASCII `DS3WORK` followed by NUL |
| 8 | 2 | Version, exactly 1 |
| 10 | 2 | Header length, exactly 64 |
| 12 | 4 | Flags, zero |
| 16 | 2 | Kind: request 1, response 2, failure 3 |
| 18 | 2 | Operation: generate mesh 1 |
| 20 | 4 | Reserved, zero |
| 24 | 8 | Nonzero parent-issued request identity |
| 32 | 8 | Parent-issued document revision; zero allowed |
| 40 | 16 | Nonzero parent-issued live document identity |
| 56 | 8 | Exact following payload byte count |

Document identity denotes an open-document lifetime/epoch, not merely a persistent
filename or archive identifier. The parent must issue distinct identities on
reopen and never reuse a request identity while an old reply can arrive. IDs are
stale-result guards, not authentication or proof of trustworthy payload contents.
A valid result/failure must match the pending operation, document, revision and
request; a request cannot masquerade as a response. Duplicate response retirement
and queue ownership remain responsibilities of the future parent state machine.

Limits are explicit and positive. Defaults permit 128 MiB request payloads,
256 MiB result payloads and 4096 failure bytes. Hard maxima are 256 MiB for
request/result payloads and 64 KiB for failure bytes. A fixed header can be decoded
and its advertised length rejected before body allocation. Whole-frame decoding
requires the exact complete payload span and rejects truncation or trailing bytes
before copying. Returned payloads own their storage. Zero payloads are a semantic
layer decision. Limits count logical wire bytes, excluding framing, capacities,
allocator overhead, decoded data and RSS; they do not establish memory isolation.

Typed ProtocolError codes distinguish limits, truncation, malformed/unsupported
headers, invalid identities and stale/unexpected responses. Messages are static
and never include document bytes. Ordinary allocation errors propagate to the
future infrastructure boundary; no failure injection or resource-exhaustion probe
is used to test them.

## Required transport and result follow-ups

The future parent reads exactly one fixed header into bounded storage, validates
it and its pending identity before allocating/reading the advertised body. EOF,
timeout and cancellation discard fragments. After complete framing, validate the
operation-specific schema, finite geometry, counts, indices, image layout and
independent decoded-output limits; check current document revision again before
publishing. A framed body is not an admitted mesh or diagnostic. Failure payloads
still need a typed, bounded schema and safe original-cause handling.

Parent-owned document/save state, bounded pending queues, single-use response
admission, elapsed-time/resource budgets, process launch/exit and parent-death
handling, sandbox/file staging and no automatic retry of crashing inputs remain
required. Exercise ordinary inputs and fake/incomplete transport states without
intentional parser crashes, signals or allocation failures. Existing #67/#69,
#83/#84/#95 and Apple #199 quarantines and production pin remain unchanged.

## Verification

Tests first failed compilation on the missing protocol header. Small independent
wire bytes verify endianness and offsets. Tests cover all header-prefix lengths,
actual truncated/trailing spans, magic/version/flags/reserved/kind/operation errors,
zero identities, initial revision zero, independent per-kind exact/over limits,
hostile advertised lengths without body allocation, owned byte lifetimes and
stale document/request/revision or wrong-kind responses. Tests use real bounded
arrays/vectors and null inputs, never fabricated non-null pointers.

The full normal macOS suite passes 54/54 in 31.84 seconds. ASan/UBSan passes all
53 registered C/C++ contracts in 166.40 seconds; the Swift consumer remains
normal-only. A focused sanitizer/coverage run also passes. WorkerProtocol.cc
measures 110/110 lines, 90/90 regions, 14/14 functions and 71/72 branch outcomes.
The unhit branch is the defensive size_t address-space guard, unreachable under
the hard cap on this 64-bit host. No artificial probe is added to reach it.
These are file-scoped measurements, not inherited-engine or full release coverage.
All builds reuse the shared folder and clean instrumented products afterward.
