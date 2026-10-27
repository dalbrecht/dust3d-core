# ADR 0169: Healthy procedural PNG worker integration

Status: proposed for reviewed delivery. Tracks #267 and Apple #279.

## Decision

Compile the accepted Apple callback/shared metadata sources into the development
XPC service and serve stateless jobs through the purpose-specific decoder entry point.
Enable only the procedural PNG slot; explicitly clear imported configuration so this
stage does not enable the quarantined imported/GLB provider. Session serving keeps
its existing retained owner. Callback configuration lives through the synchronous
native invocation and received channel owners remain captured through queued work.
No entitlement, global policy, production pin or app backend change is made.

The healthy fixture uses existing MIT ProceduralImage.ds3 and explicit8-pixel settings,
the same ordinary geometry/bake path already exercised with fake pixels. Parent queue
identity, independently observed OS peer PID, sandbox receipt, actual EOF and complete
control flags gate decoding. Validate240 vertices/80 UV triangles/matching tangents
and8x8 tight color-map storage; copy pixels before owner destruction and retain them
across connection retirement. The controller observes ordinary host/service exit.

## Verification and limits

The positive integration test first failed on expected baked output. Its preserved
reply carried the original explicit missing-procedural-decoder cause. Supplying only
the procedural callback makes it pass. No malformed native codec/archive, GLB generation,
crash/fault/signal/cancellation or resource-pressure scenario was added.
Fresh full build and213/213 ordinary CTest contracts pass (34.86s), including all earlier
healthy stateless/session cases, callback samples and provenance. Strict Swift6
concurrency/warnings-as-errors and format lint pass; five source hashes remain fixed.
SwiftPM library/product source did not change; prior product consumer/ASan/scoped
coverage evidence remains applicable to those exact callback files. The new RPC host
path is not separately coverage-instrumented; native/platform failure guards remain
unforced. No whole-core coverage claim follows.

This establishes healthy procedural integration and owned map transport, not independent
render/pixel parity, full output/range support, production signing/distribution, app
adoption or full containment. Core #67/#69 GLB, #83 writer, #84 fault, #95 animation and
Apple #199 remain quarantined; production pin48b0ce2d stays fixed. Apple #279/#280 and
M4–M8 acceptance remain open. Completed clean exact-head review and all CI gate merge.
