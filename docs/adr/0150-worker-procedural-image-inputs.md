# ADR 0150: Procedural archive image inputs for workers

Status: Accepted for internal producer use; baking/runtime adoption pending.

## Context

Issue #245 follows imported runtime #244. Legacy and source-baked rendering also
need procedural component color images resolved inside the worker. Those images
use a different transparent-grayscale policy from authored imported textures.
Passing a parent-decoded image or silently substituting imported semantics would
not preserve the existing Apple pipeline.

## Decision

A portable pure resolver accepts ordered component image references and stable
borrowed archive asset metadata/payload spans. It selects only referenced assets,
deduplicates normal brace/unbraced identity aliases in first-reference order, and
returns independent encoded bytes with exact archive keys and canonical braces.
Identities retain case and opaque non-hex characters, including zero UUID text,
with 36 UTF8 bytes and separator positions matching the Apple document boundary.
Both braces must be present for a braced alias. This intentionally does not inherit
the legacy Uuid constructor's unchecked closing-delimiter behavior. Invalid UTF8
and NUL reject; no Unicode normalization or case folding occurs.

Archive matching follows the existing Apple document reader: `images/`, then the
first path segment's filename stem. Other names and unreferenced payloads are not
read. Exactly one matching asset kind is required; matching non-asset metadata
alone reports wrong kind, while multiple matching assets report ambiguity. Missing
references and malformed identities remain distinct original typed errors. Empty
encoded assets remain represented for an explicit later platform decoder error.
No MIME/PNG correctness claim is inferred from a key or encoded byte count.

All metadata counts and name totals are checked before indexing. Every selected
span, per-image encoded budget, aggregate encoded budget and retained logical
charge validates before any encoded payload copying. Defaults are 10,000 references,
100,000 assets, 1 MiB of asset names, 32 MiB per encoded image and 128 MiB aggregate
encoded/logical storage. Supported reference/asset counts stop at 100,000; byte
policies are positive and at most 256 MiB. Logical output charges record objects,
identity/key bytes and encoded bytes, excluding root objects, capacity, allocator
and validation workspace and RSS. Totals start at zero and advance only after a
checked addition. Raw input pointer readability/stability remains a caller obligation.

Native extraction reuses `DocumentReader` archive envelope, model XML and snapshot
validation through a separate internal metadata entry point. The ordinary reader
keeps its existing imported-generation behavior. Metadata inspection allows imported
asset declarations but does not parse their GLB payloads or produce an imported
generation context. Callers cannot treat this result as imported-asset validation
or a generation-ready document. No second unchecked archive parser is introduced.

The observer captures bounded metadata and archive-borrowed spans, keeping original
exceptions in an exception_ptr across noexcept callbacks and rethrowing afterward.
Engine access is serialized. Snapshot root/child lists retain component-tree order;
a map iteration must not replace it. The adapter walks those lists, then uses the
same pure resolver before returning copied encoded inputs. Borrowed archive spans
never escape. Parser/workspace memory is separate from copied-input accounting.
The SwiftPM runtime's private repository include path matches CMake so this single
native adapter can access internal snapshot declarations; public headers gain no
legacy, Qt or Apple types. There is no new external dependency.

## Verification and limits

TDD starts with missing pure/native declarations. A separate pure regression first
accepted invalid UTF8; it now rejects through the shared UTF8 validator. Bounded
synthetic tests cover aliases, non-hex/zero identities, exact keys, first-reference
order, ignored assets, wrong kinds, ambiguity, spans, empty images, independent
storage, each policy's range and exact/short aggregate admission. These tests do
not parse malformed native archives or allocate large payloads.

Native tests use existing Capsule and ImportedSurface fixtures for successful
empty procedural inputs. A small positive Capsule-derived fixture adds one image
reference and embeds the existing synthetic ImportedMaterial.png unchanged; exact
encoded bytes and independent output are checked. Fixture provenance and hash are
in its README. No native writer, platform decoder, crash, allocation-failure or
pressure probe is used to create or verify it.

All 175 ordinary CTests pass (34.32 seconds), with 12 source/build/fixture hashes
stable across the final gate. SwiftPM runtime builds (12.73 seconds), after fixing
an initially missing private include path; inherited warnings and native-builder
deprecation remain. clang-format 18/22, strict manifest lint and diff checks pass.
Scoped pure and native metadata paths pass ASan/UBSan with leak detection disabled.
Pure resolver coverage is 96/96 lines, 90/90 regions, 7/7 functions and 80/80 branch
outcomes. Native adapter coverage is 44/53 lines (83.02%), 25/33 regions (75.76%),
4/5 functions and 13/22 branch outcomes. Defensive hierarchy/observer error and
allocation paths are not forced through malformed native input or fault injection;
this remains below infrastructure adoption targets. Instrumenting the maintained
reader for positive tests does not establish full parser/engine coverage or leaks.

Actual platform PNG/JPEG decoding, distinct procedural/imported pixel policies,
source-baked and legacy production/runtime transport, persistent session metrics,
Swift output ownership, Apple #280 supported-range parity and full M4–M8 acceptance
remain open. No opcode/default backend activation, production pin, quarantine or
process-containment claim changes in this stage.
