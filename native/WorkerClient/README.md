# Dust3DWorkerClient

Swift6 client scheduling and immutable-source ownership, without an engine or transport
dependency. Import the `Dust3DWorkerClient` static SwiftPM product at an exact reviewed
source revision. It does not launch a process, parse archives, perform XPC, publish UI
state, authenticate a peer or bound native RSS/CPU. The Apple dependency pin is unchanged.

## Use

Create `WorkerPreviewSourceCoordinator<Snapshot>` on one serialized collecting owner.
Snapshot must be an independently owned immutable Sendable value containing archive and
captured policies, never borrowed native memory. Submit a host-created
`WorkerPreviewIdentity`; begin returns one active identity. Retrieve its snapshot with
`source(for:)` for isolated work. A replaced waiting identity releases its retained source;
active source remains held until collected channel cleanup and complete/abandon.

After validating reply framing/semantics through actual EOF and observing the authenticated
peer, construct the passive `WorkerPreviewControlReceipt` from original callback fields
and independent host observations, then call complete. New source submissions retire older
publication through identity comparison. Call cancel/close to retire publication, not to
interrupt native work. Retain and surface the original transport error when abandoning.
No operation may reenter/concurrently access this collecting-thread coordinator.

## Development

The canonical source is here; CMake fixtures compile/import this same module and direct
pure contracts use these files. No duplicate implementation remains under tests/swift.
Run SwiftPM `swift test --build-system native --filter WorkerClientPackageTests` with the project's explicit
shared scratch/cache paths, or run CTest's worker_client_swiftpm contract. The explicit
backend reuses the established cache layout; its deprecation warning is retained rather
than silently switching build backends. The public
XCTest cases import the package module; other pure contracts cover lifetime, queue and
admission state. Native XPC fixtures use only existing healthy licensed inputs.

No external dependency was added. Public API stability, a production transport/worker
bundle and full editor parity are not claimed; see docs/editor-port-plan.md in the root.
