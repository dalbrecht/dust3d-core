// SPDX-License-Identifier: MIT
#ifndef DUST3D_WORKER_SESSION_INTERNAL_H_
#define DUST3D_WORKER_SESSION_INTERNAL_H_
#include "WorkerConnectionSession.h"
#include <Dust3DWorkerSession.h>
struct Dust3DWorkerSession {
    dust3d::worker::ConnectionGenerationSession value;
    Dust3DWorkerSession(const std::array<uint8_t, 16>& document, uint64_t generation, uint64_t cacheBytes)
        : value(document, generation, cacheBytes)
    {
    }
};
#endif
