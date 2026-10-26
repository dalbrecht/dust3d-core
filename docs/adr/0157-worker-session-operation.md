# ADR 0157: Correlated session generation producer

Status: proposed for reviewed delivery. Tracks #251.

## Decision

Add protocol operation 9, generateSessionIndexedSourcedMesh, for an explicit C++
producer supplied with an existing ConnectionGenerationSession and trusted host
connection lifetime. Request bytes never select or create a cache owner. Require
an exact request kind, operation, identity and body length before execution. The
body reuses the complete source-budget request codec: zero requests the native
64 MiB default, and an explicit budget can only tighten the host source allowance.
The opaque archive is copied only after request framing/options admission.

Generate through the existing serialized session. Return a correlated response
whose body is the atomic SessionOutput envelope, including primary failure or empty
outcomes and actual final-call cache observations. Source fallback remains inside
the same worker owner; no parent/stateless fallback occurs. Protocol, request,
scope, copy and serialization errors retain their typed causes without a partial
frame. Request/revision correlation in this producer is not authentication or host
current-revision publication admission.

## Diagnostic reservation and limits

Validate all output policies and detailed diagnostic prefix capacity before native
work. Reserve worst-case source and statistics diagnostics before assigning geometry
allowance, so secondary diagnostic storage does not unexpectedly consume that
allowance after generation. With Fw/Fc the failure wire/cause limits and Sw/Sc the
statistics wire/cause limits, reserve payload wire bytes as
64 + Fw + max(Sw, 64 + Fw), and logical decoded bytes as
Fc + 38 + max(Sc, Fc + 38). The 38-byte resource identity maximum is shared with the
failure codec. Remaining allowance must also retain a primary failure.

Both ordinary and sourced geometry quotas are explicitly tightened to those
remaining aggregate allowances; independent inner policies remain enforced.
These conservative reservations reduce the available geometry payload, exclude
roots/capacity/workspace/RSS and do not establish Apple supported-range parity.
Apple #280 remains a deployment gate. The outer frame header is separate from
its payload quota. Insufficient policy rejects before generation.

Bound all moved primary/source/statistics diagnostics to the admitted failure
policy, preserving original classification and explicit truncation. Originally
unclassified diagnostics remain unclassified unless a truncation marker is needed.
This closes the tighter-host-quota gap discovered during producer integration.

## Explicit unsupported paths

Operation 9 framing does not enable it in the stateless channel server, legacy
PendingReply/ServiceReply semantic admission or legacy C decoder. Those paths reject
it explicitly, including failure frames. Otherwise the generic base mesh/failure
branches could misinterpret a session envelope and lose its statistics/diagnostics.
The new diagnostic operation name is a static content-independent literal. No new
C enum/capability, serving entry point, decoded owner, Apple backend or pin is enabled.

## Verification

The positive operation contract began with a missing-header red test. Separate
red regressions demonstrated tighter host diagnostic limits were not applied to
all moved causes, and legacy C admission accepted a session failure frame. Both
are fixed. Full CMake build and all 200 ordinary CTest contracts pass (39.55 s),
as do the SwiftPM runtime product (0.69 s), strict manifest lint, quarantine guard
and pinned clang-format 18 checks. Seventeen source/build/test hashes match the
validated inputs.

The licensed Capsule test exercises default and explicit positive source budgets,
exact response operation/document/request/revision, actual second-call cache reuse
and owned output after session close. Pure preflight tests reject wrong kind,
operation/body length and inadequate diagnostic policies before native work.
Valid session frames reject through stateless channels without output/native
execution. Tiny framed messages verify legacy C and PendingReply rejection for
both response and failure kinds. No malformed native archive or fault probes.

Scoped ASan/UBSan passes for the producer/copy adapter using ordinary Capsule input
and tiny pure diagnostic values. Producer coverage is 44/44 lines, 23/23 regions,
3/3 functions and 14/14 branch outcomes. Copy adapter measures 69/70 lines,
49/50 regions, 5/5 functions and 34/34 branch outcomes; its allocation-failure catch
remains unforced. Native engine libraries are uninstrumented; leak detection is
disabled. This does not claim whole-engine coverage or process containment.

## Remaining adoption

Dedicated C ownership/getters and serving lifecycle, authenticated service/queue
admission, Swift/XPC ownership and platform decoding remain required. Host current
identity must gate publication. No production backend/pin or quarantine changes.
This does not close #251, Apple #279/#280 or M4–M8 acceptance. Completed clean
exact-head review and every CI check are required before merge.
