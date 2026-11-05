# ADR 0203: Preflight interpolation identities

Status: implemented; focused local contracts passed.

## Context

Issue #315 records a valid two-endpoint tube whose node UUIDs share their first half. Inherited interpolation combines UUID halves to derive two inserted node IDs and three segment IDs. Those formulas can produce duplicates or alias existing node/edge keys, overwriting geometry or producing a self-edge before rest-rig processing.

## Decision

Collect eligible edges without changing the snapshot. A portable value-only planner validates their references and canonical braced UUID syntax, then reserves all original node and edge identities and every uncontested legacy candidate before generating replacements. Reservations also recognize case and brace spelling aliases. The plan preserves caller output order while assigning contested candidates in stable edge-ID order.

Keep the inherited UUID formulas exactly when their candidates are uncontested. Collisions use a deterministic, sequential UUID-shaped namespace, checked against all reservations. The fallback fixes numeric formatting to the classic locale, does not use the inherited global random generator, and cannot consume a later edge's uncontested legacy ID. Finite occupied inputs imply a finite number of skipped candidates; the counter has an explicit overflow error. No exhaustion fixture is introduced.

Only after the complete identity plan succeeds does interpolation insert nodes and replace edges. Existing coordinate/radius/deformation math, edge orientation, bone metadata, source ordering and opt-out rules are retained. Internal generated IDs change where the inherited formula aliases another entity; source document IDs and unrelated records remain intact. SwiftPM discovers the new portable source through its existing mesh directory membership; CMake lists it explicitly. No dependencies or production Apple core pin changes.

A cached part can acquire a different generated identity when an unrelated original node or another interpolation plan reserves its previous ID. Retain exact per-part identity receipts in the generated cache, compare them before ordinary dirty-flag propagation, and mark only changed or removed plans dirty. Publish the new receipts only after successful generation. Account their map/string ownership in the existing retained-payload visitor; removed plans release their payload.

## Verification

The pure contract first failed to compile with the API absent. A value-only adapter containing the exact inherited formulas then failed the expected duplicate-ID assertion. The planner passes legacy compatibility, shared-prefix collisions, original/generated and node/edge table conflicts, later-candidate reservation, input-order permutations, repeated determinism, UUID spelling aliases, process-locale independence and bounded invalid-reference cases. A tiny grouped-hex locale regression failed before the formatting was fixed to the classic locale.

Fresh normal and ASan/UBSan interpolation contracts passed. Ordinary small tube fixtures check original/generated identity counts, untouched original records, distinct directed chains, finite mesh positions and valid indices, exact interpolated ring/radius/deformation values, mirrored topology, bone-name transport, rest-rig extent and node influences. A bounded cached/fresh comparison first demonstrated stale source ownership when another part reserved an earlier generated ID. Its regression now checks affected-part rebuild, unrelated component reuse, unchanged-plan reuse and interpolation opt-out. A value-level payload check verifies receipt accounting and release. Three related indexed-source, hollow and procedural-UV contracts passed. Actual CMake and SwiftPM engine products built.

These are scoped contracts rather than a full C++ coverage claim. M5 foundation acceptance, full Qt parity and Apple runtime adoption remain separate. Quarantined allocation-failure, crash, signal, exhaustion, writer, animation evaluator and live-pose paths were not exercised; the separately denied broad parent inventory was not run.
