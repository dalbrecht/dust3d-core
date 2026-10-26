# ADR 0155: Connection-scoped worker cache lifecycle

Status: proposed for reviewed delivery. Tracks #251.

## Decision

A ConnectionGenerationSession exclusively owns one NativeGenerationSession for an
immutable host-selected document and connection generation. Construct one owner per
host connection/document; never select shared cache state from an incoming reply.
SessionScope validates nonzero scope before native owner construction. Host transport
must authenticate the connection and choose its lifetime; this class does neither.

The pure scope admits exact document/lifetime and strictly increasing nonzero
request IDs. Rejected scope attempts do not consume ordering state. An accepted
attempt consumes its ID even if subsequent policy validation or native work fails.
Revision remains unchanged: existing PendingServiceReply/current host identity
checks still govern publication. This wrapper does not replace queue-wide request
ordering across connection lifetimes or authenticate source bytes.

One mutex serializes admission, generation/copy/statistics, reset and close. Reset
clears retention without clearing replay protection. Close permanently retires
admission and destroys the native owner, after any currently serialized call; it is
idempotent and does not interrupt or cancel native generation. Caller must preserve
wrapper lifetime through calls; concurrent destruction is unsupported. Previously
returned owned geometry survives reset/close. A separate connection creates a fresh
native owner, including when cache retention is disabled with allowance zero.

## Evidence

Both test files began red with missing headers. Full CMake build and all 197 CTest
contracts pass (39.95 s). SwiftPM Dust3DWorkerRuntime builds (0.67 s), strict manifest
lint, quarantine guard and exact clang-format18 pass. Pure scope tests cover invalid
scope, wrong connection/document, zero/replayed/out-of-order IDs, rejection without
consumption, independent scopes, revision pass-through and idempotent close.

The existing licensed Capsule fixture verifies actual repeated cache reuse, reset
without request replay, regeneration after reset, close with prior output ownership,
closed-query rejection and a fresh cache-disabled owner. These are ordinary positive
native generations; no new malformed archive/fault/cancellation/pressure probes.

Scoped ASan/UBSan passes for pure scope and the owning wrapper/native session adapter.
Engine static libraries remain uninstrumented and leak detection is disabled. Pure
scope coverage is27/27 lines,28/28 regions,7/7 functions and14/14 branch outcomes.
No whole-engine or concurrent-destruction coverage claim follows from these checks.

## Remaining work

The host must adopt this owner from authenticated connection/document lifecycle,
serialize lifecycle callbacks and admit current request/revision publication. Wire
requests/results, C ownership and original statistics exception mapping, Swift/XPC
integration and real platform image decoding remain separate. This does not close
#251, Apple #279/#280, range parity or full M4–M8 acceptance. Production pin and all
quarantines remain unchanged. Completed exact-head review and every CI check are
required before merging.
