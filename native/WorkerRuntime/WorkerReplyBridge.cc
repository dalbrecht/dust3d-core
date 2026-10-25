// SPDX-License-Identifier: MIT
#include "WorkerReplyCollector.h"
#include <Dust3DWorkerRuntime.h>
#include <memory>
#include <new>
using namespace dust3d::worker;
struct Dust3DWorkerReplyCollector {
    std::vector<uint8_t> bytes;
    WorkerReplyCollector collector;
    bool isFramed = false;
    bool isRejected = false;
    Dust3DWorkerReplyCollector(const Header& header, const Limits& limits)
        : collector(header, limits, bytes)
    {
    }
};
namespace {
void copyReplyCause(Dust3DWorkerReplyResult& result, const char* cause) noexcept
{
    if (!cause)
        return;
    size_t length = 0;
    while (length < sizeof(result.message) - 1 && cause[length]) {
        result.message[length] = cause[length];
        ++length;
    }
    result.message[length] = 0;
    result.message_truncated = cause[length] != 0;
}
template <typename Action>
Dust3DWorkerReplyResult protect(Action action) noexcept
{
    Dust3DWorkerReplyResult result {};
    result.status = DUST3D_WORKER_REPLY_INTERNAL_FAILED;
    try {
        result.status = action();
    } catch (const ProtocolError& cause) {
        result.status = DUST3D_WORKER_REPLY_PROTOCOL_REJECTED;
        result.detail_source = DUST3D_WORKER_DETAIL_FRAME;
        result.detail_code = static_cast<int32_t>(cause.code());
        copyReplyCause(result, cause.what());
    } catch (const std::bad_alloc& cause) {
        result.status = DUST3D_WORKER_REPLY_LIMIT_EXCEEDED;
        result.detail_source = DUST3D_WORKER_DETAIL_RUNTIME;
        copyReplyCause(result, cause.what());
    } catch (const std::exception& cause) {
        result.detail_source = DUST3D_WORKER_DETAIL_RUNTIME;
        copyReplyCause(result, cause.what());
    } catch (...) {
        result.detail_source = DUST3D_WORKER_DETAIL_RUNTIME;
        copyReplyCause(result, "Worker reply exception has no available diagnostic.");
    }
    return result;
}
template <typename Action>
Dust3DWorkerReplyResult use(Dust3DWorkerReplyCollector* owner, Action action) noexcept
{
    auto result = protect([&] {
        if (!owner || owner->isRejected)
            throw ProtocolError(ProtocolErrorCode::invalidInput, "Worker reply owner is null or rejected.");
        action(*owner);
        return owner->isFramed ? DUST3D_WORKER_REPLY_FRAMED : DUST3D_WORKER_REPLY_READY;
    });
    if (owner && result.status != DUST3D_WORKER_REPLY_READY && result.status != DUST3D_WORKER_REPLY_FRAMED)
        owner->isRejected = true;
    return result;
}
}
extern "C" Dust3DWorkerReplyCollector* dust3d_worker_reply_create(const uint8_t* expected, size_t size,
    Dust3DWorkerWireLimits limits, Dust3DWorkerReplyResult* result) noexcept
{
    if (!result)
        return nullptr;
    std::unique_ptr<Dust3DWorkerReplyCollector> owner;
    *result = protect([&] {
        const Limits policy { limits.maximum_input_bytes, limits.maximum_output_bytes, limits.maximum_failure_bytes };
        const auto header = decodeHeader(expected, size, policy);
        owner = std::make_unique<Dust3DWorkerReplyCollector>(header, policy);
        return DUST3D_WORKER_REPLY_READY;
    });
    return owner.release();
}
extern "C" Dust3DWorkerReplyResult dust3d_worker_reply_next_read(Dust3DWorkerReplyCollector* owner, size_t maximum, size_t* count) noexcept
{
    if (count)
        *count = 0;
    return use(owner, [&](Dust3DWorkerReplyCollector& value) {
        if (!count)
            throw ProtocolError(ProtocolErrorCode::invalidInput, "Worker reply read size output is null.");
        *count = value.collector.nextReadBytes(maximum);
    });
}
extern "C" Dust3DWorkerReplyResult dust3d_worker_reply_append(Dust3DWorkerReplyCollector* owner, const uint8_t* bytes, size_t size) noexcept
{
    return use(owner, [&](Dust3DWorkerReplyCollector& value) { value.collector.append(bytes, size); });
}
extern "C" Dust3DWorkerReplyResult dust3d_worker_reply_finish(Dust3DWorkerReplyCollector* owner) noexcept
{
    return use(owner, [&](Dust3DWorkerReplyCollector& value) {
        value.collector.finish();
        value.isFramed = true;
    });
}
extern "C" const uint8_t* dust3d_worker_reply_bytes(const Dust3DWorkerReplyCollector* owner, size_t* size) noexcept
{
    if (!size)
        return nullptr;
    *size = 0;
    if (!owner || owner->isRejected || !owner->isFramed)
        return nullptr;
    *size = owner->bytes.size();
    return owner->bytes.data();
}
extern "C" void dust3d_worker_reply_destroy(Dust3DWorkerReplyCollector* owner) noexcept
{
    delete owner;
}
