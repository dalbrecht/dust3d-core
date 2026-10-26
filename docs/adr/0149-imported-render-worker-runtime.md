# ADR 0149: Imported rendering through the explicit worker runtime

Status: Accepted for explicit internal use; Apple adoption pending.

## Context

Issue #243 connects the same-archive producer and C image callback to framed
worker jobs, semantic admission and independent C output ownership. Imported
rendering must preserve complete geometry, source catalogs, authored materials,
encoded images and decoded pixels, with successful absence distinct from failure.

## Decision

Opcode 6 is `generateImportedRender`; existing opcodes 1–5 retain their values.
Its version-1 request has an 80-byte big-endian prefix: version u16 at 0, prefix
size u16 at 2, reserved zero u32 at 4, archive length u64 at 8, and eight u64
budgets at 16–72. The opaque archive follows exactly, with no trailing bytes.
Budgets name material-native, surface-native, facts-copy, surface-copy, encoded
image, decoded pixel, aggregate decoded and output wire bytes, respectively.
Zero selects maintained defaults; positive values through 256 MiB can only tighten
receiver policy. Inner geometry/metadata/material copy limits also tighten to the
aggregate policy. Logical request accounting charges archive bytes, excluding root
objects, capacity, temporary workspace and RSS. Framing and options validate before
retained archive copying or native execution.

The dedicated C request factory owns the full immutable frame. Its exact-layout
options struct is optional; the generic factory selects the same default codec
and rejects unrelated UV options. The output-wire request budget is additionally
clamped to the caller's authoritative frame policy. No archive is parsed by a
request factory. All C allocation boundaries catch exceptions and retain original
typed diagnostics. No STL types or exceptions cross C.

The C++ job entry point accepts an optional portable decoder. The explicit C
`dust3d_worker_serve_with_image_decoder` validates and copies a callback configuration
for one exclusive synchronous job; context and code remain borrowed through return.
The ordinary entry point installs no decoder and returns explicit unsupported
failure for this opcode before native generation. There is no parent fallback or
global decoder registration. With a provider, the same unchanged archive supplies
native facts and surface, then the producer prepares complete imported output.
Successful absence is an empty response body; a nonempty response is a complete
joined rendering payload. Framing completion alone never implies rendering success.

Child-exit, service and queue semantic admission retain their existing identity,
EOF and completion checks. Only after those checks does opcode 6 decode a bounded
optional imported rendering value. C policies clamp joined and inner logical/wire
budgets, decoded pixels and material spans to the authoritative caller policy.
The current C semantic hard maximum remains 128 MiB; this stage does not resolve
Apple #280 supported-range parity.

A distinct C decoded kind survives successful absence. Imported getters expose
exact counted identity bytes, component membership kinds, per-corner UV/colors,
material factors, sampler/filter presence, encoded image bytes and tight straight
RGBA8 source pixels. Geometry accessors accept imported replies. Borrowed spans
expire at decoded-owner destruction; Swift must copy before release. Invalid
kind/index/output guards clear outputs. Pixel counts describe actual retained
pixel storage, not native generation/cache metrics. The summary uses explicit
variant checks, avoiding an assumption that every unfamiliar variant is failure.

Failure v1 bytes remain unchanged when no details exist. Optional details select
v2: 24-byte prefix with version u16=2, prefix u16=24, outcome u16 at 4, domain u16
at 6, signed code bits u32 at 8, cause length u32 at 12, original status u32 at 16
and truncation flag u32 at 20. Domains distinguish native, image decoder, producer,
surface, materials, render and standard exceptions. Cause bytes remain opaque,
including NUL/non-UTF8; original signed code, status and explicit truncation survive
callback-to-frame-to-owned-result conversion. Existing failure getters remain
valid; a separate accessor returns optional details. Failure budgets account for
the larger prefix. Domain/flag/layout/length validation precedes retained copying.

## Verification and limits

TDD began with missing operation, request, admission and C API declarations.
Tiny pure fixtures cover explicit request framing and all eight tightening budgets,
exact limits, invalid scalar layouts, successful absence, typed opaque failures,
child/service/queue routing, immutable ownership and C getter guards. Existing
ImportedSurface and Capsule archives with a fake one-pixel callback verify C
request-to-descriptor-job output using owned regular files inside the shared build
root. No actual PNG/JPEG correctness or new malformed-native-input claim follows.
Strict C consumers compile and exercise the request factory and output declarations.

All 173 ordinary CTests pass (33.66 seconds); 42 changed source/build-spec hashes
remain stable across the final gate. SwiftPM builds the runtime (28.97 seconds),
with existing inherited warnings and the native-builder deprecation. Changed C/C++
files pass clang-format 18 and 22; the Swift manifest passes strict formatting lint.
Selected pure codecs/admission and instrumented C bridges pass ASan/UBSan with leak
detection disabled. Per-binary coverage is 91/91 lines, 57/57 regions, 11/11 functions
and 42/42 branch outcomes for the new request codec; imported getters reach 107/107
lines, 84/84 regions, 10/10 functions and 58/58 branch outcomes. This is scoped
evidence, not full engine/runtime coverage. Native and allocation/unknown-exception
catch paths remain unprobed; no quarantined fault injection is used to raise coverage.
An auxiliary legacy failure-test sanitizer link omitted native libraries and did
not execute; its ordinary CTest passes in the full linked gate.

Actual platform PNG/JPEG decoding inside the Apple worker, Swift orchestration,
source-baked/legacy transport, persistent session/cache/native metrics, supported
range parity and full M4–M8 acceptance remain open. Callback allocation compliance
is a provider obligation; receipt checks cannot undo a faulty provider's allocation.
There is no RSS, process-containment or distribution ABI stability claim. Production
pin, default editor backend and all feature quarantines remain unchanged.
