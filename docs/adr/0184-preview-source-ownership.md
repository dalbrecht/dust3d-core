# ADR 0184: Immutable preview source ownership

Status: proposed for reviewed delivery. Tracks #270 and Apple #279/#280.

## Decision

Wrap the accepted metadata publication queue in a collecting-thread source coordinator.
Retain caller-selected immutable Sendable snapshots keyed by the queue's globally
increasing request IDs within its bound document/connection lifetime. The queue remains
the sole identity/scheduling authority; source ownership follows its active/waiting IDs.
Store at most the active source and newest waiting source, not a second metadata state.

Validate submission before replacing a source. Release a displaced waiting snapshot
immediately, but keep an accepted active snapshot through transport drain, even when
publication is cancelled/closed. Expose snapshots only for the active identity. After
matching complete/abandon, prune retired source ownership in defer even if admission
throws. Foreign completion and failed submissions preserve both active/waiting owners.
The coordinator performs no I/O, encoding, archive copy, task launch or native cancellation.
Its Source contract requires immutable independently owned Sendable values, never borrowed
native buffers. A consumer may retain its own snapshot across isolated work; map release
must not invalidate that consumer. Holding at most two map entries is not an RSS bound.

The healthy XPC fixture now captures Data archive, raw native controls, request/reply chunk
policies and cache allowance together before submission. It takes the active snapshot
from this coordinator for archive writing, RPC controls and reply decoding, then completes
only after channel cleanup and independent peer/EOF/current-identity admission. Data uses
Swift value semantics; no legacy whole-frame/envelope or duplicate wire body is created.

## Evidence

Missing source coordinator API produced a red compile before implementation. Pure fake
immutable reference snapshots and weak observers verify actual ARC release of displaced
waiting work, abandoned/stale/failed active work and retired waiting work; closed/cancelled
active sources remain usable until drain. Rejected replay/foreign submissions and foreign
completions do not free valid owners. Matching successful publication also releases map
ownership. A separate actor retains an immutable snapshot after coordinator abandonment,
then releases it independently. A Data value mutation test confirms snapshot isolation
and exact raw control preservation. Artificial tiny bytes/scalar maxima are pure values
only, never sent to a native parser or used for native resource-pressure probes.

Fresh full CMake build and230/230 CTest pass (35.50s). Actual SwiftPM runtime cached
build0.16s and a freshly linked strict Swift6 XPC host pass the existing licensed Capsule
session fixture with the current CMake service. Healthy legacy→preview→reset→legacy
cache reuse/rebuild, sandbox/signature/independent peer and normal PID disappearance
checks pass before owned fixture cleanup. Strict formatter/diff and11 frozen hashes pass.
Source remained frozen during validation and one shared build root was used.

Scoped source coordinator coverage:23/23 regions,15/15 functions,45/45 lines. Swift
reports no branch counters. This does not prove every private-state invariant or provide
native/service/whole-core coverage. No C++ parser/ownership code changed and no new
sanitizer claim is made. Native failures, cancellation, denied-peer, malformed input,
crash/fault/allocation/signal/broken-pipe/pressure/exhaustion paths were not newly probed.

## Remaining scope

This is internal prototype ownership infrastructure, not a public asynchronous SDK or
an Apple document actor. Callers must serialize coordinator access and create valid owned
snapshots. The healthy fixture source is immutable and serial; mutable-document producer,
asynchronous scheduling/backend adoption, public worker/SDK packaging and trust, full
native-range resource acceptance and baked/imported profiles remain open. Apple pin48b0ce2d,
active app routes and all quarantines remain unchanged. M4–M8/full parity is incomplete.
No protected app, OS container, global SVG or human button interaction was used.
