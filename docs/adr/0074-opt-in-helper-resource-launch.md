# ADR 0074: Versioned opt-in helper resource launch

Status: local development helper activation. Subsequent parent integration is
recorded in ADR 0075; the evidence below describes the CLI-only checkpoint.
Apple adoption remains incomplete.
Refs #123. Follows ADRs 0071–0073. Not a production resource profile or sandbox.

## Launch contract

The existing no-argument helper launch remains available to the development parent
transport. Add an explicit opt-in v1 argv contract, excluding the executable:

```text
--resource-profile-v1 ordinary|asan cpuSoft cpuHard fileSoft fileHard descriptorSoft descriptorHard [addressSoft addressHard]
```

CPU units are seconds, file/virtual units are bytes, descriptors are a descriptor-
number ceiling. Core dumps always receive zero soft/hard limits and cannot be
re-enabled through argv. No CPU/file/descriptor budget defaults are selected by
the implementation. ResourceLaunch encodes canonical unsigned decimal values;
the decoder accepts exactly eight or ten tokens, each at most 32 bytes, with no
embedded NUL, signs, whitespace, leading zeroes or integer overflow. Typed launch
errors and original resource-policy errors remain distinguishable.

The helper compares the runtime token with its compiled ASan state before any
resource installation. Unknown/version-mismatched/malformed launch metadata fails
with exit 64 before archive input. The portable codec can represent an optional
virtual-address budget, but this helper build rejects its activation before setup.
Reviewed platform/runtime policy and memory acceptance remain separate. An ASan
profile with a virtual cap is rejected by policy even before that helper gate.

The contract is selected by the trusted launcher, never by document contents.
An argv version is not binary authentication, capability negotiation or a signed
helper identity. The parent transport does not yet emit these arguments or admit
resource observations. Apple still uses its existing pinned engine in process.

## Fresh-helper setup and observation

On macOS/Linux, prepare existing I/O descriptors first. Copy only bounded launch
tokens, validate policy/runtime, query all requested inherited limits, and compute
the complete finite effective plan. Reject a descriptor ceiling below the highest
helper-owned output/error/sink descriptor plus one before applying any cap. That
floor preserves existing channels; it does not promise future runtime/engine
headroom or close inherited descriptors.

Apply only inside the fresh helper, before reading the archive frame. No parent,
shared engine, SwiftPM target or app calls setrlimit. Read back every installed
resource and require its finite soft/hard values to match the planned effective
values. An OS inspection/application error retains its original errno, resource
and operation in the bounded setup diagnostic and exits 74; policy/launch errors
exit 64. Partial setup is not rolled back or retried. Report-write failure exits
before archive work and preserves its original I/O cause when the channel allows.

After successful readback, emit one bounded, document-free stderr line:

```text
resource-profile-v1 ordinary cpuSoft cpuHard fileSoft fileHard 0 0 descriptorSoft descriptorHard
```

The runtime token is `asan` for an instrumented helper. Values are effective caps,
including stricter inherited values, not merely echoed argv. The usual framed
result remains on the separate output channel. Result consumers must check actual
exit state, resource metadata and normal frame/revision admission independently;
this line alone is not proof of confinement or valid geometry.

## Verification

Test-first compilation fails on the missing launch API. Pure contracts cover exact
encoding/decoding, optional maximum uint64 finite values, bounds, malformed and
overflowing decimals, runtime/version mismatches and preservation of policy errors.
The codec and error accessor measure 55/55 lines, 54/54 regions, 6/6 functions and
38/38 branch outcomes under focused strict-warning ASan/UBSan instrumentation.

Fresh ordinary CMake/CTest passes 66/66 in 26.60 seconds. Fresh ASan/UBSan passes
all 65 registered C/C++ contracts in 104.65 seconds; the normal-only Swift consumer
is excluded by its existing sanitizer build rule. Resource-process acceptance runs
only licensed Capsule and Sword with explicit test-only budgets: CPU 600/600
seconds, regular-file 16777216/16777216 bytes, core 0/0 and descriptors 128/128.
No virtual cap is requested. Both ordinary and instrumented helpers report their
matching runtime and effective caps, exit normally and pass every native geometry
float bit, count, warning and PendingReply result assertion. These small-workload
budgets are test settings, not production defaults or worst-case measurements.

Rejected version/runtime/zero-budget/invalid-number/virtual-cap metadata uses a
three-byte truncated frame as input. Exit 64 with empty reply and the original
bounded launch/policy cause proves rejection before archive handling; no rejected
budget is installed. The normal test observer's own CPU/file/core/descriptor/VM
limits are identical before and after the full child test suite. Setup recording
contracts also verify the descriptor floor rejects before any application call.

Scoped combined instrumentation from recording setup plus real opt-in and legacy
ordinary process contracts measures setup source/accessors at 24/24 lines,
20/20 regions, 6/6 functions and 10/10 branch outcomes. UnixResourceSystem source
reaches 51/53 lines (96.23%), 41/45 regions, 7/7 functions and 26/28 branch outcomes;
the remaining two lines are real syscall error returns, deliberately unforced.
WorkerMain.cc measures 140/185 lines (75.68%), 127/172 regions, 11/13 functions and
57/92 branch outcomes for this selected process corpus. It does not meet blanket
infrastructure coverage targets. Setup/readback mismatch, report/diagnostic I/O
failure, OS syscall failures, optional-memory activation and other unforced OS
paths remain explicit gaps. No crash, active signal delivery, resource exhaustion,
allocator failure or deliberately broken-pipe probe was used to raise coverage.

## Remaining gates

Require reviewed production budgets/capabilities, trusted helper packaging/build
identity, parent launch/report admission and ordinary cross-platform acceptance
before adoption. Actual exhaustion/CPU/file-limit termination behavior is not
experimentally verified. No RSS/descendant/filesystem/network sandbox or parent-
death containment is provided. Opt-in supervision's active termination/reap paths
remain unverified. These limits do not make previously quarantined inputs safe.
No production pin or GLB/writer/fault/animation/rig-pose quarantine changes occur;
#123/M8 and full native editor parity remain incomplete.

## Rebased delivery verification

Rebased onto reviewed-compatible saved setup f3c51e88; launch/worker/process-test
sources remain byte-identical to saved 38baea83. Fresh normal CMake/CTest on
5ce1e932 passes 66/66 in 34.02s. Fresh full ASan/UBSan CTest passes 65/65 in 109.14s. Source
quarantine and changed-file formatting checks pass. Only ordinary licensed
workloads and pre-installation metadata rejections ran, with no virtual cap.
No active limit exhaustion, crash, signal or allocation-failure probes were added.

## Public delivery after setup merge

Reparented the saved launch stage onto merged core PR128 (f2d52986), verifying
that its complete tree equals the locally validated cd197de4 before this evidence
note. Fresh normal CMake/CTest passed66/66 (21.04s), including ordinary Capsule/
Sword child-only setup/readback and full geometry reply checks. Fresh strict-
warning standalone ASan/UBSan launch-codec and recording setup/Unix-adapter
contracts passed. This is scoped sanitizer evidence, not a new whole-engine
sanitizer run. Historical broad sanitizer/coverage measurements above remain
labeled as earlier evidence. No resource-exhaustion, signal-delivery, allocation-
failure, crash or broken-pipe probes ran. Apple pin and all quarantines remain
unchanged; parent admission and production containment still require review.
