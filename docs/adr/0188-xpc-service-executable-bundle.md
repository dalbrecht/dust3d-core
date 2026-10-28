# ADR 0188: Thin XPC executable and explicit bundle trust input

## Decision

Add the `dust3d-xpc-service` SwiftPM executable over `Dust3DWorkerService`.
It reads configuration exclusively from its own XPC bundle metadata; runtime arguments
are rejected and there is no environment, sidecar or user-document trust override.
The copied pure metadata value checks the exact executable, XPC service/run-loop markers,
schema version, matching identifier/bundle basename, numeric version components and
bounded explicit host requirement. The existing service configuration compiles the
requirement unchanged; the library checks its sandbox entitlement before native work.
The executable owns listener lifetime and normal retirement exit. Metadata-only logging
preserves original native status/source/code/truncation without document diagnostic bytes.

`tools/stage_worker_service.py` creates a NEW unsigned bundle plus sandbox-only entitlement
plist from an explicit executable, identifier and UTF-8 requirement file. It copies the
requirement into Info.plist before signing; no runtime file grant is needed. It never
signs, installs, launches or overwrites artifacts. Validation/copy errors preserve their
original causes and any partial output for inspection. No new package dependency.

## Verification

Missing bundle API and missing stager produced the intended red tests. Pure regressions
also demonstrated acceptance of empty numeric version components; validation was corrected
before final delivery. Five Swift bundle cases cover preserved identity, dotted versions,
missing/wrong-typed metadata, layout mismatch, schema/policy/size/empty-field rejection.
Two fake diagnostic cases preserve native scalar classification and original copied cause
while excluding diagnostic text from logs. Four Python cases cover exact metadata/sandbox
policy, invalid scalar metadata, copied bytes, overwrite protection, mismatched basename
and symlink-source refusal. Fake bytes are never executed or passed to native parsers.

Scoped coverage from seven pure Swift tests:70/70 lines,46/46 regions,12/12 functions
across the two new helpers. This is not whole-service/engine/executable coverage; no
branch percentage is invented. Matched executable/profile retained in the shared root.
The original error/callback library continues to carry typed errors independently of logs.

Final ordinary build and233/233 CTest pass46.29 seconds. Public importing XCTest29 cases
pass. The new actual-product verifier explicitly builds both executable and service
archive, stages exact executable bytes, verifies sandbox-only entitlements, and links a
strict Swift6 consumer against the actual archive to validate generated metadata. It does
NOT launch the unsigned executable or treat a zero-CDHash syntax fixture as a trusted peer.
Existing healthy QA XPC profiles still pass through canonical service sources. Formatting,
diff and23 frozen source/fixture hashes pass. One shared build/cache root is used; final
normal flags restored after scoped profiling. No new C++/parser/ownership changes or
sanitizer claim; no new native fault, denial, malformed-input or cancellation probes.

## Remaining acceptance

Unsigned staging and syntax validation do not establish release trust, OS containment,
Team ID/designated requirement policy, host embedding/signature verification, signing,
notarization or distribution acceptance. These need the actual release identity and
separate reviewed integration. The existing QA development-pin sidecar remains test-only.
There is no production pin/route change. Apple backend adoption, native resource/range
acceptance, all quarantines and M4–M8 remain open. Tracks #290/#270/#123.
