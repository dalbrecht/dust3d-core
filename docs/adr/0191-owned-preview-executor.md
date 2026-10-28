# ADR 0191: Execute preview streams through owned XPC phases

## Decision

Add the explicit Dust3DWorkerPreview SwiftPM product. It composes the public Runtime,
Client and XPC products without depending on the service library. Immutable archive and
raw policy snapshots pass exact-revision scalar preflight before any transport. Authenticate
and retain the independent OS peer/generation before constructing job channels. One lifetime
binds one document, generation, raw cache budget and monotonic preview/reset request domain.

Move the existing inspected directional socket roles and original transfer diagnostics into
this canonical module; CMake QA consumers compile the same implementation directly. No Qt
or portable C++ source changes occur. Explicit stream flags are now available on XPC preview
and session submission; existing callers keep their file-backed default.

Each job owns two directional socket pairs. Dedicated GCD queues perform the blocking C
writer and semantic EOF decoder. The reader creates/copies/destroys the C output owner on
that queue, including throwing adapters. The control phase closes only the parent's transferred
role copies after the RPC returns. All three Result values join, retaining every original
phase/cleanup cause. Failure retires the lifetime and requests normal invalidation only after
control returns; it never closes an active reader/writer or interrupts native work. No automatic
reconnect, false completion deadline or control-error-as-drain assumption is added.

The existing pipeline retains current-source publication authority. Executor completion is
owned execution evidence only. Idle reset preserves the original legacy receipt domain;
normal retirement returns request/peer observations only. Independent supervision (#123)
must still establish native/process completion. A stuck call can retain one bounded slot.
Release trust/signing/embedding, Apple adoption, feature quarantines and M4–M8 remain open.

## Verification and limits

Nine importing XCTest cases cover owned source/scalar preflight and fake executor ordering,
held-reader joins, original multi-phase/preparation/retirement causes, no reconnect, identity
scope/replay/reset/single handshake, changed receipt peer, missing handshake observation and
original rejected reset. Fake callbacks perform no descriptor I/O or native parsing. Existing
XPC tests additionally verify explicit stream flags and the unchanged file default.

Matched instrumented XCTest/profile file summaries report executor220/231 line entries
(95.24%),87/94 regions(92.55%),17/19 functions; source preflight34/35 line entries(97.14%),
14/15 regions(93.33%),2/2 functions. Public constructor/factory and native channel/job paths
are not executed by these fakes; the latter report zero instrumented coverage. The one
address-size rejection on a 64-bit UInt64 host is not forced with allocation probes. These
are scoped LLVM reports, not whole-core, branch or native integration coverage. Retain the
matched executable/profile; do not combine profiles from different binaries.

The actual public-product consumer verifies copied finite Capsule geometry, component cache
reuse, reset/rebuild, current-source pipeline admission, handshake before channel creation,
joined writer/actual EOF/control and independent normal service PID disappearance. Source
hashes of158 core test files remained unchanged across the fresh guard/build and234/234
ordinary CTest pass64.17s. After additional fake receipt tests, the final fresh build and
234/234 CTest pass50.91s, including61 public XCTest cases, with23 changed source hashes
unchanged across validation. No malformed native, failed-peer, native cancellation, signal,
allocation-failure or resource-exhaustion probes occur; no sanitizer claim is inferred.

An initial QA assertion incorrectly required zero rebuilt components alongside reuse. Existing
per-component metrics permit both. That assertion failed after successful decoding and an
uncaught top-level Swift error trapped the QA host. The host now catches/reports errors and
exits normally; corrected healthy acceptance matches existing session semantics. Preserved
failed artifacts are not rerun. The subsequent healthy test passed2.40s, with independent
servicePID73189/hostPID73188 and normal service disappearance. This was a QA harness failure,
not evidence of a native generator crash or quarantine clearance.
