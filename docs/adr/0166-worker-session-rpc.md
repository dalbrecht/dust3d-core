# ADR 0166: Retained session RPC integration

Status: proposed for reviewed delivery. Tracks #251.

## Decision

Add explicit session-generation and session-close RPC methods to the development
XPC fixture. Scope, quotas and host-admitted document/request/revision travel as
independent typed metadata. The per-connection lazy coordinator binds scope and
serializes native work; the scheduler enforces one owner and monotonic requests.
The correlated C entry point compares the frame header before body retention.
Both received FileHandle owners must stay alive through queued execution: descriptor
numbers alone do not retain their underlying resources. Close is a scheduled job;
retirement cleanup remains ordered and idempotent.

Preserve bounded original diagnostic bytes and classification in control replies.
The host captures generation and observes NSXPCConnection.processIdentifier itself;
reported PID is not treated as authentication. Code-signing requirements and sandbox
entitlements are checked by the existing development fixture. Parent session output
admission requires matching queue identity, independent peer observation, actual EOF
and successful control flags. Geometry is copied before destroying the output root.

## Verification

The initial healthy Capsule test failed its receipt check with an empty reply file.
Code inspection identified received FileHandles not retained by the queued session
closure. Retaining the owners repairs the regression; scalar diagnostic metadata
makes future receipt mismatches actionable without logging document content.

The healthy fixture now passes two requests on one peer with actual positive cache
reuse, equal copied geometry, explicit close and copies valid after retirement.
Pure Swift value checks preserve original non-UTF8 cause bytes, signed codes and
truncation and classify scope errors without inducing native faults.
Full fresh CMake build and 210/210 ordinary CTest contracts pass (34.12s), including
existing healthy stateless XPC cases and the new positive session case. SwiftPM
runtime build passes (0.14s). Strict Swift6 concurrency/warnings-as-errors and Swift
format lint run during the build. No C++ source changed. No new fault, crash,
malformed-native-input, signal, cancellation or exhaustion scenario is added.

Coverage for the new RPC failure/control guards is partial; native error paths are
not deliberately induced, and this is not whole-core coverage or production app
acceptance. Full authored/procedural/source-baked rendering, Apple #279/#280 range
acceptance, application adoption and distribution remain open. Apple production
pin48b0ce2d and all quarantines remain unchanged. Completed clean exact-head automated
review and all CI checks remain required before merge.
