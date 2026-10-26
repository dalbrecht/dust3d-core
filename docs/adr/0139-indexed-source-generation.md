# ADR 0139: Pair indexed sources from one native generation

Status: Accepted implementation, pending reviewed delivery.

## Context

Core #221 transfers indexed values; #219 routes expanded geometry/source output.
Apple #279 still requires indexed/source correspondence, actual native copying,
remaining rendering data and owned Swift conversion before worker adoption.
Source hints address triangle-corner positions, not unique vertex rows. Core #223
must preserve that distinction and same-generation ownership.

## Decision

Add internal `IndexedSourcesPayload` with indexed geometry and source hints.
Its **24-byte big-endian version-two pair prefix** contains u16 version 2,
u16 prefix size 24, u32 reserved zero, u64 indexed-geometry bytes and u64 source
bytes, followed by those exact existing complete inner formats. Expanded/source
version one remains unchanged; neither decoder silently reinterprets the other.

Independent inner policies and checked aggregate wire/logical quotas apply.
Encoder validates geometry and measures source values before final pair allocation;
bounded temporary encoded geometry is outside the retained charge. Decoder inspects
both complete inner spans, then checks aggregate charge, index/corner count and
node/identity count before retaining decoded arrays. Original indexed, shared mesh
and source error categories propagate. No partial geometry is returned on rejection.

Expose internal source inspection/copy helpers over the existing source-copy
implementation. Inspection returns counts/charges, not native pointers. Copying
performs its own preflight instead of trusting caller-supplied counts, then validates
the complete independent hints. Existing expanded/source behavior remains intact.

The native adapter generates one source owner, derives a native indexed owner
from its exact borrowed geometry, and keeps both alive through independent copying.
Indices and every full-attribute vertex bit are retained; source identities, corner
ranges, kinds, sentinels and unnormalized weights retain their order and values.
The public internal copy function accepts both borrowed owners with an explicit
same-generation derivation precondition. Count agreement is not source authentication.

Preflight source charge tightens the geometry policy. A pure scalar export-budget
function reserves warning bytes and the complete index table, then rounds down
complete vertex records within count, decoded and wire ceilings. It rejects zero
record allowance before native indexing, avoiding accidental selection of the native
API's zero/default budget. Native/owned vertex record charges must agree at compile
time. The indexed output then passes full codec validation before return. RAII
releases both native owners, and typed failures retain their original bounded cause.

These limits exclude root objects, capacities, native generation/lookup workspace,
encoding temporaries, CPU and RSS. Failure-payload allocation can still throw.
No active worker opcode, public C ABI, Apple pin or default backend changes.
Runtime/C/Swift conversion and material/mip/statistics parity remain open. Apple
#280 separately tracks preserving the editor's default separate geometry/source
budget range: current prototype aggregate ceilings do not establish that parity.
Do not silently tighten that range or fall back to parent native generation.

## Validation and limits

- TDD red: new pure pair contract failed to compile for missing WorkerIndexedSources.h.
- Pure four-vertex/six-index fixtures prove corners follow index positions rather
  than unique vertices. Exact aggregate quotas accept; smaller/unsupported quotas,
  count mismatches, truncated/trailing/layout/version/length errors reject.
  Independent framing tests ensure mismatches survive valid inner encodings.
  Original indexed/source/shared-geometry causes survive rejection. Pure scalar
  budget cases cover empty/partial triangles, metadata ceilings, warning/index
  reservation, zero vertex allowance, exact counts and whole-record rounding.
- Fresh licensed Capsule generation plus a second known-positive source/indexed
  owner validate actual 240-index/corner correspondence. Copy-only cases compare
  every native index, vertex bit, identity, corner and influence, accept exact
  aggregate quotas, reject twelve bounded copy policies without regeneration,
  and retain values after both owners/input release. Null-owner/configuration
  guards reject before native generation/indexing. No malformed archive reaches generation.
- New pair/native-copy and rebuilt existing source-copy contracts pass. Fresh
  full build, source quarantine and **151/151 ordinary CTests pass in 39.29 seconds**
  in the established authorized execution context. Changed-source hashes remain
  unchanged across the gate; static clients are rebuilt/relinked.
- Refreshed scoped pure and native-boundary ASan/UBSan pass, leak detection disabled.
  Selected codecs/adapters are instrumented; linked inherited engine libraries are
  not. No whole-engine sanitizer or leak-check claim.
- Pure pairing coverage: **75/75 lines, 50/50 regions, 8/8 functions, 40/40 branch
  outcomes**. Pure export budget: **17/17 lines, 26/26 regions, 1/1 functions,
  20/20 outcomes**. Both are 100% for their named files only. Native adapter coverage
  is **97/127 lines (76.38%), 69/103 regions (66.99%), 8/9 functions and 39/72
  outcomes (54.17%)**. Defensive native-storage and failure/exception mapping remain
  unprobed and below the adoption target; no unsafe probes are introduced to raise
  it. Partial shared/legacy coverage is not total runtime/engine coverage.
- SwiftPM runtime packaging, C++/manifest formatting and whitespace pass. Existing
  native-builder deprecation remains. One shared build root only; source-only WIP
  recovery copies do not create another build root. No UI changes/screenshots.

All core #67/#69 GLB, #83 writer, #84 fault, #95 animation and Apple #199 rig-pose
quarantines remain. No new malformed-native-input, crash, allocation-failure,
signal, broken-pipe, native cancellation or resource-pressure probes, stale binaries,
app control or global SVG/protected-QA/OS-container changes. Completed clean
exact-head automated review and all CI green remain mandatory before merge.
