# ADR 0089: Expose owned static FBX bytes through C

Status: Local source-matched prototype; review, Apple adoption and full M7 acceptance pending (#99).

Add Dust3DFbxExport with sized options and two explicit modes: geometry-only or
one procedural baked atlas. Generate and encode under the existing engine mutex.
The archive, options, decoded pixel views and cancellation token remain borrowed
for this synchronous call only. Release preview/atlas workspace before PNG/scene
encoding. Only final immutable FBX bytes and the parser diagnostic survive.

readDocument receives no ImportedModels owner, so embedded model assets reject
before any GLB decoder. No quarantined rig/animation/GLB writer, source-aware
material group or production pin is enabled. Geometry mode deliberately excludes
materials/rig/animation; baked mode requires complete packed UVs and caller-
resolved procedural images. These static modes are not full document export.

Expose load/destroy, borrowed bytes/count, retained payload and warning accessors.
NULL handles have empty values and destroy accepts NULL. Swift must copy bytes
and diagnostics before destruction; C++ owners, STL and exceptions stay private.
This is a source-matched prototype with no binary ABI stability claim.

Validate option size/mode and geometry/PNG/output bounds before generation.
Defaults are64MiB each; geometry/output maxima256MiB and PNG capacity max64MiB.
Final output includes handle, complete byte-buffer capacity and conservative
warning storage. Account known root/warning overhead before generation, adjust
the document limit and recheck actual retained capacities before publication.
Exact final quota is accepted; one byte below it rejects. Budgets are independent,
not a total-operation RSS/time bound; generator, scene nodes, codec workspace,
source storage and allocator bookkeeping remain outside them.

One outer cancellation token governs pre-lock, generation phase/corner boundaries,
post-bake/material/scene checks. Nested bake token is NULL or the same token.
Tokens stay alive throughout the call. The generator, PNG compression and scene
encoder do not provide bounded cancellation latency inside every phase. No OS
signals, termination, allocation-failure or exhausted-resource probes are added.

Document/UV/bake errors retain their status and cause; bad_alloc maps to a typed
limit and other standard exceptions retain their original what() message.
A non-standard exception receives an explicit fallback diagnostic because C
cannot represent an arbitrary exception object. Diagnostics remain bounded by
the existing C error buffer. No partial owner/bytes escape on any error.

## Verification and limits

The missing C API began red. New/changed registered sources were inspected and
the quarantine/format guards pass. Recovery uses a fresh Debug build and89/89
normal CTest contracts pass (36.97s). Pure-C consumer coverage now compiles/links/calls the new
surface. C++ contracts cover both modes, option/nested-size/decoded-span errors,
quotas, exact/one-below output budget, independent owner lifetime across another
export, copied bytes after destruction, original archive rejection, optional
NULL diagnostics, matching/conflicting tokens and pre-cancelled calls.

The new native Swift consumer compiles with Swift6 complete strict concurrency
and warnings as errors. It imports the actual C types, exports from a detached
task, copies Data/diagnostics before destroying the handle, survives another
export and transfers the owned Sendable value to an actor. It links the fresh
CMake archives and system zlib; it is not an Apple app dependency-pin change.

Recovery uses a fresh full ASan/UBSan rebuild of engine, core, remesher and
triangle targets; all87/87 sanitizer contracts pass (111.93s). The two ordinary
macOS Swift consumers are excluded because swiftc does not link UBSan. System
zlib is not instrumented. SwiftPM builds in6.92s; inherited compiler warnings
remain recorded separately. Qt boundary, manifest and Swift consumer lint pass.
The tested recovery source is2f4ff005a941242089c060219ac653ae3a085d7c; exact revision/tree, binary hashes,
flags and logs are retained in the recovery checkpoint.

Historical scoped C-boundary coverage was93/112 lines (83.04%),80/104 regions
and46/62 branch outcomes; it was not remeasured during recovery. Unforced residuals include UV/bake exception handlers, bad_alloc,
standard/non-standard exception fallbacks, the final ownership-capacity recheck
and impossible empty-owner defense. This is below adoption goals and is not
whole-core coverage. Pure error translation and broader ordinary failure cases
remain follow-up work; never use quarantined/fault probes to inflate coverage.

Earlier independent import evidence in ADR0088 proves one generated four-map
scene's corners/UVs/material bindings/pixels. These C-boundary and Swift tests
prove ownership/interop, not a second independent visual acceptance. Broader
normal/AO/alpha/source-aware materials and consumers remain required.

## Remaining delivery

This stage still prepares a float preview before retaining original double
geometry. It therefore inherits preview admission restrictions despite encoding
original doubles. A separate generation-purpose stage must decouple export from
preview preparation before general export acceptance.

No destination writer, native app export command or production dependency update
is included. Atomic filesystem publication, progress/cancellation UX, aggregate
operation supervision, reviewed dependency adoption, imported policy groups,
rig/animation/WAV exports and complete M7 acceptance remain open. The original
full native editor plan remains active and incomplete.
