# ADR 0140: Route indexed worker output with optional source hints

Status: Accepted implementation, pending reviewed delivery.

## Context

Core #223 owns same-generation indexed/source pairs, but no indexed worker opcode
or C consumer route exists. The current editor uses `generateSourcedIndexed`, then
on source status 3/4 retries ordinary indexed generation while retaining the source
issue. Worker adoption must preserve this optional-hint behavior inside the worker,
without defaulting to parent native generation. Apple #279/#280 remain adoption gates.

## Decision

Add **operation 4, generateIndexedSourcedMesh**, using the existing source-budget
request format and version-two indexed/source response; add **operation 5,
generateIndexedMesh**, using opaque raw archive bytes and indexed mesh response.
Existing operation numbers 1–3 and wire formats remain intact. Ordinary indexed
requests reject unrelated UV options. Dedicated indexed/source construction shares
source encoding and original typed receipts; construction never parses archives.

Native dispatch validates exact operation/kind/length before generation. Indexed
sources retain one-generation derivation. Ordinary indexing uses the same pure
export policy and native copy validation, and drops its independent expanded owner
before allocating retained indexed copies. Both return complete correlated geometry
or original bounded typed failure. No-geometry remains distinct failure status 5
for later successful-empty application mapping.

Child, persistent-service and queue-current semantic admission now include both
indexed alternatives. A grouped `IndexedReplyLimits` parameter carries independent
ordinary/indexed-source policies alongside the existing families. C semantic
limits propagate into both complete indexed geometry and aggregate source-pair
decoders; stale/current-source, EOF and peer/receipt checks remain unchanged.

C ABI additions preserve existing struct layouts and enum numbers. Decoded kinds
**5 (indexed)** and **6 (indexed/source)** expose unique-table vertex count,
triangle count derived from indices and separate index-count/range-copy methods.
Range subtraction precedes addition or writes; valid empty ranges need no output
storage, while null/wrong-kind/range/storage rejection writes nothing. Vertex and
warning helpers support both expanded and indexed owners; source getters support
both source variants. Owners are immutable after publication, and all returned
index/vertex values are independent. Borrowed strings still expire at destruction.

This provides worker capabilities, not an implicit retry controller. Future editor
adoption must retain the original source issue and ordinary-failure cause, observe
parent-issued monotonic identities and preserve successful emptiness. It must not
retry by invoking the parent engine. ABI version 1 remains a layout version rather
than feature negotiation; matching client/helper revisions are required. Internal
C++ variants/signatures changed and all source clients rebuild.

No payload ceilings are widened or claimed equivalent to current editor budgets.
Apple #280's supported-range audit remains required before default adoption.
No Swift decoder, Apple pin/backend switch, generation-statistics fabrication or
source-aware XPC acceptance claim. Materials, mips, real persistent-session/cache
statistics and native editor acceptance remain open. Logical limits do not bound
native workspace, container capacity, CPU or RSS.

## Validation and limits

- TDD red: the new contract fails to compile for missing indexed operations/APIs.
  The first pure run then identifies an expanded-only warning getter; it is updated
  to retain indexed warning text. Both routes pass after that correction.
- Pure tests cover independent opaque request ownership/options, exact operation
  correlation, wrong operation/kind/length rejection before generation, all three
  C admission routes, original indexed/pair error categories, no-geometry status,
  per-index source hints, unnormalized weights, index ranges, warning lifetime,
  negative-zero attributes and existing expanded corner compatibility. Header
  unknown-opcode cases use 6; all five supported operations explicitly round-trip.
  Synthetic process/EOF observations test admission logic, not authentication.
- Two fresh known-positive licensed Capsule descriptor jobs exercise indexed/source
  and ordinary indexed generation with default options, exact response identity,
  complete 240-index output and source correspondence. No malformed archive reaches
  native execution. This is not an actual source-failure retry or new XPC pilot.
- Rebuilt new/expanded-source/decoded/protocol focused contracts pass. Fresh full
  build, source quarantine and **152/152 ordinary CTests pass in 45.56 seconds**
  in the established authorized execution context. Changed-source hashes remain
  stable; static clients are rebuilt/relinked.
- Refreshed scoped ASan/UBSan passes pure cases and both small healthy runtime
  jobs, leak detection disabled. Selected runtime/adapter/codecs are instrumented;
  linked inherited engine libraries are not. No leak or whole-engine coverage claim.
- New C index count/range getters cover **14/14 branch outcomes**; indexed lookup
  and shared vertex helpers cover **10/10 outcomes**. Focused native indexed adapter
  coverage is **152/204 lines (74.51%), 92/148 regions (62.16%), 11/13 functions
  and 45/96 outcomes (46.88%)**. Remaining native failure, defensive and option
  paths stay unprobed and below the adoption target. Partial old-route/engine
  coverage from this executable is not total runtime/engine coverage. No unsafe
  probes are introduced to raise it.
- SwiftPM runtime packaging, C++ formatting and whitespace pass. Existing native
  builder deprecation remains. One shared build root only; source-only WIP backups
  are recovery artifacts. No UI changes; screenshots do not apply.

All core #67/#69 GLB, #83 writer, #84 fault, #95 animation and Apple #199 rig-pose
quarantines remain. No new malformed-native-input, crash, allocation-failure,
signal, broken-pipe, native cancellation or resource-pressure probes, stale
binaries, app control or global SVG/protected-QA/OS-container changes. Completed
clean exact-head automated review and all CI green remain mandatory before merge.
