# ADR 0064: Typed worker non-success payloads

Status: implemented locally for #111; public review and hosted CI pending.

## Decision

Complete the result wire vocabulary with a separately versioned typed non-success
payload. Keep it separate from framing and base-mesh semantics in the existing
standard-library-only worker library. It does not link the engine; the native
cause integration test does. No C ABI, SwiftPM engine source list, Apple pin,
process launch or quarantine changes.

Native diagnostics are bounded C bytes and can already be truncated through a
UTF-8 sequence. Preserve the bytes actually reported rather than assuming valid
text or replacing the original cause. The owned vector may contain NUL, control
bytes and non-UTF-8 sequences. It is data, never a command, format string or log
message. Future presentation must format it safely and retain original bytes;
validation errors themselves use static messages without interpolating payloads.
This cannot recover truncation that occurred before the native ABI reported it.

## Version-one wire schema

All integers use big-endian order. The prefix is exactly 16 bytes:

| Offset | Bytes | Meaning |
| --- | --- | --- |
| 0 | 2 | Failure schema version, exactly 1 |
| 2 | 2 | Prefix length, exactly 16 |
| 4 | 2 | Typed non-success status |
| 6 | 2 | Flags, zero |
| 8 | 4 | Reserved, zero |
| 12 | 4 | Exact following original-cause byte count |

Stable statuses are invalid document 1, unsupported asset 2, generation failed 3,
limit exceeded 4, no geometry 5 and cancelled 6. Success zero and unknown values
reject. A future native adapter must map statuses explicitly, not assume enum
layout compatibility. No geometry remains a distinguishable normal empty-output
outcome for the application; this payload does not turn it into an alert policy.
All six statuses permit an originally empty diagnostic.

Default cause quota is 4080 bytes and complete-wire quota is 4096 bytes, consistent
with the default outer failure-frame limit. Hard maxima are 65520 cause bytes and
65536 complete bytes. Limits are positive and checked independently; limits too
small for the prefix reject. Advertised cause lengths are bounded before copying.
A complete exact span is required, with truncation and trailing bytes rejected.
Returned bytes are independently owned. Quotas exclude roots, container capacity,
allocator overhead and other input/frame copies; they do not cap RSS. Ordinary
allocation errors propagate to the future infrastructure boundary. Another
allocation failure during error reporting is not claimed to be contained here.

The future parent must validate the outer failure kind and pending document,
request and revision identity before interpreting this payload, and recheck current
application state before publication. This does not retire requests, stop a child,
retry input, save documents or establish process containment. Transport/exit errors
remain distinct from an admitted operation outcome.

## Verification

Test-first compilation failed on the missing failure codec header. An independent
wire fixture verifies offsets and byte order; all six numeric status IDs have
explicit contract checks. Tests cover ownership, originally empty and binary
causes, exact/minus-one cause and wire quotas, prefix-too-small quotas, invalid
limits, every truncated prefix/body span, trailing input, unsupported versions,
flags/reserved/layout/status errors and hostile advertised lengths without large
allocation. A real ordinary three-byte invalid archive returns a native error;
its bounded diagnostic survives framing, identity admission and payload decoding
byte-for-byte after owner/source storage release. No allocation-failure, crash,
signal, resource-exhaustion or quarantined GLB/writer/animation probe runs.

Full normal macOS CMake/CTest passes 56/56 in 30.03 seconds. ASan/UBSan passes all
55 registered C/C++ contracts in 96.61 seconds; the Swift consumer stays normal-only.
Focused instrumented sanitizer execution also passes. WorkerFailure.cc, its inline
error accessor and WorkerWire.h measure 75/76 lines, 54/55 regions, 10/10 functions
and 47/48 branch outcomes. The unhit line/branch is the defensive address-space
check, unreachable under the wire cap on this host. No artificial probe is added
to reach it. These are scoped metrics, not inherited-engine coverage or a release
containment claim. Builds reuse one shared folder and clean profiling products.

## Follow-ups

Use the codecs in a real single-job helper and parent transport with bounded
queue/time/resource policy, complete-result/exit admission, cancellation and
parent-death handling. Preserve parent-owned document/save state and reject stale
or partial outputs without automatic retries. Source-aware UV/material/rig/animation
schemas, sandbox/file containment and full app/export acceptance remain required.
All existing quarantines and the production core pin remain unchanged.
