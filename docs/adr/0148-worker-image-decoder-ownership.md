# ADR 0148: C ownership boundary for worker image decoding

Status: Accepted for explicit internal adapter use; Apple/runtime adoption pending.

## Context

Issue #241 follows the injected imported-render producer (#239). A Swift platform
decoder needs a C-compatible callback and explicit pixel ownership without STL,
C++ exceptions, borrowed output storage or platform types crossing the boundary.

## Decision

`Dust3DWorkerImageDecoder.h` is a standalone C11 header. `Dust3DWorkerImage` is an
opaque independent tight straight RGBA8 owner. Creation validates positive image
shape and byte quota (positive, at most 256 MiB) before copying pixels. Getters
return scalar dimensions/count; bounded range copying exposes no borrowed pointer.
All copy guards run before writes. Empty range at end permits null destination;
null getters return zero and null destruction is safe. Required result storage is
reset on entry. All C entry points are noexcept and catch allocation/other C++
exceptions where allocations can occur; original available diagnostics survive.
No fault injection is used to exercise those catches.

`Dust3DWorkerImageResult` owns status, signed original code, explicit cause length,
truncation and 512 opaque bytes. Embedded NUL/non-UTF8 are preserved. The failure
factory copies at most 512 actual readable bytes without allocation and marks
truncation. Success requires zero code/length/truncation. This is an in-process
receipt, not a serialized wire struct; header version 1 is not a distribution ABI
stability or feature-negotiation claim.

A callback receives immutable actual readable encoded bytes and exact UTF-8 model
identity, encoding PNG1/JPEG2 and independent input/output quotas. Input is borrowed
only for the synchronous call. Configuration has an exact struct-size check,
required callback and borrowed caller context; no global registration or retain
hooks. Context/code must outlive exclusive use. Callback implementation must enforce
quotas before allocation, return source-scanline straight RGBA8 without orientation,
color conversion or premultiplication, and must not throw across C/Swift.

Ownership of any returned image transfers to `CallbackImportedImageDecoder`, even
when the callback reports failure. The C++ adapter owns it immediately, validates
receipt layout, preserves original bounded failure status/code/bytes, rejects
inconsistent success, enforces returned output quota and shape, then moves pixel
storage into the producer value and destroys the emptied handle. Failure paths
destroy the returned owner. Callback input/identity values and policies validate
before invocation. `WorkerImageDecoderError` retains the full original receipt;
its textual `what()` is supplementary and cannot represent opaque embedded NUL
bytes completely.

The adapter is explicitly worker-side. This stage does not register a callback
with an opcode, invoke the producer through C, install ImageIO or select a default
backend. A future worker entry point must carry the configuration and the Apple
worker supplies the platform decoder. No parent decoding/native fallback is added.

## Verification and limits

TDD started with a missing-header failure. Tiny synthetic tests cover independent
pixel ownership, transparent RGB, getters/ranges/nulls, exact/short quotas, image
shape, C configuration/input guards, PNG/JPEG declarations, owned callback results,
failure with/without returned owner, opaque original errors, explicit truncation,
invalid receipt layout, inconsistent success and over-budget returned pixels.
A strict C11 consumer compiles, links and copies an owned pixel. These are not
actual PNG/JPEG decoding, malformed archive or native fault probes.

All 166 ordinary CTests pass (34.46 seconds), with seven source/build-spec hashes
stable across the final gate. SwiftPM runtime builds (0.63 seconds), retaining the
existing native-builder deprecation. clang-format 18 and 22 and manifest lint pass.
Selected bridge and pure codec code passes ASan/UBSan with leak detection disabled.
Named bridge coverage: 83/89 lines (93.26%), 116/119 regions (97.48%), 12/12
functions and 86/86 branch outcomes. Unexecuted lines are allocation/generic
exception catches; coverage remains below the infrastructure line adoption target.
Do not induce allocation failures or other quarantined faults to increase it.
No full inherited-engine, leak, process containment or RSS guarantee follows.

Limits exclude roots/capacities/allocator overhead, callback/platform workspace
and process RSS. A callback must honor its supplied allocation quota; receipt
checks cannot undo allocations already made by a faulty implementation. Actual
Apple decoder integration, runtime transport/C/Swift orchestration, real session
metrics, supported-range parity and full M4–M8 acceptance remain open. Production
pin and all feature quarantines are unchanged.
