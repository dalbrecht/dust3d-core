# ADR 0025: Quarantine the unfinished GLB material C API

Status: Proposed mitigation for core #69. The experimental API stays out of the
build. Re-enablement remains blocked in #67.

## Evidence

The unfinished material-catalog C API, exercised after the narrow JSON parser
ownership fix, still terminated with an uncaught `std::bad_alloc`. The remaining
cause is unconfirmed. The parser transitions repaired for that fix do not
establish safety of the catalog load path. An outer exception handler cannot
contain termination when another allocation fails during unwinding.

The implementation and its fault-injection test stay out of active sources.
This change does not restore them, rerun allocation-failure probes, or change
the Apple production pin.

## Decision

CMake configuration fails closed if the public bridge header is missing, catalog
C declarations reappear, `native/Engine/GlbMaterialCatalog.cc` is restored, or
`Package.swift` / the root `CMakeLists.txt` names that file. A contract tests
harmless temporary source fixtures. This guards accidental restoration. It is
not a sandbox or a proof that the parser survives allocation failure.

Run the same check with `cmake -P cmake/CheckGlbCatalogQuarantine.cmake` before
the SwiftPM source build. SwiftPM does not execute CMake, so the Apple package
scans the checkout it is about to compile. The reviewed pin remains a separate
boundary. No new dependencies or assertion suppression.

## Future isolated admission

Before re-enablement, implement a separate headless worker with a versioned,
length-bounded request/response protocol. Only immutable input copies and
validated result data may cross the boundary, never pointers or opaque owners.
The parent retains document/save ownership and admits results only after full
response validation against the initiating document revision.

Bound input bytes, decoded geometry, image payloads, output bytes, queue depth
and elapsed time independently. Cancellation, timeout, abnormal exit and
incomplete responses must return typed errors preserving the original diagnostic
when available. Discard partial results, preserve the document and do not retry
a crashing input automatically. Diagnostics must not embed document contents.
OS-specific process/resource enforcement belongs in platform infrastructure;
protocol and result validation remain portable.

The worker is planned, not implemented. Threads, exception handlers, quotas or
process launch alone do not establish containment. Re-enablement requires a
separate reviewed change, ordinary-input protocol and termination-handling tests
without intentional parser crashes, all required CI and explicit risk acceptance.
Remove the guard explicitly in that review. Do not bypass it or advance the pin
in this mitigation.
