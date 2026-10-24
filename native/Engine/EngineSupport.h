// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_ENGINE_SUPPORT_H_
#define DUST3D_NATIVE_ENGINE_SUPPORT_H_
#include "include/Dust3DBridge.h"
#include <mutex>
namespace dust3d {
namespace native {
    /// Write an optional fixed diagnostic buffer without throwing or exposing allocator ownership.
    void setError(Dust3DError* error, Dust3DStatus status, const char* message) noexcept;
    /// Shared serialization for native construction/generation; inherited globals are not proven thread-safe.
    std::mutex& engineMutex();
}
}
#endif
