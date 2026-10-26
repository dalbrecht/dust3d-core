// SPDX-License-Identifier: MIT
#include "WorkerDecodedInternal.h"
#include "WorkerQueue.h"
#include "WorkerReplyInternal.h"
#include "WorkerServiceInternal.h"
#include <memory>
#include <new>
#include <utility>
using namespace dust3d::worker;
struct Dust3DWorkerServiceQueue {
    Limits policy;
    RequestQueue state;
    std::vector<uint8_t> activeWire;
    std::unique_ptr<Dust3DWorkerReplyCollector> reply;
    explicit Dust3DWorkerServiceQueue(Limits limits)
        : policy(limits)
        , state(limits)
    {
    }
};
namespace {
template <typename Result>
void copyCause(Result& result, const char* cause) noexcept
{
    if (!cause)
        return;
    size_t size = 0;
    while (size < sizeof(result.message) - 1 && cause[size]) {
        result.message[size] = cause[size];
        ++size;
    }
    result.message[size] = 0;
    result.message_truncated = cause[size] != 0;
}
template <typename Result, typename Status, typename Error>
void reject(Result& result, Status status, Dust3DWorkerDetailSource source, const Error& cause) noexcept
{
    result.status = status;
    result.detail_source = source;
    result.detail_code = static_cast<int32_t>(cause.code());
    copyCause(result, cause.what());
}
template <typename Action>
Dust3DWorkerQueueResult protectQueue(Action action) noexcept
{
    Dust3DWorkerQueueResult result {};
    result.status = DUST3D_WORKER_QUEUE_INTERNAL_FAILED;
    try {
        result.status = action(result);
    } catch (const QueueError& cause) {
        reject(result, DUST3D_WORKER_QUEUE_REJECTED, DUST3D_WORKER_DETAIL_QUEUE, cause);
    } catch (const ProtocolError& cause) {
        reject(result, DUST3D_WORKER_QUEUE_PROTOCOL_REJECTED, DUST3D_WORKER_DETAIL_FRAME, cause);
    } catch (const std::bad_alloc& cause) {
        result.status = DUST3D_WORKER_QUEUE_LIMIT_EXCEEDED;
        result.detail_source = DUST3D_WORKER_DETAIL_RUNTIME;
        copyCause(result, cause.what());
    } catch (const std::exception& cause) {
        result.detail_source = DUST3D_WORKER_DETAIL_RUNTIME;
        copyCause(result, cause.what());
    } catch (...) {
        result.detail_source = DUST3D_WORKER_DETAIL_RUNTIME;
        copyCause(result, "Worker queue exception has no available diagnostic.");
    }
    return result;
}
Dust3DWorkerServiceQueue& checked(Dust3DWorkerServiceQueue* owner)
{
    if (!owner)
        throw ProtocolError(ProtocolErrorCode::invalidInput, "Worker queue owner is null.");
    return *owner;
}
void clearActive(Dust3DWorkerServiceQueue& owner) noexcept
{
    owner.reply.reset();
    std::vector<uint8_t>().swap(owner.activeWire);
}
}
extern "C" Dust3DWorkerServiceQueue* dust3d_worker_service_queue_create(Dust3DWorkerWireLimits limits, Dust3DWorkerQueueResult* result) noexcept
{
    if (!result)
        return nullptr;
    std::unique_ptr<Dust3DWorkerServiceQueue> owner;
    *result = protectQueue([&](Dust3DWorkerQueueResult&) {
        owner = std::make_unique<Dust3DWorkerServiceQueue>(Limits { limits.maximum_input_bytes, limits.maximum_output_bytes, limits.maximum_failure_bytes });
        return DUST3D_WORKER_QUEUE_READY;
    });
    return owner.release();
}
extern "C" Dust3DWorkerQueueResult dust3d_worker_service_queue_submit(Dust3DWorkerServiceQueue* queue, const uint8_t* bytes, size_t size) noexcept
{
    return protectQueue([&](Dust3DWorkerQueueResult& result) {
        auto& owner = checked(queue);
        if (owner.state.isClosed())
            throw QueueError(QueueErrorCode::closed, "Worker queue is closed");
        auto request = decodeFrame(bytes, size, owner.policy);
        const auto replaced = owner.state.submit(std::move(request));
        if (replaced) {
            result.has_replaced = 1;
            result.replaced_identity = bridge::copyRequestIdentity(*replaced);
        }
        return DUST3D_WORKER_QUEUE_READY;
    });
}
extern "C" Dust3DWorkerQueueResult dust3d_worker_service_queue_begin(Dust3DWorkerServiceQueue* queue, uint64_t generation) noexcept
{
    return protectQueue([&](Dust3DWorkerQueueResult& result) {
        auto& owner = checked(queue);
        if (!generation)
            throw QueueError(QueueErrorCode::invalidServiceSession, "Service queue requires a nonzero connection generation");
        if (owner.reply)
            return DUST3D_WORKER_QUEUE_BUSY;
        auto request = owner.state.beginService(generation);
        if (!request)
            return DUST3D_WORKER_QUEUE_IDLE;
        try {
            auto reply = std::make_unique<Dust3DWorkerReplyCollector>(request->header, owner.policy);
            reply->serviceConnectionGeneration = generation;
            reply->isQueueBound = true;
            auto wire = encodeFrame(*request, owner.policy);
            owner.activeWire = std::move(wire);
            owner.reply = std::move(reply);
            result.active_identity = bridge::copyRequestIdentity(request->header.identity);
            return DUST3D_WORKER_QUEUE_BEGUN;
        } catch (...) {
            // No external callback can change this private matching service slot.
            owner.state.abandon(request->header.identity);
            clearActive(owner);
            throw;
        }
    });
}
extern "C" const uint8_t* dust3d_worker_service_queue_request_bytes(const Dust3DWorkerServiceQueue* queue, size_t* size) noexcept
{
    if (!size)
        return nullptr;
    *size = 0;
    if (!queue || !queue->reply)
        return nullptr;
    *size = queue->activeWire.size();
    return queue->activeWire.data();
}
extern "C" Dust3DWorkerReplyCollector* dust3d_worker_service_queue_reply(Dust3DWorkerServiceQueue* queue) noexcept
{
    return queue ? queue->reply.get() : nullptr;
}
extern "C" void dust3d_worker_service_queue_cancel(Dust3DWorkerServiceQueue* queue, Dust3DWorkerRequestIdentity identity) noexcept
{
    if (queue)
        queue->state.cancel(bridge::copyRequestIdentity(identity));
}
extern "C" void dust3d_worker_service_queue_close(Dust3DWorkerServiceQueue* queue) noexcept
{
    if (queue)
        queue->state.close();
}
extern "C" Dust3DWorkerQueueResult dust3d_worker_service_queue_abandon(Dust3DWorkerServiceQueue* queue, Dust3DWorkerRequestIdentity identity) noexcept
{
    return protectQueue([&](Dust3DWorkerQueueResult&) {
        auto& owner = checked(queue);
        owner.state.abandon(bridge::copyRequestIdentity(identity));
        clearActive(owner);
        return DUST3D_WORKER_QUEUE_READY;
    });
}
extern "C" Dust3DWorkerDecodedReply* dust3d_worker_service_queue_admit(Dust3DWorkerServiceQueue* queue, Dust3DWorkerServiceObservation observed,
    Dust3DWorkerServiceReceipt receipt, uint64_t maximum, Dust3DWorkerDecodeResult* result) noexcept
{
    if (!result)
        return nullptr;
    *result = {};
    result->status = DUST3D_WORKER_DECODE_INTERNAL_FAILED;
    try {
        auto& owner = checked(queue);
        if (!owner.reply)
            throw QueueError(QueueErrorCode::unexpectedCompletion, "Service completion has no active attempt");
        auto reply = std::move(owner.reply);
        std::vector<uint8_t>().swap(owner.activeWire);
        bool hasStartedCompletion = false;
        try {
            if (reply->isRejected || !reply->isFramed || reply->isDecoded)
                throw ProtocolError(ProtocolErrorCode::invalidInput, "Queue admission requires one unused EOF-framed owner.");
            reply->isDecoded = true;
            const auto control = bridge::copyServiceReceipt(receipt);
            const auto policy = bridge::semanticPolicies(owner.policy, maximum);
            // Only this private slot can reach completion; cancel/close retire publication, not identity.
            hasStartedCompletion = true;
            auto value = owner.state.completeService(reply->expected.identity,
                { observed.connection_generation, observed.process_identifier, true }, control,
                reply->bytes.data(), reply->bytes.size(), policy.mesh, policy.failure, policy.uv, policy.sources, policy.indexed, policy.imported);
            auto* decoded = bridge::adoptDecodedReply(std::move(value));
            result->status = DUST3D_WORKER_DECODE_OK;
            return decoded;
        } catch (...) {
            if (!hasStartedCompletion)
                owner.state.abandon(reply->expected.identity);
            throw;
        }
    } catch (const QueueError& cause) {
        reject(*result, DUST3D_WORKER_DECODE_QUEUE_REJECTED, DUST3D_WORKER_DETAIL_QUEUE, cause);
    } catch (const AdmissionError& cause) {
        reject(*result, DUST3D_WORKER_DECODE_ADMISSION_REJECTED, DUST3D_WORKER_DETAIL_ADMISSION, cause);
    } catch (const ProtocolError& cause) {
        reject(*result, DUST3D_WORKER_DECODE_PROTOCOL_REJECTED, DUST3D_WORKER_DETAIL_FRAME, cause);
    } catch (const MeshPayloadError& cause) {
        reject(*result, DUST3D_WORKER_DECODE_CODEC_REJECTED, DUST3D_WORKER_DETAIL_MESH, cause);
    } catch (const UvPayloadError& cause) {
        reject(*result, DUST3D_WORKER_DECODE_CODEC_REJECTED, DUST3D_WORKER_DETAIL_UV, cause);
    } catch (const UvMeshPayloadError& cause) {
        reject(*result, DUST3D_WORKER_DECODE_CODEC_REJECTED, DUST3D_WORKER_DETAIL_UV_MESH, cause);
    } catch (const SourceHintPayloadError& cause) {
        reject(*result, DUST3D_WORKER_DECODE_CODEC_REJECTED, DUST3D_WORKER_DETAIL_SOURCE_HINTS, cause);
    } catch (const SourcedMeshPayloadError& cause) {
        reject(*result, DUST3D_WORKER_DECODE_CODEC_REJECTED, DUST3D_WORKER_DETAIL_SOURCED_MESH, cause);
    } catch (const IndexedMeshPayloadError& cause) {
        reject(*result, DUST3D_WORKER_DECODE_CODEC_REJECTED, DUST3D_WORKER_DETAIL_INDEXED_MESH, cause);
    } catch (const IndexedSourcesPayloadError& cause) {
        reject(*result, DUST3D_WORKER_DECODE_CODEC_REJECTED, DUST3D_WORKER_DETAIL_INDEXED_SOURCES, cause);
    } catch (const ImportedRenderError& cause) {
        reject(*result, DUST3D_WORKER_DECODE_CODEC_REJECTED, DUST3D_WORKER_DETAIL_IMPORTED_RENDER, cause);
    } catch (const ImportedSurfaceError& cause) {
        reject(*result, DUST3D_WORKER_DECODE_CODEC_REJECTED, DUST3D_WORKER_DETAIL_IMPORTED_SURFACE, cause);
    } catch (const ImportedMaterialsError& cause) {
        reject(*result, DUST3D_WORKER_DECODE_CODEC_REJECTED, DUST3D_WORKER_DETAIL_IMPORTED_MATERIALS, cause);
    } catch (const FailurePayloadError& cause) {
        reject(*result, DUST3D_WORKER_DECODE_CODEC_REJECTED, DUST3D_WORKER_DETAIL_FAILURE, cause);
    } catch (const std::bad_alloc& cause) {
        result->status = DUST3D_WORKER_DECODE_LIMIT_EXCEEDED;
        result->detail_source = DUST3D_WORKER_DETAIL_RUNTIME;
        copyCause(*result, cause.what());
    } catch (const std::exception& cause) {
        result->detail_source = DUST3D_WORKER_DETAIL_RUNTIME;
        copyCause(*result, cause.what());
    } catch (...) {
        result->detail_source = DUST3D_WORKER_DETAIL_RUNTIME;
        copyCause(*result, "Service queue exception has no available diagnostic.");
    }
    return nullptr;
}
extern "C" void dust3d_worker_service_queue_destroy(Dust3DWorkerServiceQueue* queue) noexcept { delete queue; }
