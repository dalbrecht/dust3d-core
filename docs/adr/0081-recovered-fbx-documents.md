# ADR 0081: Recover bounded owned FBX container encoding

Status: Delivery validated locally; review and complete scene/export acceptance pending (#99).

Recover the saved document encoder over the audited immutable node/property
layers. The private checkpoint records the exact source provenance. Preserve the
existing MIT footer attribution in the new source. No rig/animation evaluator,
GLB path, public C ABI, destination-file writer or production pin is introduced.

Preflight a nonempty immutable node forest against 65,536 aggregate nodes and an
owned-buffer budget: zero selects 64 MiB; the ceiling is 256 MiB including root
and actual vector capacity. Inputs, temporary work and allocator overhead are
excluded; no process RSS claim is made. Immutable node construction bounds depth
to 64 and names to 255 bytes. Check aggregate sizes before allocating the complete
output buffer. Emit the FBX7400 magic/version, absolute record ends, property/name
bytes, child/root terminators and existing reference footer. Keep a checked
writer cursor and verify record/final sizes before publishing an independent
move-only owner. Inputs remain unchanged on success or failure.

Add the existing MIT vendor FBX codec and miniz implementation as test-only
targets. Products and SwiftPM do not link those targets. Alternatives considered
were a new external importer SDK or a second custom decoder; this stage uses the
existing structural oracle, while independent 3D importer acceptance remains
required later. No package is downloaded or newly vendored. Reference code and
licenses remain untouched.

The reference contract first requires small native output to equal reference
writer bytes for all 13 property types, typed NUL names, signed zero, arrays,
children/siblings and footer. Only that byte-equal known-valid fixture reaches
the inherited reader. The test owns its temporary directory under the shared
build root and enables input-stream exceptions so an unexpected I/O failure
cannot leave the reader continuing after failed extraction. No malformed stream,
invalid pointer, allocation fault, resource exhaustion or crash probe is used.
This is not a general safety claim for that inherited reader.

Current-source tests began red on the missing header. The native contract checks
exact header/offset/terminator/footer bytes, ownership after input destruction and
moves, small tightened quotas/counts, supported depth and name boundaries.
Defensive writer-consistency and allocator-capacity branches remain unforced;
do not corrupt immutable owners or inject faults to cover them.

Container agreement is not actual geometry/material/bind/animation re-import.
Scene construction, independent consumers, caught ownership adapters and Apple
UI/CLI adoption remain required for full M7. All quarantines and review/CI gates
stay in force.

## Initial local verification

Source quarantine and formatting guards pass. The two newly registered targets
are the audited native document and known-positive reference contracts. A clean
Debug CMake rebuild and all 68 CTest contracts pass (32.96 seconds). The reference
comparison and reader round trip pass, with no malformed data supplied. Worker
link inspection confirms it excludes the reference library; the updated core
SwiftPM product also builds. New reference-test source passes a strict-warning
C++17 compilation.

A fresh scoped strict-warning ASan/UBSan build of the native property/node/document
implementation and native document contracts passes. This does not instrument or
establish safety for the inherited reference codec or whole core. Scoped document
coverage is 72/76 lines (94.74%), 47/51 regions and 32/36 branch outcomes. Writer
buffer-bound, record-size, final-size and allocator-capacity guards remain unforced.
This falls short of the eventual core coverage adoption goal; retain the guards
and report the gap without corrupting owners or injecting faults. No archived
executable, quarantined fixture or evaluator was run.

## Delivery revalidation

Prepared on the separately tested node-delivery stage `bf04a46a`, retaining the
reviewed worker/writer contracts and tested FBX prerequisites. Only the native
container and known-positive reference contracts are newly registered. The fresh
ordinary CMake build and 74/74 CTest contracts passed (16.45 seconds). Fresh
standalone strict-warning C++17 ASan/UBSan native container contracts passed;
this scoped run does not instrument the inherited reference codec. The reference
test source compiled with strict warnings, and worker link-command/symbol checks
exclude the test-only codec. SwiftPM built (1.16 seconds); manifest lint,
formatting, source quarantine and Qt boundary passed.

Earlier 68-case and coverage measurements are historical and were not remeasured.
Defensive consistency/capacity guards remain unforced. The inherited reader saw
only the small byte-equal known-positive reference fixture, not malformed or
compressed inputs. Prerequisite review, scene construction and independent 3D
consumer acceptance remain required; no production pin or quarantine changed.
