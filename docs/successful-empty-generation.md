# Successful empty generation contract

Core issue #7 supports the Apple PR #97 review regression: clearing fill on the
document's only isolated stitching loop must remove previous geometry, rather
than be indistinguishable from failed generation.

The C ABI appends DUST3D_STATUS_NO_GEOMETRY = 5 without changing existing status
values. A NULL result with that status means the generator completed successfully
and produced a valid object with zero triangles. The original success check
still precedes this classification; failed generation, missing output objects,
malformed input, nonfinite geometry and output limits retain their error paths.
Existing nonempty owners and their borrowed buffers remain valid across empty
loads. Older callers receive a descriptive diagnostic. No STL or exception
crosses the ABI, and no inherited geometry algorithm changes.

The new portable contract first failed to compile without the status and then
failed at runtime against the old generation classification. Synthetic fixtures
exercise a filled isolated loop, its unfilled empty result, optional diagnostics,
retained owner/count/buffer lifetime, malformed input, degenerate side-closed
straight profiles remaining GENERATION_FAILED, and refilling after errors.
Fixture XML is independently serialized in tests/core/empty_result_tests.cc and
is synthetic project test data under the repository's MIT license.

Local verification on 2026-10-17: twelve Debug CTest contracts and twelve
ASan/UBSan contracts pass. macOS uses detect_leaks=0 because the system sanitizer
does not support leak detection; no local leak-check claim is made. Linux CI
retains its existing leak-enabled sanitizer configuration. New C++ formatting,
Qt-boundary checks and the SwiftPM source build also passed.
This proves the adapter distinction and regression cases, not full inherited
engine coverage or all possible empty boolean results.
