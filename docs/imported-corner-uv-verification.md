# Imported corner UV verification

Tracked by core #48; representation and limits are specified in ADR 0019.
All fixtures are synthetic MIT test inputs built independently by the tests.
No Qt/Apple image API, screenshot baseline or external asset dependency is added.

Acceptance contracts:

- GLB reader: coincident identical and distinct UV triangles retain separate corner
  records; textured triangles still require UVs on every source face.
- Imported model generator: known per-face/corner coordinates outside [0,1] survive
  deformation, original component and resolved model ownership, leaf/group preview
  expansion, real warm component caches and legacy map-only source input.
- Mirror: uncombined original/reflected triangles preserve matching source vertices,
  reversed winding/corner order and the same resolved GLB material identity.
- Mixed imported/procedural generation: union/subtraction with hard and smooth joins
  actually creates clipped imported corners. An independent dominant-plane 2D
  determinant oracle checks UV interpolation within 1e-6; procedural/generated
  faces stay unmapped. Forced root rebuilds reuse children/combination caches and
  retain identical geometry, owners and UV records. Mixed uncombined geometry keeps
  imported source triangles exact and ordinary surfaces unmapped.
- Boolean mesh states: exact source model keys, corner colors and UVs survive copying
  and clipping; the fixtures require actual generated bridges and verify absent
  UV/model ownership. UV-only/mixed unmapped input does not require color labels.
- Retained payload: reserved mesh-state UV/model-key and imported-part UV capacities
  count toward ownership budgets; exact footprint fits and one byte below rejects.

Run the complete CMake/CTest Debug and ASan/UBSan suites, the workflow's C++ format
and Qt-boundary checks, and SwiftPM compilation before publication. Local macOS
ASan uses detect_leaks=0; Linux hosted ASan must pass detect_leaks=1 before merge.
Record concrete results in the PR, including compiler/platform limits and measured
coverage. Line/branch measurements must not be generalized to the inherited engine.

The current C UV/bake entry points still reject imports. This work preserves the
prerequisite representation; it does not claim imported material rendering, export
or full M4 acceptance. Keep the Apple dependency on reviewed revisions only.

## Scoped LLVM coverage

Four existing contract executables (imported models, GLB reader, Boolean welding
and retained payload) were linked against instrumented mesh-state, generator and
GLB-reader objects. No production test hooks or stand-in generation path was added.
For executable added lines in these three implementation files, the aggregate
line-hit measurement is 55/55 in mesh_state.cc, 37/39 in mesh_generator.cc and 5/5
in GlbReader.cc. The two unexercised generator lines initialize empty model keys
for an uncombined generated mesh without model-key records; the mixed ordinary
part test exercises the separate missing-UV fallback. These residual branches
remain a coverage limit, not a claim of untestability or complete parity.

Whole-file line coverage from these selected contracts is 88.36%, 59.93% and
99.81%, respectively. Branch coverage is 73.53%, 50.08% and 92.05%. Aggregated line
hits do not prove every template instantiation/branch, and these figures neither
measure all inherited geometry nor satisfy a whole-core 100% coverage claim.
Profiles, reports and the diff-to-executable-line calculation are retained under
/private/tmp/dust3d-corner-uv-coverage. The full Debug/sanitizer suites are separate
regression gates, not equivalent to this scoped coverage harness.

## Final local gates before publication

- Complete Debug build and 39/39 CTest entries pass, 54.15 seconds.
- Complete ASan/UBSan build and 39/39 CTest entries pass, 141.26 seconds,
  with UBSan halt-on-error and local macOS detect_leaks=0. Hosted Linux leak
  detection remains a required merge gate.
- SwiftPM source product builds, 52.52 seconds; the subsequent change adds only
  the mixed-uncombined test and documentation, not product source.
- Workflow C++ formatting, Qt-boundary scan and whitespace checks pass.

Logs are /private/tmp/dust3d-corner-uv-release-*, with SwiftPM evidence in
/private/tmp/dust3d-corner-uv-final-spm.log. Timings are validation evidence, not
performance or parity measurements. Exact-head automated review and every hosted
check must pass before merge. The Apple dependency pin is unchanged.
