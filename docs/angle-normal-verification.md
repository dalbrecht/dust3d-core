# Bounded angles and opening normal regression

Core issue #13. `Vector3::angleBetween` previously passed a normalized dot product
directly to `acos`. Normalizing a finite vector does not guarantee a self-dot of
at most one after rounding. The synthetic vector `(1/7, 1/11, 2/13)` reproduced
`1.0000000000000002` and a nonfinite angle locally. Delegate to the existing
`Vector3::angle` helper, which bounds the inverse-cosine input at both endpoints.
The change preserves the inherited header's CRLF endings, copyright and layout.
No public signature, dependency, C ABI status or native finite-output guard changes.

Before implementation, both new CTest scenarios failed: the direct parallel-vector
case and the complete C ABI Cloth opening fixture. The angle contract covers the
reproducer, a deterministic 512-vector set, parallel/opposite/near-parallel cases,
orthogonal vectors, and the inherited zero-vector behavior. Angular tolerances
are 1e-7 radians at parallel/opposite endpoints and 1e-12 for analytic right angles.

The wrapping fixture is an independent C++ reimplementation of MIT synthetic data
from [Apple ComponentWrappingCompatibilityTests.swift at
6886b105fd0ea1a9bca86550d56e4be020d519a5](https://github.com/dalbrecht/dust3d-swiftui/blob/6886b105fd0ea1a9bca86550d56e4be020d519a5/Tests/Dust3DKitTests/ComponentWrappingCompatibilityTests.swift):
one Model node at (.5,.5,.5), radius .2, in a Cloth group. Top/bottom openings of
.225 at 64 and 256 faces and .1 at 64 faces must generate nonempty triangles with
finite positions, bounded colors, source counts one/one and unit normals within
1e-4 after the owning C handle is destroyed. No user documents or external assets.

The first full run exposed an obsolete negative fixture: straight side-closed
profiles also relied on the unclamped-angle error. Inspection found meaningful
area in 44 of 48 output triangles. It now has finite-output coverage rather than
artificially preserving the rounding failure. A finite legacy wrapOffset=-1
removes the complete wrapping surface and remains the genuine GENERATION_FAILED
fixture. Successful empty output and malformed input retain their distinct statuses.

This numerical repair does not remove collapsed faces or establish manifoldness.
Dump inspection found collapsed wrapping lip quads and zero-area straight-profile
triangles; core #14 separately requires area/connectivity/topology cleanup and
regressions. Native controls and Qt differential parity remain pending. Do not
weaken finite checks or claim complete wrapping acceptance from these tests.

Local verification on 2026-10-17: all seventeen Debug and seventeen ASan/UBSan
CTest contracts pass; SwiftPM source build, native/test formatting, manifest lint
and Qt-boundary checks pass. macOS sanitizers use detect_leaks=0 because local
leak detection is unsupported; existing Linux CI retains it. The inherited header
has identical CRLF/newline counts; diff whitespace is checked with cr-at-eol for
that preserved source. No full inherited coverage or topology claim.
