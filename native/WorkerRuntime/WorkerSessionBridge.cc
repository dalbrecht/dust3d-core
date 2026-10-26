// SPDX-License-Identifier: MIT
#include "WorkerSessionInternal.h"
#include "WorkerSessionOutputInternal.h"
#include <algorithm>
#include <iterator>
using namespace dust3d::worker;
extern "C" Dust3DWorkerSession* dust3d_worker_session_create(Dust3DWorkerSessionConfiguration configuration, Dust3DWorkerDecodeResult* result) noexcept
{
    if (!result)
        return nullptr;
    *result = {};
    result->status = DUST3D_WORKER_DECODE_INTERNAL_FAILED;
    try {
        std::array<uint8_t, 16> document;
        std::copy(std::begin(configuration.document), std::end(configuration.document), document.begin());
        auto owner = std::make_unique<Dust3DWorkerSession>(document, configuration.connection_generation, configuration.maximum_cache_payload_bytes);
        result->status = DUST3D_WORKER_DECODE_OK;
        return owner.release();
    } catch (...) {
        bridge::rejectSessionOutputException(std::current_exception(), *result);
    }
    return nullptr;
}
namespace {
template <typename Action>
int changeSession(Dust3DWorkerSession* owner, Dust3DWorkerDecodeResult* result, Action action) noexcept
{
    if (!result)
        return 0;
    *result = {};
    result->status = DUST3D_WORKER_DECODE_INTERNAL_FAILED;
    try {
        if (!owner)
            throw ProtocolError(ProtocolErrorCode::invalidInput, "C worker session owner is null.");
        action(owner->value);
        result->status = DUST3D_WORKER_DECODE_OK;
        return 1;
    } catch (...) {
        bridge::rejectSessionOutputException(std::current_exception(), *result);
    }
    return 0;
}
}
extern "C" int dust3d_worker_session_reset(Dust3DWorkerSession* owner, uint64_t generation, Dust3DWorkerDecodeResult* result) noexcept
{
    return changeSession(owner, result, [generation](ConnectionGenerationSession& value) { value.reset(generation); });
}
extern "C" int dust3d_worker_session_close(Dust3DWorkerSession* owner, Dust3DWorkerDecodeResult* result) noexcept
{
    return changeSession(owner, result, [](ConnectionGenerationSession& value) { value.close(); });
}
extern "C" void dust3d_worker_session_destroy(Dust3DWorkerSession* owner) noexcept { delete owner; }
