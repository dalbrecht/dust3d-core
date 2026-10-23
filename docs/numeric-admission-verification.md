# Typed numeric admission (#104)

`SnapshotValidator` translates only `std::invalid_argument` and
`std::out_of_range` from numeric conversion into `DocumentError` with
`DUST3D_STATUS_INVALID_DOCUMENT`. The geometry field name and original
standard-library diagnostic survive. Allocation exceptions remain the
responsibility of the existing C boundary. Numeric locale normalization and
source metadata remain unchanged.

The decoder-free regression failed before the implementation with
`stod: no conversion`, rather than a typed document error. It now verifies syntax,
overflow and underflow classification, field context and original causes, plus
valid dot/comma spelling preservation. The existing C numeric contract also
checks field context for a small invalid-syntax archive before mesh generation.
No inherited snapshot writer is used.

Validation on October 23, 2026:

- Fresh CMake Debug build and all 49 ordinary contracts passed (14.37 seconds).
- ASan/UBSan replaced the Debug products in the same build root; all 49 contracts
  passed (45.19 seconds), without sanitizer findings.
- SwiftPM compiled successfully; inherited compiler warnings remain. Formatting
  and whitespace checks passed. Coverage was not measured for this change.
- The source quarantine guard passed before test execution. The 48 prerequisite
  registrations remain intact, with one new decoder-free contract. The successful
  allocation observer retains its approved byte hash. No fault, crash, signal,
  resource-exhaustion or quarantined writer/pose/animation probes were run.

The shared build root is `/private/tmp/dust3d-validator-safe-build`. Builds run
serially; disposable compiler products are cleaned between configurations rather
than retaining a build folder per PR. Source worktrees, checkpoints, stashes and
quarantined evidence must be preserved. After SwiftPM verification, this build
root used approximately 197 MiB.

This mitigation is prepared on the exact core #102 prerequisite, not yet
published or adopted by the Apple apps. Exact-head automated review and green CI
remain required before merge. No production dependency pin changed, and no
complete rigging, animation or full editor-parity claim follows from these tests.
