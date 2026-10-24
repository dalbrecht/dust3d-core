// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_BUILD_H_
#define DUST3D_NATIVE_WORKER_BUILD_H_
#include "WorkerResources.h"

namespace dust3d {
namespace worker {
    /// Detect this translation unit's instrumentation. Build/packaging must still
    /// guarantee consistent helper flags; this is not signed binary authentication.
    inline ResourceRuntime compiledResourceRuntime() noexcept
    {
#if defined(__SANITIZE_ADDRESS__)
        return ResourceRuntime::addressSanitized;
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
        return ResourceRuntime::addressSanitized;
#else
        return ResourceRuntime::ordinary;
#endif
#else
        return ResourceRuntime::ordinary;
#endif
    }
}
}
#endif
