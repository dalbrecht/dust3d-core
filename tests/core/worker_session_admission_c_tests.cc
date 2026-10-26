// SPDX-License-Identifier: MIT
#include <Dust3DWorkerSessionOutput.h>
#include <iostream>
#include <memory>
#include <native/Engine/WorkerAdmission.h>
#include <native/Engine/WorkerSessionOutput.h>
using namespace dust3d::worker;
void require(bool value)
{
    if (!value)
        throw std::runtime_error("C session admission contract");
}
using Collector = std::unique_ptr<Dust3DWorkerReplyCollector, decltype(&dust3d_worker_reply_destroy)>;
using Owner = std::unique_ptr<Dust3DWorkerSessionOutput, decltype(&dust3d_worker_session_output_destroy)>;
Header request(uint64_t id)
{
    Header value;
    value.operation = Operation::generateSessionIndexedSourcedMesh;
    value.identity.document[0] = 1;
    value.identity.request = id;
    value.identity.revision = 7;
    value.payloadBytes = 1;
    return value;
}
Dust3DWorkerRequestIdentity identity(const Header& header)
{
    Dust3DWorkerRequestIdentity result {};
    result.document[0] = 1;
    result.request = header.identity.request;
    result.revision = header.identity.revision;
    return result;
}
Dust3DWorkerServiceReceipt receipt(const Header& header)
{
    Dust3DWorkerServiceReceipt result {};
    result.operation = DUST3D_WORKER_OPERATION_SESSION_INDEXED_SOURCED_MESH;
    result.request_identity = header.identity.request;
    result.process_identifier = 1234;
    return result;
}
std::vector<uint8_t> response(const Header& request)
{
    SessionOutput value;
    value.statistics = GenerationStatistics {};
    value.statistics->reusedComponents = 7;
    auto body = encodeSessionOutput(value, {});
    auto header = request;
    header.kind = FrameKind::response;
    header.payloadBytes = body.size();
    return encodeFrame({ header, std::move(body) }, {});
}
Collector framed(const Header& header, bool finish = true, uint64_t generation = 9)
{
    const auto bytes = encodeHeader(header, {});
    Dust3DWorkerReplyResult result {};
    Collector owner(generation ? dust3d_worker_service_reply_create(bytes.data(), bytes.size(), generation, { 4096, 4096, 4096 }, &result)
                               : dust3d_worker_reply_create(bytes.data(), bytes.size(), { 4096, 4096, 4096 }, &result),
        dust3d_worker_reply_destroy);
    require(owner && result.status == DUST3D_WORKER_REPLY_READY);
    const auto reply = response(header);
    require(dust3d_worker_reply_append(owner.get(), reply.data(), reply.size()).status == DUST3D_WORKER_REPLY_READY);
    if (finish)
        require(dust3d_worker_reply_finish(owner.get()).status == DUST3D_WORKER_REPLY_FRAMED);
    return owner;
}
int main()
{
    try {
        const auto header = request(1);
        Dust3DWorkerDecodeResult result {};
        const Dust3DWorkerServiceObservation observed { 9, 1234 }; // Synthetic control data; no OS/native work.
        auto input = framed(header);
        require(!dust3d_worker_session_service_admit(input.get(), identity(header), observed, receipt(header), { 4096, 4096, 0 }, nullptr));
        Owner output(dust3d_worker_session_service_admit(input.get(), identity(header), observed, receipt(header), { 4096, 4096, 0 }, &result), dust3d_worker_session_output_destroy);
        require(output && result.status == DUST3D_WORKER_DECODE_OK);
        require(!dust3d_worker_session_service_admit(input.get(), identity(header), observed, receipt(header), { 4096, 4096, 0 }, &result)
            && result.status == DUST3D_WORKER_DECODE_PROTOCOL_REJECTED);
        input.reset();
        Dust3DWorkerSessionStatistics stats {};
        require(dust3d_worker_session_output_statistics(output.get(), &stats, sizeof(stats)) && stats.reused_components == 7);
        for (int mode = 0; mode < 5; ++mode) {
            auto owner = framed(header, mode != 3);
            auto current = identity(header);
            auto observation = observed;
            auto control = receipt(header);
            auto expected = DUST3D_WORKER_DECODE_ADMISSION_REJECTED;
            if (!mode)
                ++current.revision;
            if (mode == 1)
                ++observation.connection_generation;
            if (mode == 2)
                control.runtime_status = 1;
            if (mode == 3)
                expected = DUST3D_WORKER_DECODE_PROTOCOL_REJECTED;
            if (mode == 4) {
                control.operation = 0x10009;
                expected = DUST3D_WORKER_DECODE_PROTOCOL_REJECTED;
            }
            require(!dust3d_worker_session_service_admit(owner.get(), current, observation, control, { 4096, 4096, 0 }, &result) && result.status == expected);
            require(!dust3d_worker_session_service_admit(owner.get(), identity(header), observed, receipt(header), { 4096, 4096, 0 }, &result) && result.status == DUST3D_WORKER_DECODE_PROTOCOL_REJECTED);
        }
        auto childCollector = framed(header, true, 0);
        require(!dust3d_worker_session_service_admit(childCollector.get(), identity(header), observed, receipt(header), { 4096, 4096, 0 }, &result)
            && result.status == DUST3D_WORKER_DECODE_PROTOCOL_REJECTED);
        Dust3DWorkerQueueResult state {};
        std::unique_ptr<Dust3DWorkerServiceQueue, decltype(&dust3d_worker_service_queue_destroy)> queue(dust3d_worker_service_queue_create({ 4096, 4096, 4096 }, &state), dust3d_worker_service_queue_destroy);
        require(static_cast<bool>(queue));
        const auto submitted = encodeFrame({ header, { 1 } }, {});
        require(dust3d_worker_service_queue_submit(queue.get(), submitted.data(), submitted.size()).status == DUST3D_WORKER_QUEUE_READY);
        require(dust3d_worker_service_queue_begin(queue.get(), 9).status == DUST3D_WORKER_QUEUE_BEGUN);
        auto* reply = dust3d_worker_service_queue_reply(queue.get());
        const auto wire = response(header);
        require(dust3d_worker_reply_append(reply, wire.data(), wire.size()).status == DUST3D_WORKER_REPLY_READY && dust3d_worker_reply_finish(reply).status == DUST3D_WORKER_REPLY_FRAMED);
        require(!dust3d_worker_session_service_queue_admit(queue.get(), observed, receipt(header), { 4096, 4096, 0 }, nullptr));
        Owner queued(dust3d_worker_session_service_queue_admit(queue.get(), observed, receipt(header), { 4096, 4096, 0 }, &result), dust3d_worker_session_output_destroy);
        require(queued && result.status == DUST3D_WORKER_DECODE_OK && !dust3d_worker_service_queue_reply(queue.get()));
        require(!dust3d_worker_session_service_queue_admit(queue.get(), observed, receipt(header), { 4096, 4096, 0 }, &result) && result.status == DUST3D_WORKER_DECODE_QUEUE_REJECTED);
        for (int mode = 0; mode < 8; ++mode) {
            std::unique_ptr<Dust3DWorkerServiceQueue, decltype(&dust3d_worker_service_queue_destroy)> attempt(
                dust3d_worker_service_queue_create({ 4096, 4096, 4096 }, &state), dust3d_worker_service_queue_destroy);
            require(attempt && dust3d_worker_service_queue_submit(attempt.get(), submitted.data(), submitted.size()).status == DUST3D_WORKER_QUEUE_READY
                && dust3d_worker_service_queue_begin(attempt.get(), 9).status == DUST3D_WORKER_QUEUE_BEGUN);
            auto* collected = dust3d_worker_service_queue_reply(attempt.get());
            auto payload = wire;
            if (mode == 7) {
                auto partial = header;
                partial.kind = FrameKind::response;
                partial.payloadBytes = 1;
                payload = encodeFrame({ partial, { 0 } }, {});
            }
            require(dust3d_worker_reply_append(collected, payload.data(), payload.size()).status == DUST3D_WORKER_REPLY_READY);
            if (mode != 4)
                require(dust3d_worker_reply_finish(collected).status == DUST3D_WORKER_REPLY_FRAMED);
            const auto newerHeader = request(2);
            const auto newer = encodeFrame({ newerHeader, { 1 } }, {});
            if (mode == 0 || mode == 2)
                require(dust3d_worker_service_queue_submit(attempt.get(), newer.data(), newer.size()).status == DUST3D_WORKER_QUEUE_READY);
            if (mode == 1)
                dust3d_worker_service_queue_cancel(attempt.get(), identity(header));
            if (mode == 5)
                dust3d_worker_service_queue_close(attempt.get());
            if (mode == 3)
                require(!dust3d_worker_session_service_admit(collected, identity(header), observed, receipt(header), { 4096, 4096, 0 }, &result)
                    && result.status == DUST3D_WORKER_DECODE_PROTOCOL_REJECTED);
            auto control = receipt(header);
            if (mode == 6)
                control.finalization_error = 5;
            const Dust3DWorkerSessionOutputLimits policy { mode == 2 ? uint64_t(0) : uint64_t(4096), 4096, 0 };
            auto expected = (mode == 2 || mode == 3 || mode == 4) ? DUST3D_WORKER_DECODE_PROTOCOL_REJECTED : mode == 7 ? DUST3D_WORKER_DECODE_CODEC_REJECTED
                                                                                                                       : DUST3D_WORKER_DECODE_ADMISSION_REJECTED;
            require(!dust3d_worker_session_service_queue_admit(attempt.get(), observed, control, policy, &result) && result.status == expected
                && !dust3d_worker_service_queue_reply(attempt.get()));
            if (mode == 5)
                continue;
            if (mode != 0 && mode != 2)
                require(dust3d_worker_service_queue_submit(attempt.get(), newer.data(), newer.size()).status == DUST3D_WORKER_QUEUE_READY);
            const auto next = dust3d_worker_service_queue_begin(attempt.get(), 10);
            require(next.status == DUST3D_WORKER_QUEUE_BEGUN && next.active_identity.request == 2);
            require(dust3d_worker_service_queue_abandon(attempt.get(), next.active_identity).status == DUST3D_WORKER_QUEUE_READY);
        }
        auto legacy = framed(header);
        std::unique_ptr<Dust3DWorkerDecodedReply, decltype(&dust3d_worker_decoded_destroy)> wrongOwner(
            dust3d_worker_service_admit(legacy.get(), identity(header), observed, receipt(header), 4096, &result), dust3d_worker_decoded_destroy);
        require(!wrongOwner && result.status == DUST3D_WORKER_DECODE_PROTOCOL_REJECTED);
        require(!dust3d_worker_session_service_admit(nullptr, identity(header), observed, receipt(header), { 4096, 4096, 0 }, &result) && result.status == DUST3D_WORKER_DECODE_PROTOCOL_REJECTED);
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
    return 0;
}
