# ADR 0112: Same-owner worker geometry and UV generation

Status: prepared development stage for #164; not yet dispatched by the helper.

## Decision

Generate geometry and atlas metadata with one `dust3d_uv_mesh_load` owner. The
geometry view belongs to that owner, and triangle/corner order is the C ABI's
published correlation contract. Never call independent mesh and atlas generators
and assume matching counts prove source provenance.

Validate request options and independent/aggregate consumer policies before
execution. Resolve image descriptors into bounded C ABI values. After successful
ordinary generation, inspect UV counts, borrowed array presence, bounded original
component text and chart image text. Compute exact retained-array/text and wire
charges before copying. Reserve this UV charge in the aggregate budget and pass
the remaining geometry budget to the existing bounded mesh-copy boundary. Copy
both values while the owner lives, release it, then run full paired wire/semantic
validation. No borrowed pointer escapes. The final encoded validation buffer is
temporary workspace, excluded from the logical retained-output quota.

Expose shared request/pair policy validation and the existing geometry copier
rather than maintaining divergent option or geometry-copy rules. The geometry
copier now validates its own policy as well as its callers doing preflight.
Keep inherited generation serialized through its existing C ABI. These quotas
bound retained copies/wire, not native workspace, allocator capacity or process
RSS. No pixels are decoded or baked.

Native non-success status and original diagnostic map through the existing
failure schema. Invalid request/policy causes throw before execution; operational
copy/wire limits become typed limit replies, with no partial pair. Other caught
operational causes remain bounded failure data. Allocation errors can still
propagate while constructing failure data; no failure injection is used.

## Evidence and limits

Missing-header red test preceded implementation. The licensed Capsule produces
240 retained geometry corners and 80 UV triangles. Its complete independent pair
survives owner destruction and wire round-trip; exact aggregate quotas accept,
while small output copy/wire quotas reject with typed limits. No new malformed
native inputs or OS actions are introduced. Fresh source-audited CMake build and
117/117 ordinary contracts pass (23.77 seconds); focused same-owner contract
passes in 0.28 seconds. Fresh pure request/paired codec ASan/UBSan and scoped
ASan/UBSan on the changed native copying wrappers pass. The latter links an
uninstrumented inherited engine and does not establish whole-engine sanitizer
coverage. Incremental unchanged SwiftPM product compatibility passes in 0.09
seconds; this module is CMake-only. Changed C++ formatting and whitespace pass.

The initial producer measurement, before the review correction below, covered
119/155 lines, 80/111 regions and 49/82 branches.
Native inconsistent-owner, missing-storage, exceptional/allocation and other
failure guards remain unforced; the licensed fixture does not cover all visual
families. This does not satisfy a whole-core coverage claim. Do not inject faults
or add crash probes to raise these numbers. Existing quarantines remain intact.

Helper opcode/request dispatch, current-source parent admission, ordinary
positive subprocess acceptance, containment and production Apple adoption remain
required. Production dependency and app behavior are unchanged. Full M4/M8
acceptance remains open.


## Review correction: aggregate text before copies

PR #190 finding 4371026655 identified that native inspection charged chart image
identity bytes only to the logical decoded budget. Component keys alone were
checked against maximumTextBytes. The complete wire codec rejected mixed excess
text later, after both retained output copies had already been constructed.

A scalar-only internal UvCopyTextBudget now spans both component and chart loops.
Every retained string length is charged against the same aggregate text allowance,
including repeated chart identities, before either geometry or UV copying begins.
Chart fixed-wire charges and independent decoded charges remain unchanged. Typed
limit causes remain original content-independent diagnostics.

The pure regression first failed on the missing admission helper. It covers exact
mixed key/image limits, one-byte-under rejection, per-chart repeated identities,
empty image text and unchanged prior state after rejection. A small independently
owned image-backed metadata value also confirms agreement with the existing wire
codec at the same exact/under text budgets. No native image-backed failure case,
malformed native archive or allocator probe is used. The native loop wiring and
pre-copy ordering were inspected directly; actual native fixtures remain the
ordinary licensed Capsule.

Fresh source guard and audited inventory add only that pure test, for 118 ordinary
contracts. Clean Debug build, nine pure preflight checks (0.91 seconds), all 118
CTest cases (21.53 seconds), pure budget ASan/UBSan, changed healthy-copying-wrapper
ASan/UBSan and SwiftPM compatibility pass. The wrapper check still links the
uninstrumented inherited engine. The new helper measures 9/9 lines, 9/9 regions,
3/3 functions and 4/4 branch outcomes. The initial whole-producer measurement
above is historical; it is not a corrected-head whole-boundary coverage claim.
Exceptional native guards remain unforced. Production pin and quarantines remain
unchanged; require fresh completed review and all CI on the corrected head.
