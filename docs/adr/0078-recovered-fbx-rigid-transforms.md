# ADR 0078: Recover pure rigid-transform conventions for FBX

Status: Delivery validated locally; review and full export acceptance pending (#99).

Recover the archived transform validation and FBX Euler extraction as pure value
operations. Define the fixed `RigMatrix` alias in its own dependency-free header.
The archived rig-owner header can consume that header during a future audited
recovery; no rig-owner, pose evaluator or skeleton-generator code is restored here.
This keeps the value contract inward of generation and future serialization.
The private recovery checkpoint records the exact saved-source provenance.

The matrix is a column-major 16-double array acting on column vectors, with
translation at indices 12 through 14. Validate finite entries, affine homogeneous
coordinates, orthonormal unit rotation and positive handedness. Retain the saved
local/world/skin translation ceilings of 1M/128M/256M. Input contexts report
INVALID_DOCUMENT; generated contexts report GENERATION_FAILED; translation
limits report LIMIT_EXCEEDED. Unknown contexts reject before matrix work.
Finite rotation-entry bounds establish that the subsequent small dot products
and six determinant products cannot overflow; redundant nonfinite checks there
are removed with that precondition documented in code.

Euler extraction uses XYZ degrees with Rz*Ry*Rx reconstruction. Clamp the admitted
small numeric drift in sine yaw and use the reference roll-zero gimbal branch,
absorbing the coupled roll into pitch. Inputs and global engine state remain
unchanged. No public C ABI or Apple dependency pin changes.

The current-source test began red on the absent matrix header. It checks the
saved independent rotation/reconstruction grid, both gimbal signs and near-gimbal
round trips, typed contexts, both translation signs on every axis, nonfinite
entries in every slot, affine coordinates, scaling, shear and reflections.
These are pure scalar/matrix contracts. They neither evaluate an animation nor
load a rig/archive, exercise a serializer, deliver a signal or inject a failure.

This enables later audited FBX serialization. Full static/rigged/animated output,
independent-consumer re-import, native UI and M5/M7 acceptance remain open. Existing
GLB, writer-fixture, fault, animation and rig-pose quarantines stay in force.

## Initial local verification

Source quarantine and formatting guards pass. The only newly registered test
target is the audited pure transform contract. A clean Debug CMake rebuild and
64/64 CTest contracts pass (32.66 seconds). Fresh C++17 strict-warning ASan/UBSan
builds of the math/validation sources pass both the transform and time/angle
contracts. Scoped coverage reports 53/53 lines, 35/35 regions and 22/22 branch
outcomes for FbxAnimationMath, and 44/44 lines, 48/48 regions and 42/42 branch
outcomes for RigTransformValidation. These measurements cover these value
operations, not the inherited core, actual rig generation or exported FBX.
The updated core SwiftPM product also builds successfully.

## Delivery revalidation

Prepared on the separately tested convention-delivery stage `ff4a74fa`, after
reviewed core master `bdb36e72`. Only the pure transform contract is newly
registered relative to that stage. The fresh ordinary CMake build and 70/70 CTest
contracts passed (16.31 seconds). Fresh standalone C++17 strict-warning ASan/UBSan
builds passed both transform and time/angle contracts; SwiftPM built successfully
(0.71 seconds). Formatting, manifest lint, source quarantine and Qt-boundary checks
passed. Earlier 64-case and scoped coverage numbers above are historical and were
not remeasured for delivery. Review of the prerequisite and this stage remains
required; no rig-owner/evaluator, serializer or production pin was introduced.
