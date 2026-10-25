# 0132: C service request coordinator

Issue: #183. Status: local implementation; serial public review pending.

## Decision

Expose the existing RequestQueue through an externally serialized opaque C owner,
not a second scheduling state machine. It retains one active outbound frame and
framing collector plus one latest waiting request. Submission validates/copies an
exact frame before changing waiting state. A nonzero trusted service generation
is captured at begin, before transport submission. Busy begin preserves that
lifetime. Closed submission rejects before copying input.

The caller borrows active bytes and the collector for transport/framing only.
Those views survive submit/cancel/close and expire on admission, abandonment or
owner destruction. Never destroy the borrowed collector or use standalone
semantic decoding. Both standalone service and child semantic gates reject these
collectors. Only coordinator admission supplies its private captured identity and
its current queue source to the existing service gate. Caller receipt values
cannot substitute the current source. Raw operation values validate before enum
conversion. Common receipt/policy conversion is private C++ implementation.

A required result pointer makes every consumed attempt observable. NULL result
leaves the attempt untouched. Matching admission releases the slot even after
stale, retired, protocol or codec rejection; latest waiting work survives. On
framing/transport failure the caller preserves the original cause, cleans up
transport and abandons matching work. Foreign abandonment preserves active work.
Cancellation and close retire publication, never interrupt native execution.
Independent decoded owners survive coordinator destruction. All C++ exceptions
are caught, with bounded original typed causes. Quotas bound logical retained
bytes, not native CPU, RSS, framework buffers or descriptor blocking.

CMake and SwiftPM runtime expose the same implementation; runtime source count
is17, disjoint from the engine target. Actual XPC host/editor adoption, geometry-
equivalent document revisions, multi-document fairness, release trust, resource
policy and reviewed production pin adoption remain open. A single latest-waiting
queue is not full multi-document acceptance.

## Verification and recovery provenance

Temporary delivery checkouts were found missing. Their cause is unknown. The
saved core182 source bundle chain restored50939b56. Uncommitted183 source was
reconstructed, not recovered verbatim. A new C11 regression first failed on the
missing API; all evidence below belongs to fresh reconstructed source. Prior
results from disappeared source are not used as proof. Source copies, tracked
patch and fresh evidence are retained outside the temporary checkout.

Pure C/C++ consumers cover copied request ownership, generation capture, busy/
idle/null behavior, latest replacement, stale/retired results, cancel/close,
foreign/matching abandonment, missing EOF, independent raw diagnostic bytes,
base/UV vertices surviving coordinator destruction, all four codec categories,
unsupported budget metadata, invalid raw opcode, peer/control correlation,
standalone-admission bypass rejection and slot recovery. These are small in-memory
state/protocol values, not native failure or cancellation probes.

Fresh clean CMake build and source quarantine guard pass. The inventory contains
the previous140 cases plus two pure coordinator contracts, with unchanged prior
test sources. Final11pure preflight and142ordinary CTest pass (16.19seconds).
SwiftPM runtime/helper packaging passes; no helper execution. Its deprecated
native build-system warning remains. Focused supported macOS ASan/UBSan passes;
no leak-check claim. QueueBridge measures209/234lines,94/102regions,20/20functions
and41/44branch outcomes. Residual catches/rollback and exception diagnostics are
not fully covered; this does NOT meet the whole-library95% or core100% targets.
Do not add quarantined allocation/crash probes or unreachable branches to claim
coverage. No whole-core, app-acceptance or comprehensive sandbox claim follows.

Production pin41af7d4c, all quarantine restrictions, original source/stashes,
protected QA processes and global SVG remain unchanged. Full editor parity and
M4–M8 acceptance remain incomplete.

## Serial delivery verification

Replayed onto accepted core #210, preserving the accepted UV aggregate text
budget, checked UV fixture indexing and decoded-budget portability corrections.
Source inspection found the same mixed uint64_t/ULL deduced initializer pattern
in the new coordinator test. It now uses std::array<uint64_t, 2>, applying the
established #198 GCC portability correction before delivery. No new Linux red
execution is claimed; the earlier #198 compiler failure is the basis for this fix.

The quarantine guard and audit pass with 143 ordinary registrations: the accepted
141 plus two pure coordinator contracts. All four XPC resource locks remain.
A fresh clean build in the single shared root, root clang-format and 21 pure
C/C++ contracts pass. Normal and optimized policy suites each pass four tests;
pure outcome/admission/channel checks pass. A fresh coordinator test linked only
to the pure bridges/codecs/queue passes ASan/UBSan with detect_leaks=0; no leak-check
or native-engine sanitizer claim follows.

SwiftPM runtime/helper builds pass using explicit shared scratch/caches
(2.75/1.46 seconds), with 17 runtime sources disjoint from 34 engine sources and
five startup sources. The existing native-build-system deprecation warning remains.
No helper is executed. Four unchanged healthy XPC regressions pass in 2.11 seconds:
socket base returns 240 vertices (host19983/service19984), and socket UV returns
240 vertices/80 triangles (host19993/service19994). Each verifies actual EOF,
OS peer and C admission; these jobs do not adopt the coordinator yet. Owned cleanup
follows host exit and service PID disappearance, not observed service exit status.

All 143 ordinary CTests pass in 22.25 seconds. All 276 source hashes and six fresh
fixture/sanitizer binary hashes remain equal. Earlier coverage is historical
scoped evidence, not a new profile or whole-core claim. No native cancellation,
foreign-peer, retirement, overlap, fault or pressure probes were run. Production
pins, quarantines, protected apps, global SVG and OS-managed containers remain
unchanged. Host/editor adoption and full acceptance remain open.
