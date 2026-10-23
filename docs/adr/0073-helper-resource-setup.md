# ADR 0073: Private helper resource setup adapter

Status: local development library; not activated in the worker or Apple app.
Refs #123. Follows the policy and implementation contract in ADRs 0071–0072.

## Decision

Add a portable ResourceSystem inspection/application boundary and a
prepareWorkerResources operation. It gathers observations for all requested
resources, validates the complete profile through planWorkerResources, then applies
the finite effective entries in stable order. It returns the plan only after all
applications succeed. No document/engine operation runs inside this function.
On any error, callers must exit setup and must not begin archive processing.
Partial OS application cannot be rolled back: do not retry or continue after a
failure. Fresh-helper ownership is a caller precondition.

ResourceSetupError retains the failed inspection/application, original resource,
captured system error code and system-category description. ResourceError policy
causes propagate unchanged. The OS adapter must capture errno immediately; the
orchestrator does not inspect ambient errno, replace the cause or log document data.

The private macOS/Linux UnixResourceSystem converts CPU/file/core/descriptor/virtual
resource identifiers to their OS constants and translates RLIM_INFINITY to an
explicit optional inherited value. Finite values stop below that sentinel. Compile-
time assertions reject unreviewed resource-width/signedness/saved-sentinel layouts.
Unknown resource enums, reversed finite caps and out-of-range finite conversions
are rejected before an application syscall. Resource application must use a plan
from the same exclusively owned, unchanged process setup; the adapter is not a
cross-process privilege boundary. Header sys/resource.h stays in the private Unix
adapter, outside portable policy and the public C ABI.

UnixResourceCalls is an explicit borrowed syscall seam; the actual
SystemUnixResourceCalls captures getrlimit/setrlimit results. Construct that
implementation only in a fresh helper after a reviewed activation contract. Tests
construct a recording implementation exclusively. The portable setup and Unix
adapter compile as separate development libraries; neither is wired into the
current helper main, parent transport, SwiftPM engine or Apple app. No process
limits are installed by this stage and no production profile is selected.

## Evidence

Test-first compilation fails on the missing setup API. A recording syscall backend
verifies exact resource mappings, query-before-apply event order, effective values,
infinity handling, maximum finite conversion, stricter inheritance and core zero.
For each of the five resources, simulated inspection errors preserve EACCES without
mutations and simulated application errors preserve EPERM with no later calls,
retry or rollback. These are returned API error values, not OS fault/crash probes.
Late invalid descriptor policy, incompatible ASan virtual policy and invalid
inherited values prevent all writes. Unknown resources and invalid direct adapter
arguments never reach the recorded write operation.

Fresh ordinary CMake/CTest passes 64/64 in 32.78 seconds. Focused strict-warning
ASan/UBSan execution and formatting pass. Instrumentation reports setup source and
accessors at 22/22 lines, 17/17 regions, 6/6 functions and 8/8 branch outcomes.
WorkerUnixResources.cc reaches 43/53 lines (81.13%), 35/45 regions, 5/7 functions
and 24/28 branch outcomes; its base destructor adds one covered header line.
The ten uncovered lines/four branch outcomes are the real syscall wrappers. They
are not experimentally executed by these tests. This does not meet a blanket
infrastructure coverage target or prove installation/enforcement of resource caps.

## Remaining activation gates

Select and version a trusted build/runtime/profile launch contract before changing
the current helper's no-arguments interface. Check startup descriptor/runtime needs,
choose reviewed ordinary-workload budgets, install only inside the fresh helper
before request/engine work, and report setup failure without success publication.
Observe effective caps on an ordinary licensed workload separately. Kernel limit
installation failures and active enforcement remain unverified; no exhaustion,
allocator-failure, crash, signal-delivery or deliberately broken-pipe probes run.

Rlimits do not supply filesystem/network sandboxing, resident-memory/descendant
containment or parent-death handling. Active termination/reap acceptance, signed
trusted helper packaging, source-aware result contracts and Apple integration
remain open. #123/M8 and full parity remain incomplete. Production pin and all
GLB/writer/fault/animation/rig-pose quarantines remain unchanged.
