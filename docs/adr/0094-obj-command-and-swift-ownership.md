# ADR 0094: Deliver OBJ command dispatch and Swift ownership verification

Status: Local implementation; review and full M7 acceptance pending (#99).

Dispatch .obj destinations, case-insensitively, to the original-polygon C API.
Reuse existing bounded source reads, alias/symlink rejection, same-directory
private staging, flush/close and atomic rename. Retain the FBX path unchanged.
Reject --baked OBJ explicitly instead of silently dropping materials; texture-size
still requires baked mode. Output quotas retain the C API meaning. This is static
procedural geometry only; imported/rigged/animated/GLB paths remain quarantined.

A Swift6 strict-concurrency/warnings-as-errors C consumer copies Data, diagnostics
and payload count while each owner lives, destroys it before return, compares two
independent exports and transfers immutable Sendable values across actors. It
checks actual polygon text. CMake registers this consumer on supported Apple hosts.

The new CLI regression first failed because .obj was rejected by the old command.
It now verifies uppercase/Unicode destinations, actual vertex/face text, explicit
baked rejection, quota rejection and preservation of existing output/source.
Recovery uses a fresh Debug build; all 98/98 CTest contracts pass (39.70s),
including the actual Swift consumer and both verifier modes. SwiftPM passes
(1.66s). All ten ordinary CLI filesystem contracts pass through a fresh strict-
warning ASan/UBSan executable (0.808s), instrumenting the command and OBJ boundary;
inherited engine/other adapters and system zlib are not instrumented by this
scoped run. Formatting, manifest/consumer lint and Qt/quarantine checks pass.
Exact tested recovery source is 760d8c043df64b63fab58d368e97d692a959ff3b; source
bundles, tree/binary/output hashes and logs are retained with recovery evidence.
No fault/crash probes ran.

Fresh recovery CMake and SwiftPM commands each produced identical2492byte Capsule OBJ
with42 original shared vertices and48 polygons. Independent Assimp dumps preserve
polygon arity, corner order/positions within float importer tolerance2e-5 and
identity transforms. This comparison uses emitted OBJ text as its source oracle;
it does not independently prove original generator precision. ADR0091's controlled
source-Object verification and ADR0092's healthy precision regression cover those
separate boundaries. Do not combine them into a full workflow/parity claim.

Native editor export service/UI adoption remains open, as do coverage expansion,
other formats and full M7 acceptance. Production pins/quarantines are unchanged.

## Reproducible controlled import verification

The committed `tests/core/verify_generated_obj_import.py` compares emitted OBJ
text with a separately produced Assimp XML dump. It checks finite source/import
positions, valid indices, polygon counts/arity/order/winding and both identity
transforms at the documented float-import tolerance. Seven value-only regression
tests run normally and with Python optimization, covering changed coordinates,
nonfinite/missing streams, triangulation/reordering/reversed winding, invalid
indices/source records and optimized false acceptance. Negative values remain
in synthetic Python inputs; only fresh known-positive output reaches Assimp.

```sh
"$BUILD_DIR/dust3d-export" tests/core/fixtures/Capsule.ds3 "$FIXTURE_DIR/model.obj"
assimp dump "$FIXTURE_DIR/model.obj" "$FIXTURE_DIR/model.assxml"
python3 -B -O tests/core/verify_generated_obj_import.py "$FIXTURE_DIR/model.obj" "$FIXTURE_DIR/model.assxml"
```

The output directory is caller-owned and empty before this controlled acceptance
run. The text oracle does not independently establish generator precision or
unused-vertex preservation by the importer; those are separate boundaries.
