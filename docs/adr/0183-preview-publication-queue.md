# ADR 0183: Preview publication queue

Status: proposed for reviewed delivery. Tracks #270 and Apple #279/#280.

## Decision

Add a collecting-thread metadata queue for the explicit segmented preview profile.
Retain one active publication gate and the newest waiting identity; never store a legacy
frame or copy an archive. The caller retains immutable source/context/policies keyed to
these identities and supplies independent authenticated transport observations.

The first accepted submission binds document and connection generation. Reject closed,
foreign-scope or non-increasing request IDs before modifying waiting/current state.
Return the replaced waiting identity so its caller-owned source can be released. Begin
freezes one active identity; subsequent submissions update current source and waiting
work without replacing active transport. Revision zero and UInt64 maximum IDs remain
valid; no increment arithmetic or incidental wire/count/text cap is introduced.

Cancellation retires matching publication and removes matching waiting metadata only.
Close retires publication/rejects new submissions while active transport still drains.
A foreign completion/abandon retains all work. After owned channel cleanup, matching
completion releases only active before checking current source, EOF, peer/lifetime and
original receipt through ADR0182's gate. Failed/stale work cannot consume newer waiting
metadata or resurrect an older source. Abandon releases failed transport while the caller
retains the original error. There is no native cancellation, process control or retry.

## Integration and evidence

The existing healthy Capsule XPC fixture owns one preview metadata queue for its peer.
It submits/begins the segmented request without the legacy frame queue, decodes under
captured policies through actual EOF, explicitly closes its channels, then completes the
queue before copied output publication. On failure, scoped channel cleanup occurs before
matching abandonment; the primary cause is preserved through the existing compound-result
helper. One authenticated peer still runs legacy→preview→reset→legacy with actual shared
cache reuse, geometry/source correspondence and rebuild after reset.

Missing queue API produced a red compile before implementation. Pure Swift cases cover
latest-waiting replacement, stale completion, foreign completion/abandon, original failed
receipt bytes, monotonic replay, active/waiting publication cancellation, no resurrection,
close with active work, document/generation mismatch without consuming IDs, and UInt64.max.
These simulate scalar scheduling only; no native cancellation/denied-peer/input probes.

Fresh full CMake build and229/229 CTest pass (57.29s). Actual SwiftPM runtime build32.37s
and a freshly linked strict Swift6 XPC host pass the healthy Capsule fixture with the
current CMake service. Sandbox/signature/independent peer checks and normal service PID
disappearance pass before owned bundle cleanup. Strict formatting/diff and9 frozen hashes
pass. Source remained frozen while validation ran; one shared build root was used.

Scoped pure queue coverage:34/34 regions,9/9 functions,49/49 lines; Swift emits no branch
counters. This is not native/service coverage. No C++ parser/ownership change or new
sanitizer claim. Actual transport-error, process-failure or native-cancellation paths are
not newly forced. Existing inherited C++ bit-field warnings remain outside this change.

## Remaining scope

This is an externally serialized queue, not an asynchronous production SDK, an archive
owner or an Apple document actor. The fixture source is immutable; mutable SwiftUI source
and waiting-job ownership/adoption still require their own integration with fake backends.
Public SDK/worker packaging, production trust distribution, backend adoption, high-resource
native-range acceptance and baked/imported profiles remain open. Apple pin48b0ce2d and
active app routes remain unchanged, all quarantines persist, and M4–M8/full parity remains
incomplete. No crash/fault/allocation-failure/signal/broken-pipe/native-cancellation/pressure
or exhaustion probes, protected app/OS container/global SVG changes were made.
