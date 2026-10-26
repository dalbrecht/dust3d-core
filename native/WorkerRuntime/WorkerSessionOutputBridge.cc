// SPDX-License-Identifier: MIT
#include "WorkerQueue.h"
#include "WorkerReplyInternal.h"
#include "WorkerServiceInternal.h"
#include "WorkerSessionOutputInternal.h"
#include <algorithm>
#include <memory>
using namespace dust3d::worker;
namespace dust3d::worker::bridge {
Dust3DWorkerSessionOutput* adoptSessionOutput(SessionOutput value)
{
    auto owner = std::make_unique<Dust3DWorkerSessionOutput>();
    if (auto* sourced = std::get_if<IndexedSourcesPayload>(&value.outcome)) {
        owner->outcome = DUST3D_WORKER_SESSION_SOURCED;
        owner->primary.emplace(Dust3DWorkerDecodedReply { std::move(*sourced) });
    } else if (auto* mesh = std::get_if<IndexedMeshPayload>(&value.outcome)) {
        owner->outcome = DUST3D_WORKER_SESSION_ORDINARY;
        owner->primary.emplace(Dust3DWorkerDecodedReply { std::move(*mesh) });
    } else if (auto* failure = std::get_if<FailurePayload>(&value.outcome)) {
        owner->outcome = DUST3D_WORKER_SESSION_FAILURE;
        owner->primary.emplace(Dust3DWorkerDecodedReply { std::move(*failure) });
    }
    if (value.sourceIssue)
        owner->sourceIssue.emplace(Dust3DWorkerDecodedReply { std::move(*value.sourceIssue) });
    if (value.statisticsIssue)
        owner->statisticsIssue.emplace(Dust3DWorkerDecodedReply { std::move(*value.statisticsIssue) });
    owner->statistics = std::move(value.statistics);
    return owner.release();
}
}
namespace {
void copyCause(Dust3DWorkerDecodeResult& result, const char* cause) noexcept
{
    size_t count = 0;
    while (count < sizeof(result.message) - 1 && cause[count]) {
        result.message[count] = cause[count];
        ++count;
    }
    result.message[count] = 0;
    result.message_truncated = cause[count] != 0;
}
template <typename Error>
void rejectCodec(Dust3DWorkerDecodeResult& result, Dust3DWorkerDetailSource source, const Error& cause) noexcept
{
    result.status = DUST3D_WORKER_DECODE_CODEC_REJECTED;
    result.detail_source = source;
    result.detail_code = static_cast<int32_t>(cause.code());
    copyCause(result, cause.what());
}
SessionOutputLimits policy(Dust3DWorkerSessionOutputLimits value)
{
    if (!value.maximum_wire_bytes || value.maximum_wire_bytes > maximumPayloadBytes || !value.maximum_decoded_bytes || value.maximum_decoded_bytes > maximumPayloadBytes || value.maximum_source_bytes > maximumPayloadBytes)
        throw ProtocolError(ProtocolErrorCode::invalidLimits, "C session output policies exceed supported positive bounds.");
    SessionOutputLimits result;
    result.maximumWireBytes = value.maximum_wire_bytes;
    result.maximumDecodedBytes = value.maximum_decoded_bytes;
    result.ordinary.maximumWireBytes = value.maximum_wire_bytes;
    result.ordinary.maximumDecodedBytes = value.maximum_decoded_bytes;
    result.sourced.maximumWireBytes = value.maximum_wire_bytes;
    result.sourced.maximumDecodedBytes = value.maximum_decoded_bytes;
    result.sourced.geometry = result.ordinary;
    result.sourced.sources.maximumWireBytes = value.maximum_wire_bytes;
    const auto sourceBytes = value.maximum_source_bytes ? value.maximum_source_bytes : 64ULL * 1024 * 1024;
    result.sourced.sources.maximumDecodedBytes = std::min<uint64_t>(sourceBytes, value.maximum_decoded_bytes);
    return result;
}
}
namespace dust3d::worker::bridge {
SessionOutputLimits sessionOutputPolicy(Dust3DWorkerSessionOutputLimits value) { return policy(value); }
void rejectSessionOutputException(const std::exception_ptr& exception, Dust3DWorkerDecodeResult& value) noexcept
{
    auto* result = &value;
    *result = {};
    result->status = DUST3D_WORKER_DECODE_INTERNAL_FAILED;
    try {
        std::rethrow_exception(exception);
    } catch (const AdmissionError& cause) {
        result->status = DUST3D_WORKER_DECODE_ADMISSION_REJECTED;
        result->detail_source = DUST3D_WORKER_DETAIL_ADMISSION;
        result->detail_code = static_cast<int32_t>(cause.code());
        copyCause(*result, cause.what());
    } catch (const QueueError& cause) {
        result->status = DUST3D_WORKER_DECODE_QUEUE_REJECTED;
        result->detail_source = DUST3D_WORKER_DETAIL_QUEUE;
        result->detail_code = static_cast<int32_t>(cause.code());
        copyCause(*result, cause.what());
    } catch (const ProtocolError& cause) {
        result->status = DUST3D_WORKER_DECODE_PROTOCOL_REJECTED;
        result->detail_source = DUST3D_WORKER_DETAIL_FRAME;
        result->detail_code = static_cast<int32_t>(cause.code());
        copyCause(*result, cause.what());
    } catch (const SessionOutputError& cause) {
        rejectCodec(*result, DUST3D_WORKER_DETAIL_SESSION_OUTPUT, cause);
    } catch (const GenerationStatisticsError& cause) {
        rejectCodec(*result, DUST3D_WORKER_DETAIL_GENERATION_STATISTICS, cause);
    } catch (const IndexedSourcesPayloadError& cause) {
        rejectCodec(*result, DUST3D_WORKER_DETAIL_INDEXED_SOURCES, cause);
    } catch (const IndexedMeshPayloadError& cause) {
        rejectCodec(*result, DUST3D_WORKER_DETAIL_INDEXED_MESH, cause);
    } catch (const SourceHintPayloadError& cause) {
        rejectCodec(*result, DUST3D_WORKER_DETAIL_SOURCE_HINTS, cause);
    } catch (const MeshPayloadError& cause) {
        rejectCodec(*result, DUST3D_WORKER_DETAIL_MESH, cause);
    } catch (const FailurePayloadError& cause) {
        rejectCodec(*result, DUST3D_WORKER_DETAIL_FAILURE, cause);
    } catch (const std::bad_alloc& cause) {
        result->status = DUST3D_WORKER_DECODE_LIMIT_EXCEEDED;
        result->detail_source = DUST3D_WORKER_DETAIL_RUNTIME;
        copyCause(*result, cause.what());
    } catch (const std::exception& cause) {
        result->detail_source = DUST3D_WORKER_DETAIL_RUNTIME;
        copyCause(*result, cause.what());
    } catch (...) {
        result->detail_source = DUST3D_WORKER_DETAIL_RUNTIME;
        result->detail_code = -1;
        copyCause(*result, "Session output exception has no available diagnostic.");
    }
}
}
extern "C" Dust3DWorkerSessionOutput* dust3d_worker_session_output_decode(const uint8_t* bytes, size_t size, Dust3DWorkerSessionOutputLimits limits, Dust3DWorkerDecodeResult* result) noexcept
{
    if (!result)
        return nullptr;
    *result = {};
    result->status = DUST3D_WORKER_DECODE_INTERNAL_FAILED;
    try {
        auto value = decodeSessionOutput(bytes, size, policy(limits));
        auto* owner = bridge::adoptSessionOutput(std::move(value));
        result->status = DUST3D_WORKER_DECODE_OK;
        return owner;
    } catch (...) {
        bridge::rejectSessionOutputException(std::current_exception(), *result);
    }
    return nullptr;
}
extern "C" Dust3DWorkerSessionOutputSummary dust3d_worker_session_output_summary(const Dust3DWorkerSessionOutput* owner) noexcept
{
    if (!owner)
        return {};
    return { owner->outcome, static_cast<uint8_t>(owner->statistics.has_value()) };
}
extern "C" const Dust3DWorkerDecodedReply* dust3d_worker_session_output_primary(const Dust3DWorkerSessionOutput* owner) noexcept
{
    return owner && owner->primary ? &*owner->primary : nullptr;
}
extern "C" const Dust3DWorkerDecodedReply* dust3d_worker_session_output_source_issue(const Dust3DWorkerSessionOutput* owner) noexcept
{
    return owner && owner->sourceIssue ? &*owner->sourceIssue : nullptr;
}
extern "C" const Dust3DWorkerDecodedReply* dust3d_worker_session_output_statistics_issue(const Dust3DWorkerSessionOutput* owner) noexcept
{
    return owner && owner->statisticsIssue ? &*owner->statisticsIssue : nullptr;
}
extern "C" int dust3d_worker_session_output_statistics(const Dust3DWorkerSessionOutput* owner, Dust3DWorkerSessionStatistics* output, size_t size) noexcept
{
    if (!output || size != sizeof(*output))
        return 0;
    *output = {};
    if (!owner || !owner->statistics)
        return 0;
    const auto& value = *owner->statistics;
    *output = { value.maximumCachePayloadBytes, value.retainedPayloadBytes, value.reusedComponents, value.rebuiltComponents, value.rebuiltParts, value.cacheOutcome, value.cacheStatus };
    return 1;
}
extern "C" const uint8_t* dust3d_worker_session_output_cache_cause(const Dust3DWorkerSessionOutput* owner, size_t* size) noexcept
{
    if (!size)
        return nullptr;
    *size = 0;
    if (!owner || !owner->statistics || owner->statistics->cacheCause.empty())
        return nullptr;
    *size = owner->statistics->cacheCause.size();
    return owner->statistics->cacheCause.data();
}
extern "C" void dust3d_worker_session_output_destroy(Dust3DWorkerSessionOutput* owner) noexcept { delete owner; }

extern "C" Dust3DWorkerSessionOutput* dust3d_worker_session_service_admit(Dust3DWorkerReplyCollector* owner,
    Dust3DWorkerRequestIdentity active, Dust3DWorkerServiceObservation observed,
    Dust3DWorkerServiceReceipt receipt, Dust3DWorkerSessionOutputLimits limits, Dust3DWorkerDecodeResult* result) noexcept
{
    if (!result)
        return nullptr;
    *result = {};
    result->status = DUST3D_WORKER_DECODE_INTERNAL_FAILED;
    try {
        if (!owner || owner->isRejected || !owner->isFramed || owner->isDecoded || !owner->serviceConnectionGeneration || owner->isQueueBound)
            throw ProtocolError(ProtocolErrorCode::invalidInput, "Session admission requires one unused EOF-framed non-queue service collector.");
        owner->isDecoded = true;
        const auto policy = bridge::sessionOutputPolicy(limits);
        const auto control = bridge::copyServiceReceipt(receipt);
        PendingServiceReply pending(owner->expected, owner->serviceConnectionGeneration, owner->policy);
        auto value = pending.completeSession(bridge::copyRequestIdentity(active),
            { observed.connection_generation, observed.process_identifier, owner->isFramed }, control,
            owner->bytes.data(), owner->bytes.size(), owner->policy, policy);
        auto* output = bridge::adoptSessionOutput(std::move(value));
        result->status = DUST3D_WORKER_DECODE_OK;
        return output;
    } catch (...) {
        bridge::rejectSessionOutputException(std::current_exception(), *result);
        if (owner)
            owner->isRejected = true;
    }
    return nullptr;
}
