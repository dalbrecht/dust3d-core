// SPDX-License-Identifier: MIT
#include "../Engine/WorkerBuild.h"
#include "WorkerResourcesBridge.h"
#if defined(__APPLE__) || defined(__linux__)
#include "../Engine/WorkerUnixResources.h"
#endif
#include <exception>

using namespace dust3d::worker;
// Only this fresh-helper boundary constructs a real OS adapter; the shared result logic stays injectable.
extern "C" Dust3DWorkerResourceResult dust3d_worker_resources_apply(Dust3DWorkerResourceProfile profile) noexcept
{
    const auto runtime = compiledResourceRuntime();
#if defined(__APPLE__) || defined(__linux__)
    try {
        SystemUnixResourceCalls calls;
        UnixResourceSystem system(calls);
        return applyWorkerResourceProfile(profile, runtime, system);
    } catch (const std::exception& error) {
        auto result = initialWorkerResourceResult(runtime);
        recordWorkerResourceCause(result, error.what());
        return result;
    } catch (...) {
        auto result = initialWorkerResourceResult(runtime);
        recordWorkerResourceCause(result, "Unknown service resource adapter construction failure");
        return result;
    }
#else
    auto result = initialWorkerResourceResult(runtime);
    result.status = DUST3D_WORKER_RESOURCE_UNSUPPORTED;
    recordWorkerResourceCause(result, "Service process resource setup is unsupported on this platform");
    return result;
#endif
}
