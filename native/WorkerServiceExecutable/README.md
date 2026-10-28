# dust3d-xpc-service

Thin XPC executable over Dust3DWorkerService. Build with `swift build --product dust3d-xpc-service`
using the shared build root. It is launched by XPC from its containing bundle, not as a standalone CLI.
There are no runtime argument/environment trust overrides and no QA peer sidecar.

Stage an **unsigned** bundle with `python3 tools/stage_worker_service.py --help`, supplying the freshly
built executable, service identifier and explicit host requirement file. The requirement is copied into
Info.plist before signing; no requirement file is read by the running service. Bundle basename must be
`<identifier>.xpc`, executable is `dust3d-xpc-service`, configuration schema is 1. The entitlement file
contains App Sandbox only. Staging never signs, installs, launches or overwrites a bundle.

Release tooling must sign the service with the generated entitlements, embed it in its host, sign the
host, and enforce the service's trusted requirement from the client. Choose an appropriate stable host
designated requirement/Team ID for the actual distribution policy. An unsigned bundle or successful
syntax compilation is not trust, containment, notarization or release acceptance. Development QA
continues using its explicit private controller pins; do not ship that sidecar as a fallback.

Startup validates metadata and checks its own sandbox entitlement before listener/native work. Normal
retirement waits for accepted work and native-owner cleanup; errors log original typed metadata without
archive diagnostic bytes. Signals, hard native cancellation and broad feature acceptance remain open.
