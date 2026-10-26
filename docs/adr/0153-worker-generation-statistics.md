# ADR 0153: Owned worker generation statistics

Status: Statistics foundation; persistent worker adoption pending.

## Context

Issue #251 follows source-baked worker delivery. Apple currently uses an actual
native generation session with cache diagnostics. Stateless worker output does
not establish equivalent metrics. Unavailable observations must not become
fabricated zero counts, and cache-only issues must stay separate from primary
geometry outcomes.

## Decision

`GenerationStatistics` retains configured/retained cache payload bytes, actual
reused/rebuilt component counts, rebuilt part count, cache outcome, signed numeric
cache status and opaque original diagnostic bytes. The containing optional models
availability. No codec or identity association proves the observation's provenance.

Version1 uses a 64-byte big-endian prefix: version u16 at0, prefix size u16 at2,
availability u32 at4, five u64 fields at8–40 (configured bytes, retained bytes,
reused components, rebuilt components, rebuilt parts), outcome u32 at48, signed
status bits at52, and cause length u64 at56. Exact cause bytes follow. Absent values
require every field after availability to be zero, including an empty cause;
measured all-zero observations retain availability1. Current outcomes are0–4;
retained payload cannot exceed configured payload, whose supported maximum is
256MiB. Counts remain unsigned64 without invented native work limits.

Independent positive cause/wire quotas validate before allocation/copy. Defaults
are4000/4096 bytes; hard diagnostic envelope is64KiB. Quotas describe storage,
excluding roots/capacity/allocator/workspace/RSS. Complete framing, scalar validity
and quota admission precede retained decoded diagnostic allocation.

The native snapshot adapter copies actual `Dust3DGenerationStats` values with
checked representation assumptions and bounded terminated message inspection.
It shares codec scalar admission and preserves cache-only status/cause. Callers
must serialize generation and statistics queries for their session; this helper
neither owns a session nor authenticates its call association. No platform types
enter the portable codec target.

## Verification and remaining work

TDD began with missing-header tests. The complete ordinary build and193 CTests
pass (45.14s); strict Swift manifest lint, exact clang-format18 and the SwiftPM
runtime product (0.68s) pass. Scoped ASan/UBSan checks cover the pure codec and
adapter with an existing positive Capsule session: two sequential generations,
actual component reuse, exact native-to-owned statistics and explicit reset.
Constructed cache diagnostics verify status/opaque bytes and copy admission.
No malformed archive, native fault, crash or pressure probes are added.

Matching profiles measure codec76/76 lines,60/60 regions,8/8 functions and46/46
branch outcomes. Adapter19/20 lines and4/6 outcomes are covered; defensive missing
termination and a separate quota condition remain unforced. Native engine static
libraries are not instrumented; leak detection is disabled. This is not whole-core
coverage, process containment or visual/performance acceptance.

Persistent worker session lifecycle, same-call geometry/source/statistics output,
request/reply/C ownership, real host admission and Apple integration remain open
under #251/#279. No opcode/default backend or production pin change occurs here.
All existing feature quarantines and the single shared build cache remain.
