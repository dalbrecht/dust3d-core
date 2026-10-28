# Dust3DWorkerService

Service-side Swift library owning the serialized native runtime behind the shared XPC contract.
The client transport never depends on this target. Build with `swift build --product Dust3DWorkerService`.

Create `WorkerServiceConfiguration(hostSigningRequirement:)` from explicit host trust configuration,
then `try WorkerServiceListenerDelegate(configuration:onRetired:)`. Assign the delegate to an
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
`WorkerServiceBundleMetadata` validates copied own-bundle values without Security/native
calls; `WorkerServiceFailureMetadata` formats typed metadata without archive diagnostics.
