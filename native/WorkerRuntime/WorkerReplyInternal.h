// SPDX-License-Identifier: MIT
#ifndef DUST3D_WORKER_REPLY_INTERNAL_H_
#define DUST3D_WORKER_REPLY_INTERNAL_H_
#include "WorkerReplyCollector.h"
#include <Dust3DWorkerRuntime.h>
struct Dust3DWorkerReplyCollector {
    std::vector<uint8_t> bytes;
    dust3d::worker::Header expected;
    dust3d::worker::Limits policy;
    dust3d::worker::WorkerReplyCollector collector;
    bool isFramed = false;
    bool isRejected = false;
    bool isDecoded = false;
    Dust3DWorkerReplyCollector(const dust3d::worker::Header& header, const dust3d::worker::Limits& limits)
        : expected(header)
        , policy(limits)
        , collector(header, limits, bytes)
    {
    }
};
#endif
