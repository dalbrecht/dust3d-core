# ADR 0177: Native-budget indexed/source pair copy

Status: proposed for reviewed delivery. Tracks #270 and Apple #279/#280.

## Decision

Add opt-in `copyNativePreviewIndexedSourcedMesh` beside the accepted ordinary preview
copy. Borrow two live immutable owners: a source mesh and an indexed mesh derived from
that exact source geometry. Their same-generation relationship remains the caller's
contract; dimensional agreement cannot authenticate provenance. Keep legacy APIs,
copy/transport limits, opcodes, active routes and the Apple dependency unchanged.

Check captured archive/native controls before native getters. Check stable storage,
source/geometry dimensions, archive-derived membership and native record charges before
retaining arrays. Use the shared record planner with zero observed text to establish
the native base charge. Measure identity strings against the remaining source budget,
decrementing it for each actual node/part byte. Validate each identity's original UTF8
and then recheck the complete record plan with actual text. No incidental1024-byte
identity or8MiB text default narrows this opt-in native policy.

Validate every original influence and corner through the shared pure semantic rules,
including sentinel indices, finite nonnegative weights and kind correspondence. Then
call the ordinary preview copy for full geometry preflight, and copy owned identities,
corners and influences. No whole encoded mesh/pair buffer is constructed for validation.
Unknown kinds, weights, aliases and record order are preserved without normalization.

Identity C strings are trusted immutable native storage. Maximum scanning is bounded by
the remaining source policy (default64MiB, configured at most256MiB); the native budget
also keeps lengths below the existing UInt32 wire fields. Subtraction follows checked
base charges; remaining text accounting prevents arithmetic overflow. Native indexed
zero/default256MiB and explicit addressable policies remain independent.

The copy owns no native handles and does not generate geometry, activate dispatch,
authenticate peers or define an RSS bound. Native owners, capacities and generator/index
workspace remain outside worker logical accounting. Full-budget session generation,
output channels, C/Swift admission and app adoption remain open; #270 is not complete.

## Evidence and residuals

Missing-API red compile preceded implementation. The existing small licensed Capsule
contract now compares canonical geometry/source bytes against the established copy,
checks exact independent native budgets, one-byte-smaller text rejection and base-source
record rejection, maximum policies with only tiny actual geometry, captured membership,
missing owners, and all copied data after native/archive release. No new native fixture,
large allocation, malformed-native-input or resource-exhaustion probe is used.

Fresh full build and221/221 CTest pass (34.95s). Actual SwiftPM runtime builds (2.43s),
and the updated contract links/runs against that actual product. Both C++ formatters,
strict package formatting, quarantine guard and twelve frozen source/fixture/build
hashes pass. Scoped ASan/UBSan instruments the new copy file and existing contract;
linked engine/other libraries are uninstrumented, leak detection off. This is not
whole-engine sanitizer or full-range runtime/performance acceptance.

New file64/67 lines (95.52%),62/66 regions (93.94%),6/6 functions and37/46 branch
outcomes (80.43%). Residuals include missing storage/dimension guards inside trusted
opaque owners, null identity storage, the SIZE_MAX sentinel conversion and alternative
short-circuit paths. Existing pure protocol validators cover opaque kinds, sentinels,
weights and association failures independently; they do not prove native boundary
coverage. Do not forge owners, add malformed native inputs or inject faults to increase
these numbers. Larger licensed native compatibility coverage remains a release gate.

Apple pin48b0ce2d and core67/69GLB,83writer,84fault,95animation and Apple199rig-pose
quarantines remain unchanged. No crash/allocation-failure/signal/broken-pipe/cancellation,
pressure or exhaustion tests, app control or protected process/container changes.
