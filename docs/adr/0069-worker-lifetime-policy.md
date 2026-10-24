# ADR 0069: Pure worker lifetime decision policy

Status: local foundation, not connected to process execution. Refs #123.

## Context

The development Unix transport can wait indefinitely for generation, channel EOF
or blocking exception cleanup/reap. The request queue bounds pending archives and
retires obsolete publication but does not stop active generation. These are not
production containment or permission to adopt a new Apple core pin.

## Decision

Add an externally serialized, single-owner WorkerLifetime policy. It reads elapsed
monotonic milliseconds and observed cancellation, process-exit and channel-closure
state; it never reads a clock, spawns, signals, closes descriptors or reaps.
Required explicit budgets are work 1..600000ms, stop grace and post-exit drain each
1..10000ms. These are supported policy ranges, not production defaults.

At the exact work deadline, a process whose exit has not already been observed is
retired. Cancellation wins when observed simultaneously. One requestStop decision
is emitted, followed by one requestForceStop after the grace interval if exit is
still unobserved. Observed exit suppresses further stop actions. After exit, open
channels receive a separate drain interval; expiry emits one closeChannels decision
and finishes without admission permission. A completely collected exit wins a
simultaneous drain boundary. Cancellation during drain also retires publication.
The first retirement reason survives later drain expiry.

Elapsed time and sticky exit/channel state may not reverse; validation occurs
before mutation. Relative subtraction avoids unsigned deadline-addition overflow.
A terminal successful collection emits mayAdmit only once. This permits only
subsequent actual exit-code, frame, schema and authoritative revision checks in
PendingReply; it never authorizes publication by itself. Repeated terminal
observations emit no actions or admission permission. No automatic retry exists.

## Verification

Test-first compilation fails on the absent header. Pure ordinary small-value
contracts cover timely completion, open drains, exact work/grace/drain boundaries,
cancellation precedence and retirement, single-use actions/admission, preserved
original reason, invalid policy, reversed observations and atomic rejection, and
integer-boundary elapsed arithmetic. Synthetic observations do not deliver any
signal or force a process failure. Full normal CMake/CTest passes 62/62 in 33.05s.
Scoped fresh ASan/UBSan and -Wall/-Wextra/-Werror pass for the policy contract.
Focused instrumentation measures 52/52 lines, 67/67 regions, 4/4 functions and
52/52 branch outcomes for WorkerLifetime.cc and its error accessor. This is not
whole-core coverage or OS lifecycle evidence. One shared build root is reused.

## Remaining work

The current transport and its blocking cleanup are unchanged. Integrate decisions
with a parent-owned process operation, bounded polling/drain and actual outcome
collection; establish stop/escalation error semantics and parent-death handling.
Even a force-stop request cannot promise bounded OS reap in uninterruptible kernel
states. CPU/memory/file/descriptor limits, filesystem sandbox, trusted packaging,
complete result schemas and Apple integration remain required. No crash, signal-
delivery, broken-pipe, allocation-failure or resource-exhaustion probes run.
Preserve GLB #67/#69, writer #83, fault #84, animation #95 and Apple #199 quarantines
and the production core pin. Full M8 and editor parity remain incomplete.
