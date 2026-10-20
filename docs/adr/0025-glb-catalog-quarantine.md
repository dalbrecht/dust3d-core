# ADR 0025: Quarantine the unfinished GLB material C API

Status: Proposed mitigation for core #69. The experimental API stays out of the
build. Re-enablement remains blocked in #67.

## Evidence

The catalog load reached `document.at("asset").at("version") != "2.0"`.
nlohmann JSON 3.1.2 treated every scalar, including `const char*`, as a
no-throw comparison operand. A string literal is a pointer, so the operator
constructed an owning JSON string inside `noexcept` and `std::bad_alloc`
terminated the process before the catalog handler could report it. Parser
ownership repairs do not cover that comparison. The scalar overloads now
exclude pointers, so the caller converts the literal and allocation failure
propagates. Object, array, and string parse results also commit their payload
before their type, so a failed allocation does not reach the no-throw
destructor with a null owner.

The unfinished catalog implementation and its fault-injection test stay out of
active sources. This change does not restore them or change the Apple production
pin. A separate comparison test injects allocation failure around the glTF
version literal and requires `std::bad_alloc` to propagate.

## Decision

CMake configuration fails closed if the public bridge header is missing, catalog
C declarations reappear, `native/Engine/GlbMaterialCatalog.cc` is restored, or
`Package.swift` / the root `CMakeLists.txt` names that file. A contract tests
harmless temporary source fixtures. This guards accidental restoration. The
allocation regression covers JSON parsing and string comparison.

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
