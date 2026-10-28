# ADR 0186: Public XPC connection adapter

Status: proposed for reviewed delivery. Tracks #270 and Apple #279/#280.

## Module and configuration

Expose Dust3DWorkerXPC as a static SwiftPM module depending on the pure client foundation
and system Foundation/Security, not the native runtime/engine or Qt. Extract the exact-
revision Objective-C descriptor RPC protocol/interface factory from QA support. No service
identifier, signing identity, development pin, private peer-file policy or service @main/
exit lifecycle is embedded in the module. The healthy QA host/service import this same
contract, and its client is now only a C-value-to-public-DTO facade.

Require explicit WorkerXPCConfiguration. Validate reverse-domain service identifiers and
nonempty signing requirements locally, then compile requirement syntax with the existing
Security pattern, retaining original OSStatus. Delegating an unchecked string to Foundation
would lose that typed boundary. System Security is not a new external package dependency.
Requirement compilation does not prove distribution-policy strength or authenticate a peer;
production worker packaging/trust distribution remain separate work.

## Connection, lifetime and original receipts

WorkerXPCConnection owns its Foundation channel on one actor. Plain Sendable identities
and passive option DTOs preserve original native/cache/wire/chunk controls, including raw
zero/default values. No legacy envelope/count/text cap is added. Async submit, session,
preview, reset and close methods return their original dedicated receipt domains. Legacy
and preview statuses are never coerced together; preview boundary classification remains
separate. Original NSError and diagnostic bytes remain owned and are not logged as commands.

Before resume, reject invalid request/channel/lifetime metadata. Freeze one generation,
allow one in-flight RPC, use checked monotonic internal tokens and reject overlap. Ignore
stale/duplicate callbacks without consuming newer work. Retain borrowed FileHandle objects
while control is pending. A lock-protected continuation helper resumes once outside its
lock; actor lifecycle state independently correlates callbacks.

Read PID independently from the actor-owned connection on matching completion and bind it
to the lifetime; do not trust a callback PID as that observation or silently reconnect to a
new process under an old generation. Preserve original callback PID separately. Enforce
only the original511-byte control diagnostic bound. Interruption/invalidation retires the
lifetime and fails pending control once. No automatic reconnect, deadline or native cancel
handler is installed. Task cancellation does not prematurely complete pending native work.

A callback/error is not actual channel EOF, native drain, semantic success, current-source
admission or process disappearance. Callers retain/drain/clean their channels and source
owners independently. Idle invalidate rejects a pending RPC and performs no force kill.
The existing healthy fixture continues legacy→segmented→reset→legacy on ONE connection,
with actual cache reuse/rebuild and original EOF/peer/current-source publication checks.

## Tests, coverage and provenance

Missing public contract/connection/configuration/state APIs produced red compile failures
before implementation. Pure state/fake channel tests cover correlation/overflow/generation,
invalid preflight, overlap and idle-close protection, duplicate and late callbacks, missing
proxy/PID, interruption/invalidation, observed PID change, original NSError domain/code/
cause, profile-specific receipt fields and oversized control diagnostics. All negative
cases use fakes/scalars only. Fixed valid requirement syntax is compiled; no malformed
Security requirement or denied real peer is probed. An initial direct swiftc XCTest command
lacked XCTest framework paths; correctly configured SwiftPM recorded the actual API red.

Eighteen public-package XCTest cases pass. Final normal CMake build and231/231 CTest pass
(42.27s), including every existing healthy XPC profile and the public SwiftPM test wrapper.
All three actual SwiftPM products build, and a freshly linked strict Swift6 host consumes
those archives and passes the licensed Capsule session fixture with normal service PID
retirement before owned bundle cleanup. Strict format/diff and22 frozen hashes pass; one
shared native-backend build/cache root is used. Actual transport archive symbol inspection
finds no native-worker or Qt dependency.

SDK fake-profile coverage is644/671 reported lines (95.98%),140/163 regions (85.89%) and
62/69 functions (89.86%). Connection file alone is552/563 lines (98.05%),96/112 regions
(85.71%),41/42 functions; continuation/request-state/contracts/value helpers are complete
under their scoped tests. Configuration is23/24 lines (95.83%). Swift emits no branch
counters. These are instrumentation counts, including async mappings, not whole native
coverage or a proof of every private-state invariant.

A separate instrumented Swift SDK/host profile runs only the existing healthy mesh and
session fixtures and covers the Foundation adapter6/6 functions and15/15 lines. Linked C++
engine and service remain uninstrumented. Only fresh profiles from that SAME host binary
were merged; do not sum/merge this profile with the distinct XCTest executable to invent
an aggregate. Real Security failure/nil-requirement paths, actor-disappearance callbacks
under violated ownership and actual transport/process failures remain unforced. No new
sanitizer or complete service failure-coverage claim is made.

The first actor build exposed sendability warnings from reading Foundation's non-Sendable
handler properties. Replace those getters with a narrow handler-installation method; the
refreshed focused SDK/host/service build is warning-free, without suppressions. A test's
await-in-XCTest-autoclosure compile error was corrected before any test ran.

## Remaining scope

Canonical production service executable/bundle, release signing/trust distribution,
asynchronous mutable SwiftUI producer/backend adoption, native-range resource acceptance
and baked/imported profiles remain open. QA IDs/private CDHash sidecars and service exit
behavior remain test infrastructure. No Apple pin or active app route changes; all
quarantines persist and M4–M8/full parity remain incomplete. No new malformed-native-input,
crash/fault/allocation-failure/signal/broken-pipe/native-cancellation/pressure/exhaustion or
denied-peer probes, app control, OS container or global SVG changes were made.
