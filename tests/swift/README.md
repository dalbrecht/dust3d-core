# Swift boundary consumers

These CMake-built fixtures verify the portable core's C APIs using strict Swift6
concurrency and copied value ownership. They are test code, not an Apple app SDK
or a second native adapter. No Xcode project or additional dependency is needed.

Configure/build the core in the existing shared build root, then use CTest to run
the registered consumers. The ordinary macOS inventory additionally builds
`dust3d_worker_xpc_static_fixture` and runs `worker_xpc_static_mesh`: one licensed
Capsule job in a private, freshly signed host/XPC service bundle. It needs the
macOS Foundation/Security SDK, codesign and Python3. The controller provisions
explicit development peer hashes; no credential or installed app is changed.

The XPC fixture passes FileHandles and scalar receipts, collects the reply through
the shared bounded C parser, and copies validated geometry. Temporary bundles
are deleted only after host exit and service PID disappearance; failures retain
artifacts/live processes and never force termination. Keep one shared build root,
preserve protected QA apps, and run the source quarantine audit before native
execution. See [ADR0120](../../docs/adr/0120-isolated-xpc-static-host-verification.md)
for evidence and limits. Live pipes, cancellation, production trust/sandbox and
publication admission remain unverified; all existing quarantines remain.

The companion `worker_xpc_uv_mesh` case uses the same shared C request encoder
and isolated service to verify 240 vertices/80 UV triangles. Both XPC cases share a
CTest resource lock while recycling their development bundle IDs. They still use
regular files and establish no live-pipe, sandbox or publication acceptance.
