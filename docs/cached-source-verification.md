# Cached source output verification

Tracking: core #27. Design: [ADR 0009](adr/0009-cached-source-generation.md).
The stateless [source contract](source-provenance-verification.md) still applies.

`dust3d_generation_session_load_sourced` shares the existing session load pipeline
and engine mutex. It captures original node/part membership from the current
validated snapshot before preprocessing, then resolves cached engine maps while
copying retained triangle corners. Output ownership and source budget are separate
from cache retention. Invalid options preserve old cache/statistics; an opted-in
source failure clears partial cache and preserves its primary cause. Cache-only
failures retain valid output and their separate diagnostic. Successful emptiness
keeps NO_GEOMETRY, with no partial source owner returned.

TDD: the expanded session contract failed to compile on the missing sourced-session
entry point before implementation. Tests exercise real Capsule edits/restores,
opaque asset revisions, current unbraced identity spelling, actual multi-node Cloth
weights across a cache hit, and geometry equivalence against stateless generation.
They verify source owner survival across loads/reset/deletion, independent source
and cache limits, invalid-option preservation, source-limit failure/retry, successful
empty output, and a real cache-clone allocation failure with valid source fallback.
Existing mixed concurrent worker cases now include sourced calls on shared and
independent sessions, retaining their bounded worker stacks. Existing unsourced
cache/invalidation/allocation contracts run unchanged alongside these additions.

The C translation unit calls the new entry point, and the Swift 6 source smoke
copies both stateless and cached results, verifies actual reuse statistics and
retains copied original identifiers after native owner destruction. No unchecked
Sendable declaration or production fault switch was added.

Measured verification:

- Debug build and all 29 contracts passed (21.53 s).
- ASan/UBSan build and all 29 contracts passed (68.43 s), including explicit
  sourced primary-allocation and malformed-number regressions.
- Clean SwiftPM and both strict Swift 6/warnings-as-errors consumers passed;
  the cached source consumer verifies real component reuse and copied identities.
- C++/Swift format, shell syntax, diff and Qt-boundary checks passed.
- Scoped instrumentation over the source/session contracts: SourceProvenance.cc
  113/116 lines (97.41%), 16/16 functions, 71/76 branches (93.42%);
  GenerationSession.cc 230/253 lines (90.91%), 20/21 functions,
  75/80 branches (93.75%). Remaining paths include defensive exception/lock
  fallbacks and the existing multi-asset sort comparator. These are explicit
  coverage adoption gaps, not a claim of 100% core coverage. No unrelated geometry
  algorithms were instrumented by this measurement.
 Local macOS ASan uses platform defaults;
Linux CI retains leak detection. Source associations remain quantized hints, not
exact Boolean or component provenance. No UI/GPU/Qt manual acceptance is claimed.
