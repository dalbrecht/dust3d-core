# ADR 0063: Owned worker base-mesh payloads

Status: implemented locally for #110; public review and hosted CI pending.

## Decision

Add a separate semantic codec to the standard-library-only worker protocol
library. It represents the existing untextured expanded triangle result: positions,
normals, RGBA, source node/part counts and the original compatibility warning.
It has no dependency on native handle layout, Qt, Apple APIs or engine execution.
The real-engine test links the engine; the codec library does not. Common integer
byte helpers move to WorkerWire.h, with the existing independent frame fixture
protecting its wire format.

The parent must first validate framing and the pending identity, then decode and
validate this operation-specific payload, and recheck current document state before
publication. This codec does not launch a process, retire a request or establish
containment. UVs, material policies/maps, source attribution, rigs and animations
require explicit future operation schemas; they must not silently use this base
mesh schema. No quarantine, C ABI, production pin or app rendering changes.

## Wire schema

All integers and IEEE-754 binary32 scalar bits use big-endian order. No native
struct or object serialization is used. Version one has a 40-byte prefix:

| Offset | Bytes | Meaning |
| --- | --- | --- |
| 0 | 2 | Mesh schema version, exactly 1 |
| 2 | 2 | Prefix length, exactly 40 |
| 4 | 4 | Flags, zero |
| 8 | 8 | Expanded vertex count, positive and divisible by three |
| 16 | 8 | Source node count; zero allowed |
| 24 | 8 | Source part count; zero allowed |
| 32 | 4 | Wire vertex stride, exactly 40 |
| 36 | 4 | Following UTF-8 warning byte count |

Warning bytes follow immediately, then vertices. Each vertex contains three
position, three normal and four color scalars. Unaligned wire offsets are read as
bytes; checked scalar bits are copied into a float, never interpreted as a native
object. Nonfinite wire exponents reject before constructing a floating operand.
Signed zero and all admitted scalar bits survive transport.

Every coordinate must be finite, each normal's squared length must be within
0.001 of one, and all RGBA components must be finite and in [0,1]. Every triangle
must have positive finite area at exported float precision, computed in double.
Empty successful geometry is rejected; a future worker must carry no-geometry
through its separate typed failure schema. Warnings require valid UTF-8 without
NUL, DEL or ASCII controls other than TAB/CR/LF. Text is retained, not logged or
interpolated into validation errors. This is not a universal display sanitizer.

## Bounds and ownership

Defaults permit a 256 MiB wire payload, 128 MiB logical decoded data, three million
vertices, one million nodes/parts and 4096 warning bytes. All limits are positive;
hard maxima are 256 MiB wire/decoded data, 256 MiB divided by 40 vertices, one
million nodes/parts and 64 KiB warning text. Combined prefix/text/record arithmetic
is checked separately, so an individual count ceiling is not a promise that the
whole payload fits. Limits smaller than the prefix or warning reject before
subtraction can wrap. Complete exact spans are required; fragments and trailing
bytes reject. Counts and lengths are bounded before output allocation.

Decoded quota charges logical native vertex records plus warning bytes. Roots,
container capacities, allocator overhead, input/frame copies and transient storage
are excluded. It is not an RSS or process-memory guarantee. The decoder validates
all triangles before returning independent owned storage; on error, RAII destroys
partial private data. Ordinary allocation exceptions propagate to the future
infrastructure boundary. No exception containment claim crosses a C ABI here.

## Verification

Tests first failed on the missing semantic header. An independent prefix fixture
and explicit float bytes verify wire layout. Small ordinary cases cover exact and
minus-one wire/decoded/count/text limits, every truncated prefix/body span,
trailing input, invalid options/schema/layout/counts, nonfinite positions,
nonunit normals, invalid colors, degenerate triangles and malformed UTF-8.
Decoder-side raw mutations ensure these semantic checks run on incoming bytes,
not only on encoding. Unicode, empty warnings, zero source counts and signed zero
round-trip. A licensed Capsule is generated using the ordinary C API, copied,
destroyed, framed with matching parent identity, decoded and compared bit-for-bit:
240 vertices plus exact source counts and warning. No worker or quarantined parser
is launched. No allocation-failure, crash, signal or resource-exhaustion probe runs.

At the initial semantic checkpoint 8cde28ee, normal CMake/CTest passed 55/55 in 29.71 seconds. ASan/UBSan passed all
54 C/C++ contracts in 80.95 seconds; the macOS Swift consumer remains normal-only.
Focused instrumented sanitizer execution also passes. Coverage of WorkerMesh.cc,
its inline error accessor and WorkerWire.h is 194/195 lines, 179/180 regions,
15/15 functions and 156/158 branch outcomes. The unhit line is the defensive
address-space guard; another unexercised branch is a UTF-8 prefix upper bound.
These are scoped measurements, not inherited-engine coverage or exhaustive fuzz
acceptance. No artificial probe is added to force the defensive address-space path.

## Remaining release work

Typed worker failures, parent pending-request state, partial I/O and cancellation,
real worker/transport, queue/time/resource budgets, parent-death handling,
filesystem isolation and source-aware result schemas remain required. Existing
quarantines and the Apple production pin remain unchanged. Parent mesh adaptation,
GPU/render behavior, broad corpus performance and full editor/export parity are
not established by this codec.

The later helper stage exposes pure output-policy validation for its testable
native operation adapter. ADR 0065 records that stage; the scoped measurements
above remain historical for the initial semantic codec.
