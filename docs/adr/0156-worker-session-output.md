# ADR 0156: Atomic worker session output

Status: proposed for reviewed delivery. Tracks #251.

## Decision

Represent the final session outcome as exactly one sourced/indexed pair, ordinary
indexed mesh, successful emptiness, or primary failure. Retain original source and
statistics issues independently. A source issue is permitted only for fallback
outcomes and original source status3/4. Status5 is canonical successful emptiness,
not a primary failure. Available statistics cannot coexist with a statistics issue;
unavailable observations never manufacture zero counts.

Version1 has a64-byte big-endian prefix: versionu16@0, prefixsizeu16@2, outcomeu32@4
(sourced1, ordinary2, empty3, failure4). Fouru64 section lengths at8/16/24/32 describe
geometry-or-primary-failure, optional source issue, explicit optional-statistics
codec and optional statistics issue. Bytes40–63 are reserved zero. Sections follow
in that order; emptiness requires a zero-length first section. Statistics always
carry the existing availability codec, including an absent value. Existing detailed
failure codecs preserve original opaque causes and resource context.

Independent inner quotas remain enforced alongside aggregate wire/logical quotas.
Logical charge includes geometry/hints, all diagnostic cause bytes and resource
identity bytes; excludes root/scalar storage, capacity, temporary encoding workspace
and RSS. Cause/context are charged separately with subtraction checks. Decode
validates full envelope framing and bounded diagnostic sections before retaining
geometry arrays; their aggregate charge tightens the remaining geometry quota.
Diagnostic copies are independently bounded before aggregate admission. Typed inner
errors propagate. This codec neither authenticates provenance nor adds an opcode.

The native-output adapter moves owned results, requires one final outcome and rejects
conflicting observations. It explicitly maps process-local statistics exceptions:
original NativeSessionError payloads retain their classifications; statistics codec
errors use additive failure domain15 with their original code; standard errors retain
bounded what() bytes. Unknown nonstandard exceptions have explicit unavailable-cause
classification. Causes truncated to admitted diagnostic quotas carry a truncation
flag. Originally absent details remain absent unless truncation requires an explicit
marker. No exception/STL type crosses the existing C ABI. Callers must admit diagnostic
policy before generation; mapping/serialization allocation failures can still throw.
Old clients may reject new domain15; deployment remains gated on reviewed integration.

## Verification

TDD began with missing-header contracts. The full CMake build and all199 ordinary
CTest contracts pass (41.47s), as do SwiftPM Dust3DWorkerRuntime (0.74s), strict manifest
lint, quarantine guard and pinned clang-format18. Tiny pure tests cover all outcome
families, ownership, both failure causes, availability/zero distinction, detailed
classification, truncation, framing, invalid combinations and quota rejection.
The licensed Capsule session test round-trips actual generated output and final-call
statistics through the envelope, alongside existing reuse/reset/close checks.

Scoped ASan/UBSan passes for pure codecs, ordinary diagnostic mapping and positive
Capsule output with the new codec/copy adapter instrumented. Native engine libraries
remain uninstrumented; leak detection is disabled. Matching profiles measure codec
145/146 lines,119/120 regions,13/13 functions and90/98 branch outcomes; its defensive
valueless-variant guard remains unforced. Copy adapter measures65/66 lines,45/46
regions,5/5 functions and30/30 branch outcomes; allocation-failure catch remains
unforced. This is scoped evidence, not whole-engine coverage. No new malformed
native input, allocation-failure, crash, signal, cancellation or exhaustion probes.

## Remaining adoption

Host-owned connection lifetime, correlated request/result frames, C ownership and
getters, Swift/XPC consumption and platform decoding still require integration.
The production backend/pin, all feature quarantines and single-cache policy remain
unchanged. This does not close #251, Apple #279/#280, supported-range parity or full
M4–M8 acceptance. Completed clean exact-head review and all CI precede merge.
