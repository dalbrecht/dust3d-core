// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_RESOURCES_BRIDGE_H_
#define DUST3D_NATIVE_WORKER_RESOURCES_BRIDGE_H_
#include "../Engine/WorkerResourceSetup.h"
#include <Dust3DWorkerResources.h>
namespace dust3d {
namespace worker {
    /// Shared bounded result mechanics used by the injected adapter and thin real-OS factory.
    Dust3DWorkerResourceResult initialWorkerResourceResult(ResourceRuntime runtime) noexcept;
    void recordWorkerResourceCause(Dust3DWorkerResourceResult& result, const char* message) noexcept;
    /// Canonical C-result adapter. A recording ResourceSystem performs no OS work in contracts.
    /// Real systems belong only to a fresh helper; partial setup always fails closed before work.
    Dust3DWorkerResourceResult applyWorkerResourceProfile(const Dust3DWorkerResourceProfile& profile, ResourceRuntime runtime, ResourceSystem& system) noexcept;
}
}
#endif
