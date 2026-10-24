# ADR 0075: Parent resource launch and completion admission

Status: local development parent API; Apple and production adoption incomplete.
Refs #123. Extends the opt-in helper contract in ADR 0074.

## Decision

Add runResourceLimitedWorkerJob with an explicit ResourceLaunch and the existing
supervision/transport limits. Encode bounded v1 arguments before direct spawn;
reject unsupported virtual caps and diagnostic capture below the 1024-byte
resource-prefix bound before spawning. The argv storage remains owned until
posix_spawn returns. No shell, automatic retry or unrestricted fallback is used.
The existing no-argument parent operations keep their prior behavior. The parent
never calls setrlimit; only the fresh helper installs its own selected limits.

Reuse the existing supervised channel collector and terminal process observation.
Retired work still throws WorkerStopped and cannot become an admitted resource
completion. Work remains off the UI thread with immutable configuration and
exclusive child-reaping ownership. Active stop/escalation and exceptional cleanup
retain the unverified OS limits documented in ADR 0070.

After collection, admitResourceCompletion first requires observed normal exit zero,
then decodes the resource prefix against the requested profile. ResourceObservation
contains copied runtime/caps and a byte offset; the full original diagnostic bytes
and truncation flag remain owned by CollectedJob. The prefix is exactly ten fields:
marker, runtime, and four ordered soft/hard pairs (CPU/file/core/descriptor). It
must finish within the first 1024 bytes, use canonical unsigned decimal values,
match the requested runtime, preserve soft<=hard and never relax either requested
cap, including disabled core dumps. Stricter inherited values, including zero, are
retained; metadata admission does not establish startup viability or enforcement.
Opaque binary bytes after the first report line are not interpreted or discarded.

Colocate the observation codec with the v1 launch codec to share its canonical
integer grammar. The portable observation header exposes no OS types. A small
private WorkerBuild header shares translation-unit instrumentation detection
between helper and tests; consistent build flags and trusted binary identity remain
packaging preconditions, not guarantees provided by that detector.

ResourceJobError independently owns the collected exit/reply/diagnostic context
and optional original typed observation error. A nonzero/non-normal outcome wins
over a missing report so an original helper failure is not replaced by a secondary
metadata complaint. Metadata rejection surfaces the original cause in what() and
its typed accessor; consumers must safely surface original diagnostic bytes too.
Pre-collection OS/codec exceptions still use the existing typed transport errors;
partial-buffer reporting on those paths is not improved by this stage.

Resource admission is separate from publication. Callers must subsequently use
PendingReply or RequestQueue with authoritative current identity, validated frame
and payload semantics. A valid resource prefix never makes stale/invalid geometry
publishable. These APIs do not authenticate a helper, select production budgets,
change a pin, or adopt the worker in the Apple app.

## Evidence

Test-first compilation fails on the missing observation API and then the parent
resource API. Pure small-value contracts cover missing/incomplete/oversized reports,
unknown/version/runtime metadata, invalid/overflowing decimals, reversed and relaxed
caps, stricter zero inheritance and preservation of an opaque binary tail offset.
Synthetic completion data tests normal/non-normal/nonzero decision order, exact
owned reply/diagnostic bytes and truncation, original typed cause ownership, and
that a metadata-admitted one-byte reply still fails and retires PendingReply.
These are data-model decisions, not OS failure or signal events.

Actual parent transport cases use only licensed Capsule/Sword plus the existing
three-byte controlled invalid archive. All helpers finish normally: resource
reports pass, mesh floats/counts/warnings match the native API, and invalid-document
payload causes remain exact. Small chunk sizes exercise concurrent channel transfer.
The test process reads its own CPU/file/core/descriptor/VM limits before and after
all operations and requires exact equality. Unsupported profiles, undersized
capture and pre-start cancellation use a nonexistent executable path to establish
rejection before spawning, without starting or failing an actual child.

Final normal CMake/CTest passes 67/67 in 25.53 seconds. Final ASan/UBSan passes all
66 registered contracts in 104.69 seconds; the existing normal-only Swift consumer
is omitted by its build rule. Strict warnings and formatting pass. No allocation-
failure, resource-exhaustion, crash, active signal-delivery or deliberately broken-
pipe probe runs. No unsupported-runtime child is deliberately started through the
socket transport to induce an early input-channel failure.

A single instrumented harness runs the unchanged launch and observation unit sets:
112/112 lines, 120/120 regions, 10/10 functions and 77/77 branch outcomes in the
metadata source and error accessors. An initial attempt to merge separate unit
executables reported a main-counter hash mismatch and omitted one accessor from
its object mapping; that report is retained as superseded evidence. The single-
harness measurement resolves the mismatch and includes all accessors, without
changing assertions.

Scoped real ordinary transport plus synthetic/preflight instrumentation measures
WorkerTransport.cc at 211/249 lines (84.74%), 216/304 regions, 25/26 functions and
110/174 branch outcomes. Its header adds nine covered lines/functions and two
branch outcomes; WorkerBuild adds three covered lines and one function. This does
not meet the later blanket infrastructure coverage target. Active cancellation/
termination, resource enforcement, OS/clock/wait/poll failures and exceptional
cleanup remain unforced. Pure metadata coverage does not prove those OS paths.

## Remaining gates

Production profile/capability selection, authenticated signed helper packaging,
source-aware UV/material/rig/animation results and application scheduling/error/
publication integration remain required. Strong RSS/descendant/filesystem/network
sandboxing and parent-death containment are absent; virtual-address activation is
still rejected. Preserve all GLB/writer/fault/animation/rig-pose quarantines and the
Apple pin. #123/M8 and full native editor parity remain incomplete.

## Rebased delivery verification

Rebased onto saved launch stage 23f7dbb8, preserving the reviewed request-limit
preflight fix. Fresh normal CMake/CTest on a2fa3e7f passes 67/67 in 35.30s;
fresh full ASan/UBSan passes 66/66 in 103.62s. Changed-file formatting and the
source quarantine guard pass. These reruns use the ordinary licensed workloads,
pre-start rejection and synthetic outcome contracts described above, without
forbidden probes. CMake products are cleaned and sanitizer mode reset after
terminal gates. The original SVG bytes and modification time are restored.
No production pin, quarantine, helper packaging or Apple adoption change is made.
