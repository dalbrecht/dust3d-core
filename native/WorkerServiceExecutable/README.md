# dust3d-xpc-service

Thin XPC executable over Dust3DWorkerService. Build with `swift build --product dust3d-xpc-service`
using the shared build root. It is launched by XPC from its containing bundle, not as a standalone CLI.
There are no runtime argument/environment trust overrides and no QA peer sidecar.

Stage an **unsigned** bundle with `python3 tools/stage_worker_service.py --help`, supplying the freshly
built executable, service identifier, explicit host requirement file and `--resource-policy-file` JSON plus explicit `--job-wall-milliseconds` (1...3600000). The requirement is copied into
Info.plist before signing; no requirement file is read by the running service. Bundle basename must be
`<identifier>.xpc`, executable is `dust3d-xpc-service`, configuration schema is 3. The entitlement file
contains App Sandbox only. Staging never signs, installs, launches or overwrites a bundle.

Release tooling must sign the service with the generated entitlements, embed it in its host, sign the
host, and enforce the service's trusted requirement from the client. Choose an appropriate stable host
designated requirement/Team ID for the actual distribution policy. An unsigned bundle or successful
syntax compilation is not trust, containment, notarization or release acceptance. Development QA
continues using its explicit private controller pins; do not ship that sidecar as a fallback.

The policy JSON must explicitly supply seven canonical unsigned decimal **strings** (no defaults):

```json
{"cpuSoftSeconds":"300","cpuHardSeconds":"600","fileSoftBytes":"67108864","fileHardBytes":"134217728","descriptorSoftCeiling":"1024","descriptorHardCeiling":"2048","minimumDescriptorCeiling":"256"}
```

These are example trusted lifetime requests, not universal sizing recommendations. Select the descriptor
floor for your helper startup needs; inherited stricter limits are preserved and must satisfy it. CPU caps
are cumulative process seconds, file caps are regular-file bytes, and core dumps are always disabled.
The policy is sealed into the service's own Info.plist before signing; there is no runtime policy-file,
argument, environment or archive override. Version 1 and 2 bundles are rejected; there is no missing-policy fallback.

Startup validates metadata, prepares and reads back all four resource limits, then checks its own
sandbox entitlement before listener/native work. A partial setup failure exits normally with original
metadata; it never retries or assumes rollback. This is no memory/RSS cap or complete containment claim. Normal
retirement waits for accepted work and native-owner cleanup; errors log original typed metadata without
archive diagnostic bytes. Signals, hard native cancellation and broad feature acceptance remain open.

Schema 3 also seals `Dust3DWorkerJobWallMilliseconds` as one canonical decimal string.
The scheduler starts this independent wall deadline at each admitted job, before queueing. It
checks the original end at dispatch/completion and uses an independent timer queue during native
work. Expiry or a checked clock failure seals admission and reports correlated scalar metadata
once. The dedicated executable logs metadata through unified logging and calls `_exit` without
waiting for native cleanup or process finalizers. That exit abandons the helper owner; it is not
successful drain/cancellation, a complete result, or permission to reconnect/publish stale output.
Logger/OS scheduling/exit are thin system boundaries, not real-time guarantees. No induced real
watchdog expiry/exit has been tested. Healthy fixture success and fake expiry are separate evidence.
Idle retirement cleanup and parent-side joins are not bounded by this per-job policy.
