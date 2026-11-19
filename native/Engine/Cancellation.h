// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_CANCELLATION_H_
#define DUST3D_NATIVE_CANCELLATION_H_

#include "CancellationToken.h"
#include "EngineInternal.h"

namespace dust3d {
namespace native {
    inline void checkCancellation(const Dust3DCancellationToken* token)
    {
        if (token && token->cancelled.load(std::memory_order_relaxed))
            throw DocumentError(DUST3D_STATUS_CANCELLED, "The native operation was cancelled.");
    }
}
}
#endif
