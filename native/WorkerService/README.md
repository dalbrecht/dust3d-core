# Dust3DWorkerService

Service-side Swift library owning the serialized native runtime behind the shared XPC contract.
The client transport never depends on this target. Build with `swift build --product Dust3DWorkerService`.

Create `WorkerServiceConfiguration(hostSigningRequirement:)` from explicit host trust configuration,
then, **only in a fresh dedicated service process**, call
`WorkerServicePreparedResources.prepareFreshHelper(policy:)` with explicit trusted policy. This changes
that process's limits and verifies exact readback before returning a receipt. Never invoke it from an
app or test runner. Pass that receipt to `WorkerServiceListenerDelegate(configuration:resources:onRetired:)`.
Assign the delegate to an
`NSXPCListener.service()` and retain it through listener operation. Initialization checks this
process's App Sandbox entitlement before any native work. The embedding executable owns its signed
bundle, listener lifetime, logs and normal process termination after the retirement callback.
The library never exits the embedding process. Configuration syntax validation alone does not prove
release trust; production Team ID/signing and sealed bundle configuration remain separate release work.

The QA launcher alone reads private development CDHash pins. CMake smoke tests compile the same canonical
service sources; SwiftPM exposes the actual reusable module. No duplicate adapter is maintained.
Legacy and segmented profiles share one native owner. Imported/GLB rendering remains quarantined;
only the existing procedural image callback is enabled. No native cancellation or automatic restart.

The `dust3d-xpc-service` product supplies the thin bundle-only bootstrap. See
`../WorkerServiceExecutable/README.md` for unsigned staging and release limitations.
Configuration schema 2 requires a `Dust3DWorkerResourcePolicy` dictionary of seven canonical decimal
strings: `cpuSoftSeconds`, `cpuHardSeconds`, `fileSoftBytes`, `fileHardBytes`,
`descriptorSoftCeiling`, `descriptorHardCeiling`, and `minimumDescriptorCeiling`.
CPU is cumulative process time, files are regular-file bytes, descriptors are a descriptor-number
ceiling (not an open-file count); the explicit floor must remain usable under inherited stricter caps.
Core dumps are always disabled. There is no address/RSS cap, wall deadline, rollback or full containment
claim. Any setup/readback failure preserves original operation/resource/errno and partial progress;
the executable reports metadata and exits normally before starting the listener. Do not retry partial setup.

`WorkerServiceBundleMetadata` validates copied own-bundle values without Security/native
calls; `WorkerServiceFailureMetadata` formats typed metadata without archive diagnostics.

Handshake ownership replies also retain one process-scoped128-bit incarnation. This Foundation
UUID-derived value is correlation metadata, not authentication; the configured code requirement
remains authoritative. Rebuild the exact-revision client/service together for the expanded scalar
reply. It creates no native lease, consumes no job request and exposes no document diagnostics.
