# Dust3DWorkerXPC

macOS worker RPC contract and async connection adapter. Import the static SwiftPM product
Dust3DWorkerXPC with Dust3DWorkerClient. Dependencies are that pure client module and system
Foundation/Security only; no engine, Qt, signing identity, service identifier or development
pin is embedded. The library is not directly runnable and does not package a worker service.

## Connection and source ownership

Create WorkerXPCConfiguration with an explicit embedded service identifier and trusted
host-selected signing requirement. Security validates requirement syntax and retains its
original OS status on failure; this is not a distribution-policy or peer-verification claim.
Create WorkerXPCConnection from that configuration. submit/submitSession/submitPreview,
resetSession and closeSession are async throws and return original profile-specific scalar
receipts. FileHandles remain caller-owned. Passive options retain exact raw budgets; the
adapter neither encodes archives nor interprets native semantics.

One RPC is active per connection. Overlap and idle invalidation reject while pending;
connection generation and independently observed OS PID cannot silently change. Duplicate
callbacks cannot complete twice. Original remote errors and bounded diagnostic bytes retain
their domains. Interruption/invalidation retires the lifetime; no automatic reconnect,
native cancellation or process termination is performed. Task cancellation alone does not
interrupt work or finish a continuation before the service responds.

A callback/error is not proof of frame EOF, semantic success, native drain or process exit.
Retain sources/channels and independently drain/clean them, then complete/abandon the
client source coordinator. Invalidate the idle connection only after that cleanup. Build
worker/client from the same reviewed source revision; binary ABI stability is not claimed.

## Development and limits

CMake builds this same module and the existing healthy XPC fixture uses the adapter through
a C-value-only test facade. Public SwiftPM XCTest uses fake channels for errors, duplicate
callbacks, interruption, PID changes and diagnostic limits; those cases never probe native
workers. Use the existing shared build root/native SwiftPM backend with the CTest public
package wrapper. Canonical production service executable/bundle, distribution trust and
mutable SwiftUI backend integration remain open. QA IDs and private CDHash sidecars remain
outside this library. No Apple dependency pin or quarantine is changed here.

## Pre-job identity

Call `try await connection.handshake(generation:)` before creating a real executor's job
channels. Retain its immutable receipt separately: it preserves reported PID/sandbox/ownership
status, independent OS-observed PID and fixed host generation after later control failure.
The method rejects unmatched/missing PID, unverified sandbox or rejected ownership and retires
that lifetime. It does not consume job IDs or create a native session. Repetition is ownership
only, not native-idle/admission proof. Existing direct job methods remain compatible and do not
implicitly handshake. Connection invalidation and known PID do not establish native drain or
process disappearance; the descriptor executor/supervisor must observe those independently.
