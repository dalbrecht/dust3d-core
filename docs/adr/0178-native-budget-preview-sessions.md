# ADR 0178: Native-budget preview sessions and typed receipts

Status: proposed for reviewed delivery. Tracks #270 and Apple #279/#280.

## Native session contract

Add opt-in `NativeGenerationSession::generatePreview` over `PreviewArchiveView` and
the corresponding `ConnectionGenerationSession` entry point. Borrow the stable readable
archive through return; do not construct an archive vector or a whole request envelope.
Validate nonzero host request/document, captured archive/native policies and non-null
storage before native work. Archive1..256MiB is checked independently, without passing
its size through the legacy128MiB header quota. No legacy limits/opcodes are widened.

Pass the original source policy to native sourced generation and the original indexed
policy directly to native index creation: source zero/default64MiB, configured at most
256MiB; indexed zero/default256MiB, positive addressable policies preserved. Copy through
ADR0176/0177 boundaries rather than legacy consumer count/text/wire quotas. Source and
derived indexed owners are retained by RAII through matching owned copies.

Keep generation, copying, same-worker fallback and actual statistics under the existing
session mutex; native entry points retain engine-global serialization. Source statuses3/4
use ordinary regeneration in that same cache owner, preserving both causes if necessary.
Status5 remains successful emptiness. No parent fallback is introduced. The standalone
`generateSessionPreviewIndexedMesh` adapter permits healthy ordinary-path verification
without inducing source failure; caller serializes the borrowed session and byte lifetime.
It validates captured controls/session/storage before native calls and returns an owned
geometry-or-classified-failure variant. It performs no authentication or replay admission.

Statistics limits are checked before work. After generation, copy actual same-call cache
observations; a statistics-only exception remains its original exception_ptr, separate
from geometry. Absence is not measured zero. Connection admission serializes outer
lifetime/document/replay checks before the native session; admitted IDs remain consumed
even when subsequent argument validation fails. Close/reset behavior is unchanged.

## Diagnostic compatibility

Actual C failures use the existing source-native receipt helper, preserving numeric
status/domain/code, original bounded cause and explicit truncation. C++ copy errors use
new pure `copyPreviewGenerationFailure`: indexed mesh16, source hints17, indexed pair18,
preview range19. Existing mesh13 and standard7 retain their meanings. Native status is0
for C++ receipts, rather than inventing a C status. Budget/arithmetic/addressability errors
select limitExceeded; semantic copy failures select generationFailed. Receipt allocation
can still throw. Unknown standard exceptions preserve their text; a nonstandard exception
is explicitly classified as unavailable diagnostic, code-1.

Extend detailed v2/v3 diagnostic domain validation from1..15 to1..19; reject0 and20+.
Existing v1/v2/v3 layouts and all previous numeric meanings remain unchanged. Older
consumers reject the new domains: only the opt-in preview path emits them, and explicit
consumer/profile adoption is still required. Existing operation9/server/app routes keep
their old method and do not start emitting these classifications. Move the range-error
constructor inline so pure receipt construction does not link native record accounting
or the engine. This is an internal unreleased C++ boundary, not a stable binary-ABI claim.

## Validation and limits

Missing session/receipt/ordinary-adapter API tests failed before implementation. Existing
licensed small Capsule sessions now cross legacy and native-budget methods, compare exact
pair bytes, observe actual cache reuse, reset retention and preserve returned owners.
Healthy ordinary generation yields matching geometry after session destruction. Wrong
document/request, null borrowed bytes, invalid captured controls/statistics limits and
missing ordinary owner reject before native work and preserve cache observations.
Connection tests verify replay consumption after admitted argument failure and existing
reset/close/independent cache behavior. No new archive fixture or malformed native input.

Pure tests construct ordinary typed copy exceptions and verify domain/code/cause/status,
codec round-tripping, bounded5000-byte cause truncation, absent-exception rejection and
fake fallback preserving source/ordinary causes and successful emptiness. They do not
inject native faults, allocation failure, signals or resource pressure.

Fresh full build and222/222 CTest pass (41.82s). Actual SwiftPM runtime builds (2.43s),
and updated native/connection contracts link and run against that actual static product.
Both C++ formatters, strict package formatting, quarantine guard and twenty unchanged
source/fixture/build hashes pass. Scoped ASan/UBSan passes with leak detection off:
receipt/codec tests link protocol code only; native/connection boundary tests instrument
those wrappers and their contracts, with engine/other libraries uninstrumented.

Separate per-executable profiles avoid merging distinct test-main mappings. Pure receipt
file31/32 lines,25/26 regions,3/3 functions,14/14 reported branch outcomes; the bad_alloc
catch is deliberately unforced (EH landing pads are not branch outcomes). Native-session
file94/124 lines,60/81 regions,10/13 functions,25/40 branches. Connection wrapper29/29
lines,8/8 regions,6/6 functions. Native-error/null-success guards, actual failure-driven
fallback and statistics-failure paths remain unforced; existing pure decisions/receipts
do not prove those native boundaries. No whole-engine/full-budget runtime, performance,
RSS or complete standards coverage claim. Larger native acceptance remains a release gate.

## Remaining adoption

This adds the borrowed native-generation/copy boundary, not an active transport route.
Output channels, C/Swift ownership, EOF/authenticated-peer/current-source admission and
Apple backend adoption remain open. Baked/imported output profiles require their own
integration. Apple pin48b0ce2d, active routes and core67/69GLB,83writer,84fault,95animation
and Apple199rig-pose quarantines are unchanged. No crash/fault/allocation-failure/signal,
broken-pipe/cancellation/large-allocation/pressure/exhaustion probes or protected app changes.
