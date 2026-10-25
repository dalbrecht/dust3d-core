# ADR 0100: Decouple inherited preset values before porting descriptors

Status: Local prerequisite; descriptor and runtime acceptance remain pending.
Tracking: [Apple #145](https://github.com/dalbrecht/dust3d-swiftui/issues/145).

The reference parameter table obtains defaults from AnimationGenerator, coupling
pure control metadata to the sampler translation unit. AnimationParams moves
unchanged into animation_parameters.h. The original defaultTiming/defaultParameters
bodies move unchanged into animation_defaults.cc under the animation namespace;
the reference class retains thin forwarding methods. Return types, persisted
numeric strings, per-clip overrides, alias behavior and unknown fallback semantics
are preserved. No sampler body is compiled into native products or run here.

CMake/SwiftPM compile the pure helper source explicitly. The standalone preset
contract links only catalog/declaration/default metadata, with no rig, mesh or
AnimationGenerator symbols. Tests cover every current type's timing/common/override
values, independent returned maps, aliases, directional presets, unknown types and
legacy boolean/missing-field behavior. The inherited tolerant getters do not become
validators for arbitrary stored input; C ABI/Swift adapters must retain explicit
validation. The reference descriptor callbacks also contain unchecked inverse
integer casts. Those callbacks have not been ported, evaluated with invalid values
or accepted as native controls; checked conversion is required by the next stage.

Investigation found the preceding generator-free catalog refactor omitted its new
biped source from qmake. A source-manifest regression fails on that real omission.
The current qmake target registers biped metadata and the new parameter/default
headers/source. Before publication, the earlier catalog foundation delivery PR must
include its metadata-only qmake fix and focused manifest guard, rather than landing
an intermediate broken reference closure. This is a source integration check, not
proof that the full Qt application builds or runs. No Qt dependency is added to
native targets; the full reference build remains an acceptance task.

Missing-header tests fail before implementation. Source comparison confirms the
moved value structure and preset method bodies are unchanged. The new tests were
inspected; existing test sources match the previously audited efa4d4a3 parent.
The actual quarantine guard remains unchanged and passes. A local registration
matcher initially confused defaults with fault; it stopped before CTest. Exact
comparison then confirmed all 102 previously audited commands unchanged plus only
the two reviewed new cases. Full Debug CMake build and all 104 ordinary CTest
contracts pass in 19.90 seconds. Scoped preset ASan/UBSan passes on ordinary
metadata; SwiftPM builds in 0.68 seconds. New-file formatting checks pass. These are
not C++ coverage, native sampling, Qt execution or whole-M6 acceptance claims.

Descriptor schemas/ranges, checked slider conversion, C/Swift transport, native
catalog controls, Qt/runtime interoperability, sampling/playback/audio and full M6
remain open. Production pins and core #67/#69/#83/#84/#95 and Apple #199 quarantines
remain unchanged; no protected app control or excluded probe occurred.


## Generator-free defaults delivery validation

Prepare this stage on PR #154 at `429a8300`, preserving the accepted catalog
foundation and its existing reference source-membership contract. That contract
now also checks animation_defaults.cc; no duplicate qmake source or membership
test is added. Runtime and new test sources match corrected `19c853c1`. The
AnimationParams type and both default function bodies retain their original tokens.
Reference methods forward to the new portable functions rather than maintaining
a second table. Inherited tolerant getters remain compatibility behavior, not
input validators, and no unknown stored document value is normalized.

Fresh Debug CMake build and all 104 ordinary CTest contracts pass in 19.41 seconds.
The 103 previous commands are unchanged, with one fixed-defaults contract added
and the existing manifest check extended. Scoped metadata ASan/UBSan and SwiftPM
(7.79 seconds) pass; isolated defaults symbols contain no sampler, rig or mesh
implementation. New C++/manifest formatting and the source quarantine guard pass.
The reference dispatch remains compile-only; Qt and animation sampling are not run.

These results cover declared timings, string-formatted defaults, aliases,
directional overrides and independently owned return values. They do not establish
raw-input validation, native parameter UI, runtime pose/playback/audio parity or
full M6 acceptance. Production pin and all quarantines remain unchanged. Require
completed clean exact-head automated review and all CI green before merge.


PR #154 subsequently merged at `dd3491a6` after completed clean exact-head review
and all six CI checks green. Full-tree equality before/after ancestry-only rebasing
and matching gated source hashes prove source identity. No native binary executes
after rebase; only this delivery note changes afterward.
