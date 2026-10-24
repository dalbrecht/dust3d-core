# ADR 0076: Bound snapshot XML serialization before publication

Status: Delivery candidate validated locally; automated review and dependency adoption pending.

## Decision

Replace inherited recursive component serialization with an iterative traversal
plan. Reject missing, repeated or detached components before emitting XML. Bound
component count to 65,536, total component-reference text to 16 MiB, component
levels to 126 and final XML plus any existing destination prefix to 16 MiB.
The 126 component levels reserve canvas/components within the reader's existing
128-level XML nesting limit. Callers may tighten these ceilings, not enlarge them.
Typed errors distinguish invalid limits, hierarchy and byte/count/depth rejection.

Use the same traversal and escaped-size accounting for a non-serializing
`validateSnapshotForXmlWrite` API and serialization. Count first, stage a complete
bounded string, then append with the standard string strong exception guarantee.
Retain ordinary ordering, indentation, attribute escaping, internal-field filtering
and successful append semantics. Preserve the original two-argument C++ symbol;
a three-argument overload supports tighter limits. New code remains in base,
independent of native adapters, Qt and Apple types, with C++17 and MIT licensing.

This intentionally rejects graphs whose old writer silently omitted components,
and rejects oversized writes. Consumers must handle the documented typed C++
exceptions. No STL or exceptions are newly exposed through a C ABI. The Swift
lossless document writer and production core pin are unchanged. This does not
claim comprehensive XML-name/namespace validation or inherited reader safety.

## Quarantine and verification boundaries

Issue #83's failing writer fixture remains disabled. No invalid hierarchy is sent
to serialization tests: invalid metadata is passed only to the new bounded,
non-serializing validator. Ordinary acyclic inputs verify exact output bytes,
all inherited escapes, flat sections, internal filtering, root-section presence,
append semantics and an exact successful byte boundary. Pure validation checks
missing/repeated/detached metadata and small configured budget/depth/count limits.
No allocation failure, crash, signal or resource-exhaustion probe is used.

The contract began red because the validation/error/limit APIs were missing.
Final normal CMake build and 63/63 CTest contracts passed (30.11 seconds).
A fresh strict-warning ASan/UBSan build of the writer and its contracts passed;
this is scoped instrumentation, not a fresh whole-engine sanitizer run.
Formatting and the source quarantine guard passed. Measured writer coverage is
156/157 lines (99.36%), 97/98 regions (98.98%) and 77/78 branch outcomes (98.72%).
The one uncovered line/outcome is oversized existing destination rejection. Deliberate writer rejection and allocation-failure publication
paths remain unexecuted under quarantine; review their staging/strong-guarantee
structure without claiming measured full coverage. No leak-sanitizer claim.

Before CTest, the quarantine source guard passed. Compared with the previously
audited master 537cfe01, the only added registered target is the source-audited
snapshot_writer_contracts. The retained-payload observer still counts successful
allocations and has only the standard malloc-failure fallback; no failure
injection was added. The forbidden fixtures and unfinished GLB/rig/animation paths
remain unavailable. Use the single shared build root for all validation.

## Delivery revalidation

Rebased on reviewed master `7429ab82`, the complete ordinary CMake build and
68/68 CTest contracts passed (16.14 seconds), including the ordinary Swift
consumer. Fresh strict-warning scoped ASan/UBSan writer contracts passed without
leak-sanitizer options. The SwiftPM static product built successfully (19.41
seconds), and Package.swift lint, new C++ formatting, portable Qt boundary and
source quarantine checks passed. The initial restricted SwiftPM invocation could
not initialize its nested manifest sandbox; the authorized normal build succeeded.
Inherited archive-format, UV bit-field and bundled JSON warnings remain; no new
writer warning was suppressed. Prior scoped coverage above is historical and was
not remeasured for this delivery. No invalid metadata was sent to a serializer,
no quarantined fixture was restored and no production dependency pin changed.
