# ADR 0173: Owned preview archive requests

Status: proposed for reviewed delivery. Tracks #270 and Apple #280.

## Decision

Add a collecting-thread archive owner over accepted D3PC byte framing. Its caller
captures document/request/revision, connection generation, exact archive count and
native source/indexed policies independently before collection. Only the archive
transfer direction is permitted. Constructor validation accepts the native archive
range of one byte through 256 MiB; source zero/default or up to 256 MiB; indexed
zero/default or any positive addressable native policy, including above its 256 MiB
default. Construction allocates only the bounded reader workspace, never the archive.

The begin marker must promise exactly the captured archive count plus the existing
24-byte source-request prefix. Inspect the prefix across arbitrary chunk boundaries
and compare its archive count and original source policy with captured controls
before allocating one contiguous archive. The indexed policy has no field in this
existing prefix: it remains independent captured metadata for later native admission.
No policy may be silently normalized or derived from untrusted archive contents.

Native C entry points require a contiguous readable archive. Allocate that storage
once after the prefix agrees, then copy actual chunks with a checked remainder.
Do not retain a duplicate whole envelope. Logical storage is one exact archive,
the fixed prefix and the reader's bounded workspace/header; root/optional/callback
storage, allocator bookkeeping and RSS are outside that measurement. All mutation
errors seal the owner and preserve the original exception. Partial storage remains
private and is released through RAII. Complete immutable bytes can be borrowed only
after the reader observes exact framing and the caller supplies actual EOF. An
incomplete view query throws without mutating collection. A later rejected mutation
does not free storage already borrowed; all borrowed pointers expire at destruction.
The owner cannot be copied or moved, preserving its internal callback address.

Extract fixed-prefix encoding/inspection into the existing source codec, sharing
version/reserved fields, nonempty count, source-policy and addressability validation.
Inspection requires exactly 24 bytes and performs no native archive parsing. A fixed
prefix can describe the full native archive range without allocating its body. The
legacy whole-envelope encoder/decoder retain their existing independent wire and
retained limits; prefix-plus-archive still cannot exceed their existing 256 MiB cap.
No production route or active quota is widened by this foundation.

## Evidence

The test started red on the missing owner API. A seven-byte opaque payload is emitted
with existing request encoding and D3PC framing, then ingested one byte at a time.
Neither the end marker nor an incomplete view query exposes bytes. After actual EOF,
owned bytes survive overwriting both input buffers. One-, 24- and 64-byte chunks
exercise split and combined prefix/body callbacks; changes to the caller's original
context do not affect frozen controls. Zero/default and explicit policies survive.
Native maximum archive/source fields and an indexed policy above default are checked
as scalars only, without allocating large buffers. The fixed prefix matches existing
wire bytes; the unchanged legacy envelope rejects the maximum archive plus prefix.

Tiny protocol-only cases cover unsupported/reserved prefix fields, invalid captured
limits/direction, insufficient policy, foreign connection generation, mismatched total,
archive count/source budget, trailing input, truncation, null spans and repeated EOF.
Original prefix errors survive later mutation and view attempts. No malformed archive
is parsed, no native generator runs in the new test, and no crash, fault injection,
OS signal/broken-pipe/cancellation, large-allocation or pressure probe is added.

Fresh complete build and 217/217 ordinary CTest contracts pass (48.74 seconds).
Actual SwiftPM runtime build passes (2.09 seconds, with its existing native-build-system
deprecation warning). Both pinned clang-format 18 and the newer local formatter,
quarantine guard, strict package formatting and seven source/build/test hashes pass.
A fresh scoped ASan/UBSan executable links protocol code only, with no engine library,
leak detection disabled and other existing protocol code uninstrumented. New owner
implementation: 69/70 lines (98.57%), 48/49 regions (97.96%), 9/9 functions and 29/32
branch outcomes (90.62%); inline getters: 2/2 lines, 4/4 regions, 2/2 functions and
4/4 branch outcomes. The retained local remainder guard cannot reject after exact
framing, and address-space alternatives are unforced on this 64-bit host. These are
explicit residual defensive paths, not artificial coverage probes. The changed legacy
codec has 69/78 lines, 45/52 regions, 9/10 functions and 29/40 branch outcomes in this
narrow instrumented test; its broader existing codec contracts also pass in the full
gate. These figures do not establish whole-core coverage or real-channel acceptance.

## Remaining gates

This owner establishes bytes and captured request controls, not valid archive contents,
authenticated peer identity, current source state or authorization to dispatch native
work. Real channel EOF must come from a bounded OS adapter. Complete wide session
output semantics, original diagnostics/warnings/statistics, native dispatch, immutable
C/Swift result owners and reviewed Apple adoption remain under #270 and Apple #279/#280.
No parent-process native fallback or silent native range reduction is permitted.
Production pin 48b0ce2d, active caps and every feature quarantine remain unchanged.
M4–M8 and full parity remain incomplete. Completed clean exact-head automated review
and all hosted CI checks are required before merge.
