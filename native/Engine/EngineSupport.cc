// SPDX-License-Identifier: MIT
#include "EngineSupport.h"
#include <cstdio>
namespace dust3d {
namespace native {
    void setError(Dust3DError* error, Dust3DStatus status, const char* message) noexcept
    {
        if (!error)
            return;
        error->status = status;
        std::snprintf(error->message, sizeof(error->message), "%s", message);
    }
    std::mutex& engineMutex()
    {
        static std::mutex mutex;
        return mutex;
    }
}
}
