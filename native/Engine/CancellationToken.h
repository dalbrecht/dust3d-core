// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_CANCELLATION_TOKEN_H
#define DUST3D_NATIVE_CANCELLATION_TOKEN_H

#include <atomic>

struct Dust3DCancellationToken {
    std::atomic_bool cancelled { false };
};

#endif
