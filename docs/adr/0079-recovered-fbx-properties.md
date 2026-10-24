# ADR 0079: Recover bounded FBX property encoding

Status: Delivery validated locally; review and complete FBX export acceptance pending (#99).

Recover the archived move-only in-memory FBX7400 property block independently of
scene, rig, animation and file writers. The private recovery checkpoint records
its exact saved-source identity. Leave the Qt and vendor reference trees intact;
their property tags and uncompressed array layout remain the local format
reference. No new dependency or public C ABI is introduced.

Encode scalars Y/C/I/F/D/L, binary strings/raw values S/R and arrays i/l/f/d/b in
explicit little endian. Preserve typed-name NUL separators, arbitrary raw bytes
and IEEE signed zero. Reject nonfinite numeric properties before mutation. Array
count and encoded length fit uint32 through the admitted byte ceiling. Arrays
use encoding zero and an exact checked byte length.

The retained quota includes the root and actual vector capacity: zero selects
64 MiB and the ceiling is 256 MiB. Each block permits at most 4096 properties.
Admit sizes before allocation or array scanning. Clamp geometric buffer growth to
the quota and verify actual reserve capacity before publication. Commit growth
by swap; in-place writes follow complete admission and nonthrowing primitive
encoders. Self-alias raw input is snapshotted before mutation. Invalid appends
preserve prior bytes/count; ordinary allocation exceptions propagate for a future
caught C boundary. Input buffers, temporary growth/self-alias copies and allocator
overhead are excluded; this is not a process memory or RSS limit.

Borrowed bytes expire on mutation/move/destruction. Calls on one block must be
serialized. A moved-from block may only be destroyed or reassigned. The current
value tests began red on the missing header and exercise exact wire bytes,
signed extremes/zero, all property types, embedded separators, empty values,
self-alias growth, move ownership and small explicit byte/count admission limits.
These do not exhaust physical resources or inject allocator failures. The
allocator-capacity guard must remain unforced and its coverage gap reported.

Property bytes alone do not establish valid FBX scene output. Bounded nodes,
document offsets/footer, scene/material construction, independent-consumer
re-import and Apple UI/CLI adoption remain required. All quarantines, production
pins and the serial completed-review/all-green-CI merge gate remain unchanged.

## Initial local verification

The source quarantine guard and formatter pass. Compared with the preceding safe
stage, the only added registered test is the audited property-block contract.
A clean Debug CMake rebuild and all 65 CTest contracts pass (32.43 seconds).
A fresh strict-warning C++17 ASan/UBSan build of the property implementation and
its contracts passes; this is scoped instrumentation, not a whole-engine claim.
The updated core SwiftPM product builds successfully. Scoped compiler coverage
is 126/127 lines (99.21%), 73/75 regions and 34/35 branch outcomes. The unexecuted
allocator reserve-capacity guard remains documented and unforced. Tests establish
ordinary byte/count/nonfinite rejection behavior; allocation failure is not
tested by injection. No archived binary, quarantined fixture or native evaluator
was run.

## Delivery revalidation

Prepared on the separately tested transform-delivery stage `2b1bee40`, retaining
reviewed worker/writer contracts and the tested FBX convention prerequisites.
The only newly registered contract is the audited property-block test. The fresh
ordinary CMake build and 71/71 CTest contracts passed (16.92 seconds), as did fresh
standalone strict-warning C++17 ASan/UBSan property contracts and the SwiftPM
product build (0.76 seconds). Source quarantine, formatting, manifest lint and
Qt-boundary checks passed. Earlier 65-case and coverage measurements above are
historical and were not remeasured. The reserve-capacity guard remains unforced;
no allocation failure, exhaustion, evaluator or quarantined fixture was run.
Prerequisite review, property review and full export acceptance remain required.
