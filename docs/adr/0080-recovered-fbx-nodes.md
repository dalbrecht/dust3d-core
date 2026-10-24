# ADR 0080: Recover immutable bounded FBX node ownership

Status: Delivery validated locally; review and complete FBX export acceptance pending (#99).

Recover the saved move-only FBX7400 node and ordinary value contracts over the
reviewable property-block foundation. The private recovery checkpoint records
the exact archived-source identity. No old build manifest, binary, scene writer,
rig owner or animation evaluator is restored wholesale. Existing Qt/vendor
reference sources and all licenses remain unchanged.

Each node owns its name, property block and ordered child vector. Names contain
1...255 non-NUL bytes. Children are immutable owned values, establishing acyclic
ownership. Aggregate limits admit at most 65,536 nodes and depth 64. Zero payload
quota selects 64 MiB; the ceiling is 256 MiB. Account root/name/property buffers,
the child vector's entire retained capacity and descendant dynamic storage,
without double-counting child inline roots. Temporary inputs and allocator
bookkeeping are excluded; these are retained-payload limits, not process RSS.

Cache exact record bytes from the 13-byte FBX7400 header, name, properties and
child records, plus a 13-byte child-list terminator when children exist. A separate
checked wire budget supports later absolute-offset conversion. Construction
validates metadata before publication; typed DocumentError reports invalid names
or limit violations. Child/property arguments may be consumed on failure. Borrowed
values expire on move/destruction; moved-from nodes may only be reassigned or
destroyed. No file, scene, C ABI or global-engine operation is introduced.

The restored contract began red on the missing header. Ordinary small owned
fixtures verify exact record sizes, property/name ownership, moves, spare child
capacity, exact and tightened quotas, direct/descendant counts, names and bounded
depth. The 64-node chain exercises the supported depth contract; it does not
attempt stack/resource exhaustion. No allocator faults, invalid pointers or
quarantined XML-writer fixture are used. Defensive large wire-limit rejection
remains unforced and must be reported as a coverage gap.

Node metadata is an enabling M7 layer. Complete document offsets/header/footer,
actual scene/material construction, independent re-import and Apple adoption
remain open. Production pins, all quarantines and the completed-review/all-green
CI merge gate remain unchanged.

## Initial local verification

Source quarantine and formatting guards pass. Only the source-audited node
contract is newly registered. A clean Debug CMake rebuild and all 66 CTest
contracts pass (32.70 seconds). A fresh strict-warning C++17 ASan/UBSan build of
the node/property implementation and node contracts passes. The core SwiftPM
product builds successfully. Scoped node implementation coverage is 49/50 lines
(98%), 45/46 regions and 33/34 branch outcomes; the defensive wire-limit guard
remains unforced. These results do not establish whole-core coverage, valid scene
output or independent-consumer acceptance. No archived binary, unsafe fixture or
native evaluator was run.

## Delivery revalidation

Prepared on the separately tested property-delivery stage `b1a03c5f`, retaining
reviewed worker/writer contracts and the tested pure FBX prerequisites. Only the
source-audited immutable-node contract is newly registered. Fresh ordinary CMake
build and 72/72 CTest contracts passed (16.09 seconds). Fresh standalone
strict-warning C++17 ASan/UBSan node/property contracts and SwiftPM (0.71 seconds)
passed, along with source quarantine, formatting, manifest lint and Qt-boundary
checks. Earlier 66-case and coverage measurements are historical and were not
remeasured. The defensive large wire-limit guard remains unforced. No XML writer
fixture, allocator failure, resource exhaustion, evaluator or archived binary was
run. Prerequisite reviews and complete scene/re-import acceptance remain pending.
