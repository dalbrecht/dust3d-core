# Dust3DWorkerPreview

Parent-side SwiftPM preview execution over Runtime, Client and XPC. Import this product in a
trusted host, construct `WorkerXPCConfiguration` with its embedded service identifier and
signing requirement, then create `try WorkerXPCPreviewExecutor<Value>(configuration:registrationTimeoutMilliseconds:handshakeTimeoutMilliseconds:copy:)`.
Pass it to `WorkerPreviewPipeline` and submit host-captured identities and immutable
`WorkerPreviewSource` snapshots. No development trust defaults are installed by the product.

The source validates scalar archive/native/chunk/cache policies before transport. Archive
bytes remain opaque until the service parses them. Data must own its storage independently;
never supply mutable externally borrowed storage. Raw zero/default native policies and
addressable indexed/transfer budgets are preserved. This is not legacy protocol normalization.

The executor authenticates a peer, registers a public process-exit source and completes a
fresh matching PID/incarnation handshake before creating directional socket channels.
The explicit registration and per-handshake budgets are each 1...10000ms; invalid policy rejects
before transport creation. Both initial and fresh barrier handshakes receive the same explicit
handshake budget. A deadline failure retains its original attempt/cause and prevents the job factory. Dedicated
writer/reader queues perform C archive writing and semantic reply decoding through actual EOF.
The synchronous `copy` callback receives a borrowed immutable C session-output owner on the
reader queue: copy all required values before returning; never retain, destroy or expose it.
The owner is destroyed there even if copying throws. Native generation remains service-side.

Writer, reader and original control receipt all join before completion or failure returns.
Original phase/cleanup causes are retained. An error seals the lifetime; invalidation is
requested only after control returns, and remaining owned phases still join. There is no
automatic reconnect or native task cancellation. One document/generation/raw-cache scope and
monotonic request sequence span previews and `reset(request:)`. Reset is idle-only.
`retire()` requests normal invalidation when idle and reports the captured peer; this is
**not proof of native drain or process disappearance**. The supervising host must establish
process disappearance independently. `retireAndObserve(timeoutMilliseconds:)` requests idle
invalidation and waits up to its explicit 1...10000ms observation budget for the pre-registered,
handshake-bracketed exit event. Registration, exit, original failure and expiry remain separate.
A failed barrier can never produce authenticated exit evidence. Expiry retains its original cause,
seals observation and does not stop a process, close active channels or authorize reconnect.
Captured monotonic timestamps enforce the original observation deadline despite delayed timer
delivery; early timer wakeups never extend it. Scheduling is not real-time. A stuck native invocation can still retain one slot.

`WorkerPreviewPipeline` independently admits current-source publication. UI callers must
recheck their captured identity on the main actor before applying the copied result.
Executor success alone never permits stale UI publication.

Build with `swift build --product Dust3DWorkerPreview`. Tests under `tests/WorkerPreview`
use fake phases for negative/lifecycle paths. `worker_preview_executor_product` imports the
actual product and exercises only the healthy licensed Capsule through a fresh owned signed
QA service, checking geometry, cache reuse/reset/rebuild and independent normal PID exit.
These QA pins/entitlements are not a release trust policy. See ADR0191/ADR0192 for measured evidence
and explicit coverage limits. No Apple production route/pin changes or quarantine removals
are included. Supervision, release embedding/trust and full editor acceptance remain open.
