# ADR 0073: Private helper resource setup adapter

Status: library implemented; subsequent opt-in helper activation is recorded in
ADR 0074. The pre-activation evidence below remains historical; Apple adoption is
incomplete.
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

## Ordinary small-workload baseline (2026-10-23)

A fresh Debug, sanitizers-OFF helper built from clean source
`c2da52cadb75ad48ea8c4f34d762338a6e5ac421` was observed on macOS 27.0.1
(build 26A434), arm64. These are single runs, not percentile or worst-case bounds.
No resource adapter was activated and no limits were changed. Each response passed
the existing real-process verifier: normal exit, bounded empty diagnostic channel,
PendingReply admission, native source counts/warning and every geometry float bit.

| Licensed fixture | Archive bytes | Reply bytes | User CPU seconds | System CPU seconds | Wall seconds | Peak resident bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Capsule.ds3 | 1451 | 9704 | 0.010270 | 0.004958 | 0.030470084 | 6160384 |
| Sword.ds3 | 8297 | 38504 | 0.041900 | 0.003697 | 0.068886791 | 9568256 |

The parent collected the owned child's normal wait4 usage, not a cumulative usage
estimate for multiple children. The installed SDK getrusage manual specifies
ru_maxrss bytes on this macOS platform. Wall time includes process launch and
collection; it is not engine-only CPU time. The verifier's own later in-process
comparison is excluded from each helper's usage accounting.

The first /usr/bin/time -l measurement could not query kern.clockrate under the
sandbox and returned failure after the Capsule helper produced a reply. It is
retained as incomplete evidence, not used for the table or treated as a helper
crash. The subsequent ordinary runs used wait4 without changing system settings
or bypassing that query. No failed workload or crash was deliberately reproduced.

Worker binary SHA-256:
`30d6b77f5f3abb2aecf531c44921fd9458159279fbe765927f0b2c50111e9241`.
Fixture SHA-256 values are
`04095eded18e72a1c373673799fc68a6ec21048235c79086c8b18f450b485866`
(Capsule) and
`6553fcb97e9ba6113ea9681bfcd7a79dd4ba41f58c9d152e867ca4c8b05b5206`
(Sword). Raw measurements, observer script and build logs are retained with the
recovery checkpoint. Compiled products are cleaned after collection.

Two small fixtures do not characterize complex modifiers, imports, textures,
large documents, sanitizer overhead, release builds or other OS/architectures.
No production budgets or RSS enforcement claims are derived from these numbers.
The separate reviewed launch/profile and containment gates above remain required.

## Rebased setup verification

Rebased onto resource-policy stage 5f0a92be, preserving reviewed request limits
and supervised ownership. Setup/adapter/test sources remain byte-identical to
saved f92d5cb8. Fresh normal CMake/CTest on ecc396b2 passes 64/64 in 34.22s.
A fresh strict-warning ASan/UBSan standalone setup/Unix-adapter contract suite
passes using RecordingCalls exclusively; real syscall wrappers remain unexecuted
and unenforced by these tests. Formatting and the source quarantine guard pass.
This is scoped sanitizer evidence, not a fresh whole-engine sanitizer run.
Products are cleaned and the original SVG side effect restored after terminal
gates. No production activation, pin or quarantine change is made.

## Public delivery after resource policy merge

Reparented the three saved setup commits onto merged master adb4bd70 after core
PR127 completed clean exact-head review and all six CI checks. Fresh full normal
CMake/CTest passed64/64 (18.24s); strict-warning standalone ASan/UBSan setup/adapter
contracts passed using RecordingCalls exclusively. New implementation/test sources
are unchanged from the saved stage. Real getrlimit/setrlimit wrappers are compiled
but unexecuted by these contracts. No production activation, pin or quarantine
change is included. Earlier baseline/sanitizer evidence remains historical and is
not relabeled as this delivery gate.
