# ADR 0071: Helper-only resource policy and acceptance gates

Status: proposed implementation contract, not an implemented containment boundary.
Refs #123. Follows ADRs 0069 and 0070; all existing quarantines remain in force.

## Problem

The current helper bounds framed input/output and the parent can retire results.
Neither limits the helper's CPU, memory, open descriptors or filesystem access.
The opt-in supervised transport owns only its direct child and does not guarantee
bounded kernel spawn/reap. Its ordinary tests do not verify active termination.
Apple still uses its existing pinned engine in process. These are separate gaps;
implementing an rlimit adapter alone cannot close #123 or M8.

## Decision

Add resource setup only to the fresh standalone helper, before reading its request
header, allocating the archive payload or invoking the engine. Never call
setrlimit in the parent, Apple process, shared engine library or test runner.
Do not enable a production profile or update the Apple pin in this stage.

Use an immutable, explicitly supplied resource profile. Keep profile validation
and conversion to a platform-independent list of requested limits pure. Put the
Unix getrlimit/setrlimit calls behind a thin helper-only adapter. Keep portable
headers free of OS constants, rlim_t and Apple types. Do not add arbitrary defaults
for CPU, memory or output size without ordinary-workload measurements and review.

A profile must distinguish these units and meanings:

| Resource | Requested unit / meaning | Required qualification |
| --- | --- | --- |
| CPU | Whole process CPU seconds, soft and hard limits | Not wall-clock work time; supplements ADR 0070 |
| File size | Bytes per regular file written | Not aggregate disk use or framed pipe output |
| Core dumps | Explicit zero-byte maximum | Prevents newly written core files; not a crash test |
| Descriptors | Count of open descriptor slots | Must account for inherited and helper-owned channels |
| Address space | Bytes of virtual address space, only if explicitly supported | Not resident memory; incompatible profiles must fail before archive work |

Reject unsupported resources, zero values where the resource requires a positive
budget, soft values above hard values, and values not representable by the target
rlim_t. Zero is meaningful for disabling core dumps. Do not encode infinity using
an ordinary byte/count value, silently clamp a requested cap, or raise an inherited
hard limit. Effective soft/hard limits must be the stricter of the requested and
inherited limits, with explicit handling of the platform's infinity sentinel.
Record the effective profile, not just the requested one.

Validation must complete before the first mutation. Applying multiple OS limits
is not transactional: if a later call fails, the helper reports the failed
resource, operation and original errno, then exits without decoding or producing
a success frame. Do not attempt to restore or retry partially installed limits.
A setup diagnostic is bounded and contains no archive paths or document data.
The parent preserves its actual exit status and diagnostic as a setup failure;
EOF before a reply must never become an empty successful mesh.

## Descriptor and startup ordering

Audit actual inherited descriptors and loader/runtime needs before selecting a
count cap. The current helper duplicates stdout/stderr and opens /dev/null before
reading input; those slots must remain available. A count limit does not close
already inherited descriptors or deny network/file operations. Establish a
separate close-on-exec/inheritance contract rather than treating the cap as a
sandbox. Keep sanitizer diagnostic channels available in instrumented profiles.

Trusted build configuration selects the resource profile and reports its identity
with the helper build identity. The untrusted archive/frame cannot increase or
replace it. Define and version the launch/profile contract before changing the
current no-arguments helper interface. The parent must reject an incompatible
helper/profile pair rather than silently reverting to an unrestricted worker.

## Memory and sanitizer compatibility

[Clang's ASan limitations](https://clang.llvm.org/docs/AddressSanitizer.html#limitations)
document very large virtual address mappings on 64-bit systems. A small virtual
address cap can prevent the instrumented runtime from functioning even for a
small input. Keep explicit instrumented and production profile compatibility;
never silently turn off ASan, weaken a selected production profile, or apply a
limit in the test runner to make a test pass. An instrumented profile without a
virtual cap cannot serve as evidence of production memory containment.

The [Linux getrlimit manual](https://man7.org/linux/man-pages/man2/getrlimit.2.html)
describes RLIMIT_AS as a virtual address limit and the historical, narrow effect
of RLIMIT_RSS. Do not advertise RLIMIT_RSS as a modern Linux hard resident-memory
limit. The [published XNU resource implementation](https://github.com/apple-oss-distributions/xnu/blob/main/bsd/kern/kern_resource.c)
shows a VM size validator for RLIMIT_AS; a moving source branch does not establish
behavior on the installed macOS kernel. Record supported OS/build evidence before
relying on that behavior. Strong resident-memory and descendant containment need
a separately reviewed platform mechanism. No such mechanism is selected here.

## Implementation and evidence sequence

1. Write failing pure contracts for policy units, unsupported capability,
   representability, infinity handling, inherited stricter limits, and validation
   before mutation. Use small synthetic values only; these tests make no OS calls.
2. Test the helper-only adapter with a recording OS interface. Prove requested
   call order, original-error preservation, no calls after failure, and absence of
   document work until setup completes. Simulated setup errors are API error
   values, not allocator, crash or resource-exhaustion injection.
3. Add an explicit trusted launch/profile compatibility contract. Test its
   malformed/version mismatch cases using small metadata, before spawning.
4. Run a fresh source-audited helper on ordinary licensed small fixtures with a
   reviewed compatible profile. Query/report effective limits inside that helper;
   never change limits in the app or test process. Preserve build/OS/profile and
   exact source provenance. Ordinary success is not proof of limit enforcement.
5. Review remaining platform acceptance separately. No allocation-failure,
   resource-exhaustion, crash, signal-delivery or broken-pipe probes are authorized
   by this ADR. Do not fabricate complete coverage for OS error/termination paths.

A reviewed resource adapter still leaves filesystem/network sandboxing,
parent-death behavior, descendant ownership, active termination acceptance,
trusted signed-helper packaging, source-aware geometry/UV/material/rig/animation
results and Apple integration unresolved. Maintain explicit unsupported behavior;
do not remove GLB, writer, fault, animation or rig-pose quarantines.
