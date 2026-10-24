# ADR 0093: Expose owned original OBJ bytes through C

Status: Local implementation; review and full M7 acceptance pending (#99).

Expose sized Dust3DObjExportOptions and an independent immutable opaque owner.
Generate under the shared engine lock with originalExport preparation, preserving
original double vertices and triangle/quad indices without float preview storage.
No import owner is supplied, so imported model assets reject before GLB decoding.
The API returns geometry-only v/f text; UVs, normals, materials, rig and animation
are not silently advertised. No filesystem operations or Apple pin changes occur.

Admit the output quota before generation (default64MiB, maximum256MiB), charge
owner and diagnostic capacity first, and pass the remaining budget to the OBJ
encoder. Charge the separately allocated document root and actual byte capacity
again at publication. Temporary generation/formatting storage and allocator
metadata are excluded. Borrowed bytes/diagnostic expire on destroy; C and Swift
callers must copy before releasing. C catches typed errors and exceptions,
preserves causes and maps bad_alloc to LIMIT_EXCEEDED. Cooperative cancellation
checks have no hard latency bound during inherited generation/encoding.

TDD began with a compiler-failing API contract before declarations/implementation.
Ordinary licensed Capsule contracts cover null accessors/destruction, required
sized options, quota admission, exact quota, one-byte-short rejection, independent
owner/copy lifetimes, archive diagnostics and owned cooperative cancellation.
Recovery uses a fresh Debug CMake build. After strengthening the lifetime
assertions to compare actual bytes across later successful/rejected exports and
after destroying the first owner, all 95/95 normal contracts pass (28.26s).
SwiftPM builds in 9.00s; a final recheck after the test-only change passes in
0.08s. Strict-warning syntax checks for the boundary and driver, formatting,
manifest/consumer lint and Qt boundary checks pass. Inherited warnings remain
recorded separately. A fresh full ASan/UBSan rebuild instruments engine, core,
remesher and triangle; 93/93 sanitizer contracts pass (115.97s). The two ordinary
macOS Swift consumers are excluded because swiftc does not link UBSan; system
zlib is not instrumented. Exact tested source is
2ff682b2a09479506a156b0d68968194339dd587; its tree, flags, driver hashes and logs
are retained with recovery bundles. No fault/crash/resource-exhaustion probes
ran. Whole-core coverage is not claimed; measured boundary coverage and an
actual Swift OBJ owned-copy consumer remain follow-up.

CLI OBJ dispatch, independent generated-output import and native editor adoption
remain open. ADR0091 already verifies the pure source-polygon encoder with an
independent consumer; that narrower evidence is not full API workflow acceptance.
All quarantines and production dependency pins remain unchanged.
