# ADR 0113: Development UV helper dispatch and parent admission

Status: prepared development stage for #164; production adoption remains gated.

## Decision

Extend the existing version-1 envelope with `generateUvMesh` opcode 2. Keep opcode
1 and its raw archive/mesh schema unchanged. Opcode 2 carries the bounded UV
request schema (ADR 0111) and returns a complete paired geometry/UV payload (ADR
0110), produced from one native owner (ADR 0112), or the existing failure schema.
Both operation entry points explicitly reject the other opcode before execution.
The helper dispatches after the existing exact frame/EOF validation; malformed
request or framing errors keep existing nonzero-exit handling. Native operational
failure is a complete failure frame with original status/cause when serializable.

Append `UvMeshPayload` to `AdmittedReply`, preserving mesh/failure positions 0/1.
After retirement, observed normal exit zero, current document/request/revision,
and matching response identity/opcode, `PendingReply` chooses the corresponding
payload decoder. Failures use the original independent failure policy. A paired
policy is appended to `PendingReply::complete` and `RequestQueue::complete`, so
existing base-call argument positions retain their meaning. UV inner geometry
uses `uvLimits.geometry`, not the separate base-mesh policy. Original typed
inner errors propagate. No geometry is published when UV admission fails.
Queue completion still releases matching active work before parsing; foreign
completions cannot release it, and retired completions cannot replay.

No new process machinery, retries, signing, sandbox, production pin or quarantine
changes. The caller must supply authoritative active identity and an actually
observed terminal outcome; bytes never prove that the child exited successfully.

## Verification

The new test first failed compilation for missing opcode, reply producer and
paired admission interfaces. Fresh source quarantine guard/audit and clean build
register 119 ordinary contracts. All 119 pass (20.34 seconds). The two focused
UV dispatch/admission contracts pass (0.65 seconds).

A freshly built helper processes licensed Capsule through normal and supervised
transports, exits normally zero, and yields the complete same-owner response
expected from the in-process producer. The parent admits 240 geometry corners
and 80 correlated UV triangles. Supervised success reports no exceptional cleanup
or partial-attempt diagnostics. No cancellation, signal, broken-pipe, allocation
failure, crash, resource-exhaustion or malformed native-input probe is added.

Pure synthetic values test wrong operation/current revision, failed outcome,
malformed late UV data, exact dispatch, replay retirement, paired-policy limits,
queue slot release and opaque failure causes. Pure framing/admission ASan/UBSan
and scoped changed native reply wrappers pass. Inherited engine/helper/transport
are not thereby fully sanitizer-instrumented. Combined existing-base and new-UV
pure tests measure `WorkerAdmission.cc` 28/28 lines, 25/25 regions and 14/14
branches. Merged profiles have one expected differing `main` hash; all shared
admission function hashes match. This is scoped module coverage, not whole-core
or live OS failure coverage. Native failure reply construction remains unforced.
Incremental unchanged SwiftPM compatibility passes in 0.09 seconds; this worker
is CMake-only. Changed formatting and whitespace checks pass.

## Remaining acceptance

Development transport functionality does not establish a hardened application
boundary. Trusted binary provenance, production resource/containment policy,
filesystem/descendant/parent-death controls, broad visual-family fixtures, baking,
renderer lifecycle adoption, dependency review and full M4/M8 remain open.
Preserve all existing quarantines; do not enable unsafe GLB or animation paths.

## Serial delivery verification

Replayed onto the accepted shared component-key/chart-image text-budget fix
from #190, preserving its pure regression test. The current source-audited
inventory contains 120 ordinary contracts; all pass in 20.58 seconds. Eleven
pure preflight contracts, the healthy dispatch pilot, pure protocol/admission
ASan/UBSan checks and the scoped dispatch wrapper sanitizer pass. The scoped
wrapper instruments changed wrappers/codecs; inherited engine/helper/transport
are not fully sanitizer-instrumented. No exceptional native failure is forced.

The pure malformed-metadata admission fixture uses bounds-checked vector access.
Changed C++ formatting, whitespace and recorded source hashes pass. SwiftPM
`dust3d-export` compatibility builds using the shared cache; the existing native
build-system deprecation warning remains. The earlier 119-contract timing and
coverage figures above describe the original prepared stage, not a new
whole-core coverage measurement. Production adoption and all quarantines remain
unchanged.
