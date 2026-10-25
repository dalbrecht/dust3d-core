# Development worker UV visual-family verification (#164)

The `worker_uv_visual_contracts` CMake test uses the newly built development
helper, exact current request identities and normal supervised completion. Its
inputs are the licensed `Sword.ds3`, existing synthetic MIT
`RoundedCrossing.ds3`, and an independently assembled small MIT single-node
archive in the test source. No unsafe imported asset or quarantined fixture is
used, and no malformed native document or OS-failure probe is added.

The test requires more than equivalence to the in-process producer:

- Sword retains real gradients, distinct endpoint colors, native resolution 512
  and valid chart references.
- RoundedCrossing retains mapped and explicitly unmapped triangles; unmapped
  corners remain zero, and source association is independent of mapping.
- The synthetic image chart resolves a braced image ID from the matching
  unbraced descriptor, retains the original image ID, non-square 48x96 resolved
  dimensions, original opaque legacy component key, and green/128-alpha source
  RGBA. The test does not contain or decode pixels.
- Complete paired geometry/UV wire values match the direct producer, preserve
  three retained corners per UV triangle, and are admitted only after the actual
  helper exits normally zero and pending identity validation succeeds.

Fresh source quarantine guard/audit and clean build pass. All 120 ordinary
contracts pass in 20.78 seconds; the focused visual contract passes in 0.78
seconds. ASan/UBSan instrumented copying/reply wrappers pass these healthy visual
families; inherited engine, helper and transport are not fully instrumented.
Visual-family-only coverage of `WorkerUvOperation.cc` measures 136/177 lines,
87/124 regions and 49/90 branches. Exceptional/inconsistent-owner guards remain
unforced, and this is not a whole-core coverage claim. Incremental unchanged
SwiftPM product compatibility passes in 0.09 seconds, without compiling the
CMake-only helper. Changed formatting and whitespace checks pass.

This is metadata transport acceptance, not reference-render, texture baking,
image decoding, signed helper, containment or production adoption evidence.
`WorkerBuild.h` detects sanitizer instrumentation only; it does not expose a
source fingerprint or authenticate executable identity. Trusted-helper packaging
remains required by #123 before Apple adoption. All quarantines and production
pins remain unchanged. Full M4/M8 acceptance is still open.


## Serial delivery verification

This stage is replayed after #191 onto accepted UV dispatch/admission. It
preserves #190's shared component-key/chart-image text quota and the checked
pure admission fixture access. The historical 120-case audit above predates that
additional pure quota regression; the current inventory contains 121 ordinary
contracts. The fixture README audit difference is provenance documentation only;
its bytes match the saved stage.

Fresh clean source-audited build, eleven pure preflight contracts and the normal
visual-family pilot pass. The current full-suite timing is recorded in the PR;
all 121 contracts pass. Fresh scoped ASan/UBSan copying/reply wrappers pass the
same healthy families; inherited engine/helper/transport remain outside full
sanitizer coverage. The earlier coverage figures are historical prepared-stage
measurements, not new whole-core coverage. SwiftPM export compatibility,
formatting, whitespace and recorded executable source hashes pass. The existing
native build-system deprecation warning remains. No production dependency,
quarantine, rendered reference baseline or native failure probe changes.
