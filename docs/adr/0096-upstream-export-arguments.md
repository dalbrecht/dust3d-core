# ADR 0096: Accept upstream single-source export arguments

Status: Local implementation; review and full M7 acceptance pending (#99).

Source inspection of application/sources/main.cc shows Qt supports one .ds3
source with repeated -o/-output export destinations. Add that argument expansion
to the Qt-free command instead of requiring existing scripts to manually expand
every pair. Allow targets before/after the single positional source; consume each
target literally. Bound target count64 before filesystem/generation work. Reject
missing targets, multiple positional sources and mixing legacy output flags with
explicit --batch. Existing option termination and ordinary two-path mode remain.

Route expanded jobs through the already-tested global source/output alias
preflight, serial export use case, independent atomic publication and diagnostic
summary. Continue per-job failures. Legacy aggregate failure returns1, matching
Qt's convention; explicit --batch retains5. Global argument/preflight errors
retain2 and commit nothing. An alias discovered after an earlier commit remains
a per-target failure and preserves the earlier file. Each target generates from a fresh source read. This is argument
and status compatibility, not a claim of byte-identical output, one-generation
performance, paste-XML/color-toggle behavior or full Qt batch parity.

TDD first observed unknown -o rejection. New contracts cover repeated mixed flag
spelling/ordering, successful OBJ/FBX outputs, unsupported-format partial failure,
ambiguous/incomplete invocation and65-target metadata-only admission. Unsupported
target extensions reject before encoding; the regression uses .format and no GLB decoder/writer or other
quarantined code runs. No crash/allocation/signal/resource-exhaustion probes ran.

Recovery preserves the corrected batch inode guard, POD publication identity
return and stronger retry/diagnostic checks. The case-identity regression now
exercises both explicit and legacy expansion, and temporary-file cleanup checks
include nested output directories. The case-insensitive branch ran locally; a
case-sensitive CI volume is required for the other branch. Source inspection
confirmed the upstream flags and aggregate status without running Qt.

Fresh Debug CMake/CTest passes 98/98 contracts (39.75s). SwiftPM builds (1.67s).
Both command products pass all 20 ordinary CLI contracts. A fresh strict-warning
ASan/UBSan command passes the same 20 cases (1.483s), instrumenting the CLI and
OBJ boundary only; inherited libraries/system zlib are not instrumented in this
scoped run. Formatting, manifest/consumer lint and Qt/quarantine checks pass.
Exact tested recovery source is e610110f9d63a9bb16abd1775f2b1ca43ef30498; source
bundles, red/green logs and source/binary/output hashes are retained.

Fresh CMake and SwiftPM expansions with targets before/after the source produce
byte-identical 2,492-byte OBJ and 9,706-byte geometry FBX with complete summaries.
The committed optimized OBJ verifier passes both fresh Assimp imports. Its
emitted-text oracle is not independent generator-precision evidence. This run
checks FBX header/equality, not new full independent FBX acceptance.

Historical scoped command coverage (not remeasured after recovery/batch guard):
265/297 lines89.23%,280/342 regions81.87%,172/212 branch outcomes81.13%.
Parser:62/62 lines100%,89/90 regions98.89%,66/72 branch outcomes91.67%. Existing
OS-error/interruption/cleanup and generic exception paths remain unforced; no
whole-core coverage claim is made.

Production dependency pins, GLB/animation/rig quarantines and native app behavior
are unchanged. Reviewed native binding/UI adoption, remaining format workflows
and full M7 acceptance remain open.


## Delivery preparation on the serial batch PR

Replay this stage onto publicPR150 head f5a6eafb, preserving PR149's scene-binding
verifier correction and the batch delivery evidence. Runtime/CLI test sources
remain byte-identical to the original audited be82245b stage. At initial validation,
the parent remained review-gated; the preparation did not open another public PR.
PR150 subsequently merged at ba0b1a36 after completed clean exact-head automated
review and all six CI green. Full-tree equality before/after the ancestry-only
rebase proves the final source is unchanged; no native binary executes after it.

Fresh Debug CMake build and all98 ordinary CTest contracts pass19.02s. All20 CLI
cases pass against CMake and SwiftPM products. SwiftPM builds0.80s. Fresh strict-
warning scoped CLI/OBJ-boundary ASan/UBSan passes20 cases1.426s; inherited libraries
and system zlib are not instrumented. New positive expansions place one output
flag before the source and another after it, produce equal2492-byte OBJ/9706-byte
FBX across products, and pass the strengthened optimized verifier on both fresh
Assimp OBJ dumps. These compare emitted text, not independent generator precision
or a new full FBX acceptance. All negative scene mutations remain Python-only.

No new coverage measurement, production pin change, protected app control or
quarantined runtime work. Before publication, place the stage on accepted master,
verify tree/source provenance and rerun the delivery gate if anything changes.
Completed exact-head automated review and all green CI remain mandatory; full
M7 and native editor integration are still incomplete.
