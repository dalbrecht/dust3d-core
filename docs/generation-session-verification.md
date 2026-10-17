# Bounded generation session verification

Core issue #23; ADR 0007. All additions retain MIT licensing. No Qt/Apple
frameworks, new package dependencies or CI configuration are introduced.

## Contracts

The public C ABI contract exercises the seven existing model fixtures through
cold loads, actual component reuse, edits and restores. It compares oriented
triangle position/normal/color records at 1e-5 quantization, independent of
triangle ordering, plus source counts and compatibility warnings. This is
cached-versus-fresh downstream equivalence, not an upstream Qt baseline.

Edits cover radius, movement, deformation, rounding, mirroring, component color,
smoothing, ignored saved dirty flags, linked part/node profiles, sibling reuse,
component order and grouping, inherited color, Skin/Cloth wrapping, openings,
binding and negative wrapping failures. Unsupported/failing edits must match
fresh primary statuses and clear partial caches. Successful empty source state
is retained without changing NO_GEOMETRY semantics.

Asset contracts compare exact content and unknown attributes, add/remove assets,
and preserve reuse across archive item order and model-size/offset changes.
Both preflight asset rejection and post-generation payload rejection return the
valid primary mesh. Zero disables retention. Short stats outputs stay untouched.
Output meshes remain valid after later loads, reset and session destruction;
source archive bytes remain unchanged.

Test-only ordinary C++ allocation failure injection exercises session creation,
primary snapshot allocation, optional asset item/attribute collection, and the
transient cache snapshot clone. Metadata/clone failures preserve valid primary
output, release retained state and report the original cache allocation cause.
The injection derives string allocation sizes from actual standard-library
capacity and asserts the targeted failure occurred. No production test hooks are
added. The harness routes ordinary throwing/nothrow new and matching deletes
through the same malloc/free family: libstdc++ stable_sort's nothrow temporary
allocation otherwise mismatches the test override's sized delete under Linux ASan.
Aligned/platform/C allocator failure paths are not comprehensively injected.

Concurrent legacy, same-session and independent-session callers execute real
Capsule/Sword generation with independent output owners. A C translation unit
constructs, loads, queries, resets and destroys the new opaque owner.

## Verification scope and limits

The full gate is CMake Debug build/CTest, CMake ASan+UBSan build/CTest, SwiftPM
build, new C/C++ and manifest formatting, whitespace and Qt-boundary checks.
There is no `make ci` target in this portable repository; these are the hosted
workflow's equivalent local commands. macOS leak detection is disabled because
its sanitizer runtime does not support it; hosted Linux retains leak detection.

The concurrent sanitizer test uses explicit 16 MiB POSIX worker stacks. On the
local Apple Clang Debug toolchain, disassembly shows the inherited
`tri_tri_intersection_test_3d` frame growing from 336 bytes unsanitized to over
3 MiB with ASan+UBSan. Default small worker stacks crash in that frame. The
harness preserves both the real Sword boolean workload and instrumentation;
production geometry and worker policy are unchanged.

A standalone instrumented GenerationSession.cc run measures 200/226 lines
(88.50%), 16/17 functions (94.12%) and 69/74 branches (93.24%). These figures
cover the session implementation only, using the expanded failure-injection
corpus; they do not measure all inherited geometry or the Apple app. Residual
uncovered lines are mostly defensive arbitrary-exception and mutex-failure
translation, plus an already-failed metadata collector short-circuit. The
standards' 95% adapter/100% domain targets are not claimed as achieved here;
the repository's documented coverage adoption gap remains.

A macOS Swift 6 strict-concurrency consumer compiles and runs against the SwiftPM
C module, including both imported budget constants, copied positions, actual cache
hits and reset/rebuild behavior. Run `tests/swift/verify-session.sh` from any
working directory; it builds into a temporary directory and removes only that
scratch directory on exit. This consumer caught C-cast macros that the Swift
importer rejected; integer literal constants preserve their numeric values in
both languages. The script is an explicit local gate, not yet a hosted CI step.

No UI operation or human interaction is required. Swift actor/SDK adoption,
obsolete preview scheduling, source provenance, memory/RSS benchmarks, TSan and
the agreed Qt differential corpus remain subsequent plan work. No binary ABI
stability, hard peak-memory bound or full M3 acceptance is established.
