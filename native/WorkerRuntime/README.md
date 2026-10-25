# Dust3DWorkerRuntime

Private Unix worker-host C interface to the maintained descriptor job runtime.
Build with CMake or the `Dust3DWorkerRuntime` SwiftPM static product; the
`dust3d-worker` executable depends on the same runtime target. No additional
package or external dependency is introduced.

Import `Dust3DWorkerRuntime.h` from C or `Dust3DWorkerRuntime` from Swift. Check
`dust3d_worker_runtime_abi_version()`, then call `dust3d_worker_serve` on a non-UI
worker with separate borrowed readable/writable job descriptors. The host must
own startup resource/SIGPIPE/stdio isolation and channel cleanup, serialize jobs,
and discard partial output on noncomplete results. A complete frame can carry a
native failure; no process-exit or geometry-success assertion is implied.

Results own all fields and bounded diagnostic bytes inline. C++ exceptions never
cross the C boundary; original category/code/errno/context remain available.
No native object owner or result destroy function is needed. This interface does
not launch, authenticate, sandbox, cancel, save or alter global stream policy.
See the public header for exact field and lifetime contracts. Tests use ordinary
licensed static fixtures and small pre-generation codec/preflight rejections;
all existing quarantines remain in place. Binary ABI stability is not claimed
before the project's release gates; consume an exact reviewed source revision.
