# ADR 0187: Reusable XPC service library

## Decision

Expose `Dust3DWorkerService` as a separate static SwiftPM product. Move admission,
ownership, FIFO scheduling, session lifetime, native profile mapping and XPC handler
implementation from test sources into one canonical `native/WorkerService` directory.
The client product never depends on this native-linked service product. The service
imports the existing shared protocol and canonical Apple image-decoder product.
CMake fixtures compile these same sources, rather than maintaining a second adapter.

`WorkerServiceConfiguration` requires an explicit host signing requirement and validates
its syntax. `WorkerServiceListenerDelegate` verifies its own App Sandbox entitlement
before accepting connections or native work, applies the requirement before invocation,
and retains the existing one-owner/one-request scheduling and normal-drain behavior.
The library returns original retirement errors through an injected callback; it never
terminates the embedding process. Only the thin QA launcher reads private development
CDHash pins and chooses process termination. No trust default or development identifier
is added to the reusable library. Imported image/GLB rendering remains disabled.

## Validation

The missing module produced the intended API red after an initial sandbox module-cache
setup failure (not a test result). Two configuration tests preserve a fixed valid zero
CDHash expression and reject empty input before Security compilation. Two fake-only
package lifecycle tests verify accepted-work/cleanup ordering, original cleanup-error
propagation, sealed admission and unclaimed-candidate behavior. Existing fake lifecycle
and scheduling contracts continue to compile against the moved implementation.

All 22 public package XCTest cases pass. Final ordinary CMake build and 231/231 CTest
pass in 37.84 seconds, including all existing healthy XPC modes. A freshly linked QA
launcher using the actual SwiftPM service archive passes the licensed Capsule session
cache/reuse/reset/close fixture, independently observed peer identity, EOF and normal
service disappearance (PID50868) before owned bundle cleanup. Strict Swift 6 compilation
and formatting pass. A test-only trailing-closure lint finding was corrected before
the final complete gate. One shared build/cache root is used throughout.

No new native parser/ownership code or fault probes were introduced. No new sanitizer
or whole-service coverage percentage is claimed; the moved implementation retains its
existing scoped evidence, and real Security/preflight/transport failures are unforced.
Negative lifecycle cases use fake executors and scalar errors only. No denied peer,
crash, allocation, signal, broken-pipe, native-cancellation or exhaustion probes run.

## Remaining work

This library is not a packaged production service executable. A thin executable,
sealed bundle configuration, signing/Team ID trust policy, embedding/distribution and
async mutable Apple backend remain open. Configuration syntax validation is not release
authentication evidence. Apple pin48b0ce2d, active routes and all feature quarantines
remain unchanged. M4–M8 acceptance and full parity remain unproven. Tracks #270.
