# ADR 0101: Port reference controls as checked value metadata

Status: Local C++ schema foundation; transport/UI/runtime acceptance pending.
Tracking: [Apple #145](https://github.com/dalbrecht/dust3d-swiftui/issues/145).

The reference dynamic control table moves into parameter_catalog.cc. It uses the
same declaration values, source order, biped templates, aliases, directional
inheritance and original string-formatted preset overrides. Token comparison,
including quoted labels, allows only documented type/property/dependency changes
and explicit representations of the two custom fish conversion rows. This avoids
a second maintained table. application/sources/animation_parameter_table.h remains
a compatibility adapter for the reference widget's existing struct/callback shape.
It caches only declared nonempty schemas in stable map nodes; unknown types do not
grow that cache. Qmake and both native manifests register the shared catalog.

ParameterDefinition carries names/labels, inclusive integer bounds, nominal/raw
slider defaults, a divisor and multiplier. Native schema records have no callback
objects. parameterValue divides then multiplies; sliderValue divides by multiplier,
then multiplies by divisor, truncates toward zero and checks integer representation
before its only cast. Definition validity and resulting widget range are checked;
errors preserve a typed cause. A static assertion requires slider integer endpoints
to be exactly representable in double. Invalid native inputs never reach a legacy
inverse callback. The two fish rows need explicit multipliers (8 and 4), not a
silently simplified generic divisor. The first compile exposed these exceptional
rows; the implementation and independent examples now retain their operation order.

The reference callback adapter intentionally retains inherited unchecked callback
semantics outside native products. Its translation unit is compiled as an unlinked
OBJECT target only. No reference callback, Qt application or sampler is run in
these checks. That residual gap is explicit; new native controls must use checked
conversion. Nominal stored defaults and discrete widget projection remain distinct:
0.15 and 100/667 are different values. Metadata display must not dirty/normalize a
document, and unsupported raw values must survive until an explicit edit.

Missing-header and source-membership checks fail before implementation. The new
ordinary contract examines 1592 definition instances across 180 types, unique
names/labels, finite coefficients/defaults, valid endpoints, default projection,
unknown lookup, independent scale/direct/directional/override examples, both fish
custom scales, typed nonfinite/range/representation/definition rejection and exact
integer endpoints including representable truncated fractions. All current raw
default indices lie inside widget limits. Source comparison preserves table tokens;
existing test changes only extend reference source membership. All 104 previously
audited runtime commands are unchanged, plus the reviewed new native contract.

The actual quarantine guard passes. Full Debug CMake build and 105 ordinary CTest
contracts pass in 19.97 seconds. Scoped native schema/math ASan/UBSan passes; the
standalone contract links no sampler, rig, mesh or reference callback code. SwiftPM
builds the portable product in 0.97 seconds. New-file/manifest formatting passes.
No inherited C++ coverage, full Qt behavior or native runtime acceptance is claimed.

This dynamic table excludes separate common timing/audio widgets. C/Swift descriptor
transport, native UI, Qt/native interoperability, sampling/playback/audio and full
M6 remain open. Core #67/#69/#83/#84/#95 and Apple #199 quarantines and production
pins remain unchanged. The earlier catalog delivery still needs its focused qmake
dependency fix backported before publication; no known broken intermediate should ship.


## Checked parameter delivery validation

Replay the corrected schema stage onto accepted core #155 (`0ebc34d1`), which
merged after completed clean exact-head automated review and all six CI checks
green. Runtime and contract sources match corrected `6438ca5b`; the accepted
OBJ scene-binding verification fix is retained. New descriptor conversion checks
finite scalars and integer representability before casting, then widget bounds.
Legacy reference callbacks remain compile-only and are never evaluated by tests.

Fresh Debug CMake build and all 105 ordinary CTest contracts pass (19.81 seconds).
All 104 prior registered commands are unchanged; only the inspected guarded
parameter contract is added. Scoped metadata ASan/UBSan passes across 1592
catalog descriptors and checked boundary cases. SwiftPM Dust3DEngine builds in
0.73 seconds. Formatting, quarantine guard and isolated symbol inspection pass;
that standalone contract contains no sampler, rig, mesh or reference callback
implementation. These are scoped checks, not inherited-engine coverage or runtime
animation acceptance.

Full-tree equality across the ancestry-only rebase and matching gated source
hashes preserve the tested implementation. No native binary executes after rebase;
only delivery documentation changes afterward. Require completed clean exact-head
automated review and every CI check green before merging. Production pin and all
quarantines remain unchanged. C/Swift schema transport, catalog-guided editor
controls, runtime animation/playback/audio and full M6 acceptance remain pending.
