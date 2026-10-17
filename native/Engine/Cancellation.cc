// SPDX-License-Identifier: MIT
#include "Cancellation.h"
#include <new>

Dust3DCancellationToken* dust3d_cancellation_token_create(Dust3DError* error)
{
    dust3d::native::setError(error, DUST3D_STATUS_OK, "");
    auto* token = new (std::nothrow) Dust3DCancellationToken;
    if (!token)
        dust3d::native::setError(error, DUST3D_STATUS_LIMIT_EXCEEDED, "Not enough memory to allocate a cancellation token.");
    return token;
}

void dust3d_cancellation_token_cancel(Dust3DCancellationToken* token)
{
    if (token)
        token->cancelled.store(true, std::memory_order_relaxed);
}

int dust3d_cancellation_token_is_cancelled(const Dust3DCancellationToken* token)
{
    return token && token->cancelled.load(std::memory_order_relaxed) ? 1 : 0;
}

void dust3d_cancellation_token_destroy(Dust3DCancellationToken* token)
{
    delete token;
}
