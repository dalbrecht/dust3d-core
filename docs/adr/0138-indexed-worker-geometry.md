# ADR 0138: Preserve indexed worker geometry under shared validation

Status: Accepted implementation, pending reviewed delivery.

## Context

The Apple renderer consumes indexed geometry; expanded-only worker output is not
complete parity under Apple #279. Core #219 routes expanded geometry/source data,
but it does not transfer the native indexed vertex table and triangle indices.
Core #221 adds a reusable indexed value codec before native copying, paired
source admission and C/Swift runtime adoption.

## Decision

Add internal C++17 `IndexedMeshPayload`: owned full-attribute vertex records,
UInt32 triangle indices, node/part counts and warning text. Preserve table order,
corner order and every original attribute bit; do not deduplicate, normalize or
regenerate data. Finite unused vertices are permitted and validated too. The
native indexed producer remains responsible for its own deduplication semantics.

Use a **48-byte big-endian version-one prefix**:

| Offset | Field |
| --- | --- |
| 0 | u16 version 1 |
| 2 | u16 prefix size 48 |
| 4 | u32 reserved zero |
| 8 | u64 vertex count |
| 16 | u64 triangle-index count |
| 24 | u64 original node count |
| 32 | u64 original part count |
| 40 | u32 vertex stride 40 |
| 44 | u32 warning byte count |

Exact warning bytes follow, then 40-byte IEEE-754 vertex records (position xyz,
normal xyz, color rgba), then four-byte UInt32 indices. Triangle count is derived
from a nonzero index count divisible by three; vertex count need not be divisible
by three. Unknown fields, truncation and trailing data reject.

Independent supported quotas bound wire bytes, retained logical vertex/index/
warning charge, counts and warning bytes. Checked subtraction/division precedes
aggregation/multiplication; supported spans must fit size_t at compile time.
Logical charge excludes root objects, capacities, allocator overhead, workspace
and process RSS. The defaults are 128MiB retained, 256MiB wire, 3M vertices,
9M indices, 1M source counts and 4096 warning bytes.

Share the existing expanded codec's geometry/text implementation through internal
`WorkerMeshValidation` helpers. Validate finite positions, unit normals, finite
unit colors and positive finite triangle area identically. Warning UTF8/control
rules are unchanged. Low-level 40-byte vertex readers/writers require caller-
validated spans, like the existing wire helpers; they expose no public C ABI.
Nonfinite wire bits reject before floating-point operands are constructed.

Both encoder and inspector validate every vertex, including unused table entries,
and every triangle reference/area. Decoder performs complete inspection before
constructing any returned vertex/index/text arrays. Shared geometry/text errors
retain their original `MeshPayloadError` causes; layout/count/index errors use
`IndexedMeshPayloadError`. No partial value is returned.

No active operation, public C ABI, Apple pin or default backend changes. Indexed
source pairing/native copying, runtime/C/Swift output and indexed/material/mip
parity remain open. This value codec alone does not establish editor acceptance,
native generation containment, CPU/RSS bounds or source authenticity.

## Validation and limits

- TDD red: the new contract failed to compile for missing WorkerIndexedMesh.h.
- Pure tests preserve a four-vertex/six-index shared-corner fixture, negative-zero
  bits, original colors/normal values, UTF8 warning and source counts. Exact byte/
  count quotas accept; smaller/unsupported policies reject. Owned output survives
  input/wire release. Expanded reconstruction checks shared triangle semantics.
- Bounded framing/count/index cases cover complete-prefix truncation, unsupported
  fields, trailing data, invalid indices at the first and last triangles, invalid
  source/warning counts, degenerate triangles, nonfinite/invalid attributes,
  invalid unused vertices, unsupported warning text and null warning storage.
  These are pure codec inputs; no native engine execution is added by the tests.
- New indexed and rebuilt expanded regression contracts pass. Fresh full build,
  source quarantine and **149/149 ordinary CTests pass in 41.98 seconds** in the
  established authorized execution context; changed-source hashes remain stable.
  Static clients are relinked against the changed shared implementation.
- Fresh pure-codec ASan/UBSan passes with leak detection disabled. Both indexed
  and shared mesh implementation are instrumented; no inherited engine or leak
  coverage claim. Scoped new indexed codec coverage is **131/131 lines,
  100/100 regions, 10/10 functions and 86/86 branch outcomes (100%)**. That figure
  applies only to WorkerIndexedMesh.cc, not shared legacy code or the full engine.
- SwiftPM runtime packaging, C++/manifest formatting and whitespace checks pass.
  Existing SwiftPM native-builder deprecation remains. One shared build root only.
  No UI exists in this stage; screenshots do not apply.

All core #67/#69 GLB, #83 writer, #84 fault, #95 animation and Apple #199 rig-pose
quarantines remain. No new native malformed-input, crash, allocation-failure,
signal, broken-pipe, native cancellation or resource-pressure probes, stale
binaries, app control or global SVG/protected-QA/OS-container modifications.
Completed clean exact-head automated review and all CI passing remain mandatory
before merge.
