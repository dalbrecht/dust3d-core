# ADR 0147: Same-archive imported render producer

Status: Accepted for explicit internal production; platform/transport adoption pending.

## Context

Issue #239 follows joined values (#237). Surface/material correspondence alone
cannot establish provenance. The existing Apple loader prepares both from one
unchanged archive, joins definitions in surface order, decodes only those models,
retains encoded bytes and returns absence when no imported model is rendered.

## Decision

`generateImportedRender` loads material facts and surface from the same stable,
immutable archive vector. Each native owner is held with RAII, independently copied
under layered quotas and destroyed before platform decoding. Empty material facts
return absence before surface generation. Native entry points retain their existing
serialization. Original native failures are mapped through `copyNativeFailure`
and stored in `ImportedProducerNativeError` with original status and cause bytes.
No native pointers escape, no archive write occurs, and no alternate parent path
is selected automatically.

`prepareImportedRender` is a portable value boundary. It validates complete facts
and surface, returns absence for no rendered model, then resolves exact byte keys
before moving definitions. It selects catalog-order definitions, discards unused
facts/bindings from the render result and retains each encoded image. Duplicate
facts reject through the original material validator. Missing selected definitions
have a distinct producer cause. Direct callers must establish same-archive
provenance; matching identities do not authenticate it.

The injected `ImportedRenderImageDecoder` runs synchronously on the calling worker
thread. Its implementation must enforce input/output limits before allocation and
return source-scanline tight straight RGBA8 without orientation, color conversion,
material factors or premultiplication. No Apple/Qt types enter portable code. The
future Apple worker supplies its PNG/JPEG implementation; no platform decoder or
C callback ABI is installed by this stage.

Before decoding, selected metadata is checked and charged against independent
logical/wire budgets. Per-call remaining pixel capacity is the minimum of the
pixel quota and remaining logical/wire space after metadata and optional-image
records. All selected encoded-image input sizes are checked before decoding starts.
Each decoder receipt is independently checked for returned byte quota and tight
shape before another image is decoded; original decoder exceptions propagate.
The complete final joined contract is revalidated before return. Adapter receipt
checks cannot stop a faulty decoder from allocating first: enforcing its supplied
allocation limit is part of the decoder contract, not an RSS guarantee.

Defaults retain 64 MiB independent native material/surface limits, existing fact
and render limits, and 16 MiB per-image decoder input. Root/capacity/allocator,
lookup/encoding, native generation and decoder workspace are excluded as documented
by the underlying layers. These policies are not Apple supported-range acceptance.

## Verification and remaining gates

TDD began with missing-header failures for value and native APIs. A fake decoder
regression failed when an invalid first receipt reached another decode; sharing
the existing scalar image validator fixed immediate rejection. Tests cover exact
selection/order, unused and untextured definitions, original decoder causes,
missing/duplicate definitions, absence, encoded and aggregate quotas, returned
quota/shape failures, native scalar-policy guards and owned native failure data.
Native integration uses only existing positive ImportedSurface and Capsule fixtures
with a fake decoder. These tests do not claim actual PNG/JPEG pixel correctness.

All 164 ordinary CTests pass (34.72 seconds), with ten source/build-spec hashes
unchanged across the final gate. SwiftPM runtime builds (1.36 seconds), retaining
its existing native-builder deprecation. Selected pure/native producer code passes
ASan/UBSan with leak detection disabled. Pure producer: 70/70 lines, 41/41 regions,
6/6 functions and 34/34 branch outcomes. Native wrapper: 32/34 lines, 25/27 regions,
2/2 functions and 15/18 outcomes. Actual native load failures and oversized archive
storage are unprobed; do not add malformed/fault/pressure probes to increase coverage.
Linked inherited engine code is not fully instrumented, and no leak/full-engine
coverage claim follows.

No opcode, C decoded kind, ImageIO implementation, Swift consumer, backend default
or production dependency pin changes. Platform worker integration, real generation
session/cache metrics, C/Swift transport, supported-range parity and full M4–M8
acceptance remain open. All feature quarantines remain intact.
