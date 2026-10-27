# ADR 0170: Checked native preview range accounting

Status: proposed for reviewed delivery. Tracks #270 and Apple #280.

## Decision

Add an internal pure scalar planner for actual archive/geometry/source observations.
Calculate source-request, indexed, source-pair and session payload lengths independently
from native indexed/source charges and worker retained logical charges. Native record
charges use the actual producer record types; serialized widths/prefixes use codec
constants. Source identity text excludes terminators, which are charged separately.
Auxiliary wire/logical totals must already come from validated diagnostics/statistics
codecs and include the mandatory absent-statistics prefix. Preserve ordinary source
fallback diagnostics and successful no-geometry without fabricating an empty mesh.

Every multiplication/addition/conversion boundary is checked. Validate complete native
index counts, source membership/corner correspondence, independent native budgets and
host addressability before returning the plan. This does not inspect actual indices,
vertices, UTF8 or auxiliary bytes and cannot be used as semantic admission or transport
authentication. It allocates no archives/geometry and never calls generation.

Archive input retains its256MiB native ceiling. Indexed zero selects256MiB; source zero
selects64MiB and source policies remain capped at256MiB. Crucially, Apple validation
accepts any nonnegative indexed Int budget, and native indexing accepts an explicit
positive size_t budget.256MiB is an indexed default, not its hard maximum. The planner
preserves explicit positive addressable indexed budgets instead of introducing a cap.
Actual index count still cannot exceed UInt32 and unique vertices cannot exceed corners.

Existing transport/opcode/count/text/decoded limits stay unchanged. This planner can
represent a result that legacy codecs cannot admit. It reports exact payload charges,
not framing overhead per future chunk, root/capacity/workspace/copy peaks or RSS/CPU.

## Evidence

The pure contract started red on the missing API. Tiny ordinary/source/fallback/empty
plans match actual existing complete session encoders; source wire size matches the
source codec. Scalar-only cases demonstrate the full256MiB archive plus24-byte prefix,
native geometry above incidental3M-vertex defaults, configured source output beyond
legacy frame capacity and explicit512MiB indexed policy. No large spans/arrays are
constructed and no native generator is called. Bounds/correspondence/arithmetic failures
preserve typed causes. The same source types compile on portable targets.

Fresh CMake build and214/214 ordinary CTest contracts pass (44.58s); actual SwiftPM runtime
build passes (0.98s). Pinned clang-format18, strict Swift checks and quarantine guard pass.
Five source hashes match final validation. Fresh scoped ASan/UBSan passes for the pure
contract, linked only with the ordinary protocol archive, without a native engine library.
Leak detection is disabled; existing codecs are uninstrumented. Planner coverage68/69
lines98.55%,65/66 regions98.48%,6/6 functions and51/56 branch outcomes91.07%. The host is
64-bit; address-space rejection remains unexercised. No whole-core coverage claim follows.

## Remaining transport and acceptance

The next design must add bounded segmented or descriptor-backed transfer with exact
identity/order/section/terminal state and complete semantic validation under this ledger.
It must preserve input range, warnings/text, source fallback, optional real statistics
and immutable C/Swift ownership together. Do not globally raise maximumPayloadBytes,
silently reduce native budgets or fall back to parent generation. Legacy caps remain
until that path exists. Physical chunk overhead, retention peaks and independently
validated count/text profiles must be included in its reviewed format/admission spec.

Only pure layout/state values and existing small ordinary positives are authorized;
no large-allocation/pressure, malformed-native-input, crash/fault/signal/cancellation
probes are added. High-resource runtime acceptance remains unproven. No production
pin/backend change, GLB activation or quarantine lift. Apple #279/#280 and M4–M8 remain
open. Completed clean exact-head automated review and ALL CI gate merge.
