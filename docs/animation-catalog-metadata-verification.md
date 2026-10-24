# Inherited animation catalog metadata verification

Tracking: [Apple #145](https://github.com/dalbrecht/dust3d-swiftui/issues/145).
This inventory describes declarations, not accepted native playback.

| Rig | Non-alias types |
| --- | ---: |
| Biped | 85 |
| Bird | 24 |
| Fish | 5 |
| Insect | 15 |
| Quadruped | 22 |
| Snake | 5 |
| Spider | 20 |
| Total | 176 |

Four preserved legacy names bring the total to 180: BirdForward → BirdFly,
FishForward → FishSwim, InsectForward → InsectFly and SnakeForward → SnakeSlither.
The inherited algorithm combines 68 base rows, 48 additional biped rows, four
aliases and 60 directional variants. Qt's reference UI filters aliases and compares
rig names case-insensitively when offering new clips; standardBones itself remains
case-sensitive. The reference application's parameter table is a separate contract
and has not been ported by this metadata stage.

Independent contract examples include BipedSprint (0.65 seconds, 32 samples, loop),
BipedIdle (4 seconds, 90 samples, breathingAmplitudeFactor 0.65),
QuadrupedRunBackward (travelAngleDegrees 180, stepLengthFactor 0.55),
BipedWalkForwardLeft (45 degrees, disabled foot roll/forward lean overrides), and
BirdForward's preserved BirdFly identity/timing. All entries require a present
generator identity and finite positive timing; all numeric overrides are finite.
The 48 additional rows retain entry/exit pose tags in the generator-free structure.
Those tags and numbers do not prove valid pose transitions or sampling behavior.

Verification evidence:

- Missing-header compile failed before implementation.
- All 48 rows compare token-for-token with the original dispatch table, including
  generator identifiers; syntax-only compilation and the build-only dispatch object pass.
- Standalone metadata contract passes without generator, rig or mesh symbols.
- Quarantine guard passes; all existing test sources match the previously audited
  ba82aaea parent, with the new static-data contract inspected separately.
- Full ordinary Debug CMake build and 101 CTest contracts pass (19.86 seconds).
- After adding the build-only dispatch object, full CMake build passes; runtime test
  command graph remains identical for all 101 cases.
- Metadata-only ASan/UBSan contract passes using fresh sources and ordinary data.
- SwiftPM builds the Dust3DEngine static product in the shared build directory
  (23.41 seconds). No Apple production pin is changed.
- New C++ files/table follow the inherited .clang-format; Package.swift formatting passes.

Not verified: sampling, poses, duration/playback fidelity, audio, Qt execution,
parameter inspector descriptors/units/ranges, C ABI/Swift owned catalog copies,
native UI integration or complete M6 acceptance. This stage does not close the
animation quarantine, claim C++ coverage targets, or authorize excluded probes.


## Standalone delivery dependency correction

The original local catalog extraction omitted the qmake source registration for
biped/clip_metadata.cc. Existing reference callers require that definition. The
corrected catalog-foundation-delivery branch adds the source and metadata/shared
declaration headers without importing later preset or descriptor implementations.
A focused manifest regression first fails against the extraction and passes after
the registration fix. All existing tests remain byte-identical and all 101 prior
runtime commands remain unchanged; the manifest-only contract is the 102nd case.
Fresh Debug CMake build/102 CTest contracts pass (19.31s), scoped metadata ASan/UBSan
passes, and SwiftPM builds in 7.48s. No Qt executable was built/run; this verifies
source membership and portable linkage, not Qt runtime behavior. Production pin,
quarantines and protected applications are unchanged. No executable ran after
checkpoint publication. This correction supersedes the known broken intermediate
for standalone publication; later preset/schema stages must keep this registration
and extend its existing regression rather than add a duplicate test target.
