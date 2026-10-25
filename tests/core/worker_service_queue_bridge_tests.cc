// SPDX-License-Identifier: MIT
#include <Dust3DWorkerRuntime.h>
#include <array>
#include <iostream>
#include <memory>
#include <native/Engine/WorkerAdmission.h>
#include <native/Engine/WorkerFailure.h>
#include <stdexcept>
using namespace dust3d::worker;
namespace {
void require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}
using Queue = std::unique_ptr<Dust3DWorkerServiceQueue, decltype(&dust3d_worker_service_queue_destroy)>;
using Decoded = std::unique_ptr<Dust3DWorkerDecodedReply, decltype(&dust3d_worker_decoded_destroy)>;
Queue create()
{
    Dust3DWorkerQueueResult result;
    Queue queue(dust3d_worker_service_queue_create({ 4096, 4096, 4096 }, &result), dust3d_worker_service_queue_destroy);
    require(queue && result.status == DUST3D_WORKER_QUEUE_READY, "Queue created");
    return queue;
}
Dust3DWorkerQueueResult submit(Queue& queue, uint64_t number, Operation operation = Operation::generateMesh)
{
    Frame frame;
    frame.header.operation = operation;
    frame.header.identity.document[0] = 1;
    frame.header.identity.request = number;
    frame.header.identity.revision = number;
    frame.payload = { 'd', 's', '3' };
    frame.header.payloadBytes = frame.payload.size();
    const auto bytes = encodeFrame(frame, {});
    return dust3d_worker_service_queue_submit(queue.get(), bytes.data(), bytes.size());
}
Dust3DWorkerRequestIdentity begin(Queue& queue, uint64_t number, uint64_t generation = 9)
{
    const auto result = dust3d_worker_service_queue_begin(queue.get(), generation);
    require(result.status == DUST3D_WORKER_QUEUE_BEGUN && result.active_identity.request == number, "Expected request begun");
    return result.active_identity;
}
void frameBody(Queue& queue, FrameKind kind, std::vector<uint8_t> body)
{
    size_t size = 0;
    const auto* bytes = dust3d_worker_service_queue_request_bytes(queue.get(), &size);
    require(bytes && size >= headerBytes, "Borrowed request exists");
    Frame reply;
    reply.header = decodeHeader(bytes, headerBytes, {});
    reply.header.kind = kind;
    reply.payload = std::move(body);
    reply.header.payloadBytes = reply.payload.size();
    const auto wire = encodeFrame(reply, {});
    auto* collector = dust3d_worker_service_queue_reply(queue.get());
    require(collector, "Borrowed collector exists");
    const auto append = dust3d_worker_reply_append(collector, wire.data(), wire.size());
    require(append.status == DUST3D_WORKER_REPLY_READY, "Appended before actual EOF");
    require(dust3d_worker_reply_finish(collector).status == DUST3D_WORKER_REPLY_FRAMED, "Observed pure input EOF");
}
void frameFailure(Queue& queue)
{
    FailurePayload failure;
    failure.cause = { 'x', 0, 255 };
    frameBody(queue, FrameKind::failure, encodeFailurePayload(failure, {}));
}
Dust3DWorkerServiceReceipt receipt(uint64_t request)
{
    Dust3DWorkerServiceReceipt result {};
    result.operation = 1;
    result.request_identity = request;
    result.process_identifier = 101;
    return result;
}
Decoded admit(Queue& queue, uint64_t request, Dust3DWorkerDecodeResult& result, uint64_t generation = 9)
{
    return Decoded(dust3d_worker_service_queue_admit(queue.get(), { generation, 101 }, receipt(request), 4096, &result), dust3d_worker_decoded_destroy);
}
void latestSource()
{
    auto queue = create();
    require(submit(queue, 1).status == DUST3D_WORKER_QUEUE_READY, "Submitted first");
    require(dust3d_worker_service_queue_begin(queue.get(), 0).status == DUST3D_WORKER_QUEUE_REJECTED, "Zero generation preserves waiting");
    begin(queue, 1);
    require(dust3d_worker_service_queue_begin(queue.get(), 10).status == DUST3D_WORKER_QUEUE_BUSY, "Active generation frozen");
    require(submit(queue, 2).status == DUST3D_WORKER_QUEUE_READY, "Latest waiting");
    const auto replacement = submit(queue, 3);
    require(replacement.has_replaced && replacement.replaced_identity.request == 2, "Only latest waiting retained");
    frameFailure(queue);
    Dust3DWorkerDecodeResult result;
    require(!admit(queue, 1, result), "Stale active result rejected");
    require(result.status == DUST3D_WORKER_DECODE_ADMISSION_REJECTED && result.detail_code == static_cast<int32_t>(AdmissionErrorCode::staleRequest), "Original stale cause");
    begin(queue, 3, 10);
    frameFailure(queue);
    require(!dust3d_worker_service_queue_admit(queue.get(), { 10, 101 }, receipt(3), 4096, nullptr), "NULL result preserves attempt");
    auto decoded = admit(queue, 3, result, 10);
    require(decoded && result.status == DUST3D_WORKER_DECODE_OK, "Current result admitted");
    queue.reset();
    size_t size = 0;
    const auto* cause = dust3d_worker_decoded_failure_cause(decoded.get(), &size);
    require(size == 3 && cause && cause[0] == 'x' && cause[1] == 0 && cause[2] == 255, "Decoded arbitrary bytes outlive queue");
}
void retirementAndRecovery()
{
    for (bool shouldClose : { false, true }) {
        auto queue = create();
        submit(queue, 1);
        const auto identity = begin(queue, 1);
        if (shouldClose)
            dust3d_worker_service_queue_close(queue.get());
        else
            dust3d_worker_service_queue_cancel(queue.get(), identity);
        frameFailure(queue);
        Dust3DWorkerDecodeResult result;
        require(!admit(queue, 1, result) && result.detail_code == static_cast<int32_t>(AdmissionErrorCode::retired), "Retired publication cannot admit");
    }
    auto queue = create();
    submit(queue, 1);
    const auto identity = begin(queue, 1);
    auto foreign = identity;
    foreign.request = 999;
    require(dust3d_worker_service_queue_abandon(queue.get(), foreign).status == DUST3D_WORKER_QUEUE_REJECTED, "Foreign failure preserves active");
    require(dust3d_worker_service_queue_reply(queue.get()), "Active collector retained");
    submit(queue, 2);
    Dust3DWorkerDecodeResult result;
    require(!admit(queue, 1, result) && result.status == DUST3D_WORKER_DECODE_PROTOCOL_REJECTED, "Missing EOF rejects and releases active");
    const auto next = begin(queue, 2, 10);
    require(dust3d_worker_service_queue_abandon(queue.get(), next).status == DUST3D_WORKER_QUEUE_READY, "Transport cleanup releases next");
}
void invalidInputsAndBypass()
{
    Dust3DWorkerQueueResult created;
    require(!dust3d_worker_service_queue_create({ 0, 4096, 4096 }, &created)
            && created.status == DUST3D_WORKER_QUEUE_PROTOCOL_REJECTED,
        "Invalid quotas rejected before owner publication");
    require(!dust3d_worker_service_queue_create({ 4096, 4096, 4096 }, nullptr), "Required create receipt");
    for (bool shouldUseServiceAdmission : { false, true }) {
        auto queue = create();
        submit(queue, 1);
        const auto identity = begin(queue, 1);
        frameFailure(queue);
        auto* collector = dust3d_worker_service_queue_reply(queue.get());
        Dust3DWorkerDecodeResult result;
        Decoded decoded(shouldUseServiceAdmission
                ? dust3d_worker_service_admit(collector, identity, { 9, 101 }, receipt(1), 4096, &result)
                : dust3d_worker_reply_decode(collector, 4096, &result),
            dust3d_worker_decoded_destroy);
        require(!decoded && result.status == DUST3D_WORKER_DECODE_PROTOCOL_REJECTED, "Borrowed framing cannot bypass current-source queue");
        require(dust3d_worker_service_queue_abandon(queue.get(), identity).status == DUST3D_WORKER_QUEUE_READY, "Rejected bypass remains cleanable");
    }
    for (uint64_t budget : std::array<uint64_t, 2> { 0, 128ULL * 1024 * 1024 + 1 }) {
        auto queue = create();
        submit(queue, 1);
        begin(queue, 1);
        frameFailure(queue);
        submit(queue, 2);
        Dust3DWorkerDecodeResult result;
        Decoded decoded(dust3d_worker_service_queue_admit(queue.get(), { 9, 101 }, receipt(1), budget, &result), dust3d_worker_decoded_destroy);
        require(!decoded && result.status == DUST3D_WORKER_DECODE_PROTOCOL_REJECTED, "Unsupported budget metadata rejects without large allocation");
        begin(queue, 2, 10);
    }
    auto empty = create();
    require(dust3d_worker_service_queue_begin(empty.get(), 9).status == DUST3D_WORKER_QUEUE_IDLE, "Empty queue idle");
    require(!dust3d_worker_service_queue_request_bytes(empty.get(), nullptr), "Required byte count");
    size_t size = 999;
    require(!dust3d_worker_service_queue_request_bytes(empty.get(), &size) && !size, "Idle getter clears size");
    require(!dust3d_worker_service_queue_request_bytes(nullptr, &size) && !size, "Null getter clears size");
    require(!dust3d_worker_service_queue_reply(nullptr), "Null collector getter");
    require(dust3d_worker_service_queue_submit(nullptr, nullptr, 0).status == DUST3D_WORKER_QUEUE_PROTOCOL_REJECTED, "Null submission typed");
    require(dust3d_worker_service_queue_begin(nullptr, 9).status == DUST3D_WORKER_QUEUE_PROTOCOL_REJECTED, "Null begin typed");
    require(dust3d_worker_service_queue_abandon(nullptr, {}).status == DUST3D_WORKER_QUEUE_PROTOCOL_REJECTED, "Null abandonment typed");
    Dust3DWorkerDecodeResult result;
    require(!dust3d_worker_service_queue_admit(nullptr, { 9, 101 }, receipt(1), 4096, &result)
            && result.status == DUST3D_WORKER_DECODE_PROTOCOL_REJECTED,
        "Null admission typed");
    require(!dust3d_worker_service_queue_admit(empty.get(), { 9, 101 }, receipt(1), 4096, &result)
            && result.status == DUST3D_WORKER_DECODE_QUEUE_REJECTED,
        "Absent active attempt rejected");
    dust3d_worker_service_queue_close(empty.get());
    require(dust3d_worker_service_queue_submit(empty.get(), nullptr, 0).status == DUST3D_WORKER_QUEUE_REJECTED, "Closed before input copy");
    dust3d_worker_service_queue_close(nullptr);
    dust3d_worker_service_queue_cancel(nullptr, {});
    dust3d_worker_service_queue_destroy(nullptr);
}
UvMeshPayload sampleGeometry()
{
    UvMeshPayload value;
    value.geometry.vertices = {
        { { 0, 0, 0 }, { 0, 0, 1 }, { 1, 0, 0, 1 } },
        { { 1, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0, 1 } },
        { { 0, 1, 0 }, { 0, 0, 1 }, { 0, 0, 1, 1 } }
    };
    value.geometry.nodeCount = 2;
    value.geometry.partCount = 1;
    value.uv.triangles = { UvTriangle {} };
    value.uv.packedTextureSize = 128;
    return value;
}
void geometryOwnershipAndCodecErrors()
{
    const auto sample = sampleGeometry();
    for (Operation operation : { Operation::generateMesh, Operation::generateUvMesh }) {
        auto queue = create();
        submit(queue, 1, operation);
        begin(queue, 1);
        const auto body = operation == Operation::generateMesh ? encodeMeshPayload(sample.geometry, {}) : encodeUvMeshPayload(sample, {});
        frameBody(queue, FrameKind::response, body);
        auto control = receipt(1);
        control.operation = static_cast<uint32_t>(operation);
        Dust3DWorkerDecodeResult result;
        Decoded decoded(dust3d_worker_service_queue_admit(queue.get(), { 9, 101 }, control, 4096, &result), dust3d_worker_decoded_destroy);
        require(decoded && result.status == DUST3D_WORKER_DECODE_OK, "Base and atomic UV semantics admitted");
        queue.reset();
        const auto summary = dust3d_worker_decoded_summary(decoded.get());
        require(summary.vertex_count == 3 && summary.node_count == 2, "Geometry independent from destroyed coordinator");
        Dust3DWorkerVertex vertex;
        require(dust3d_worker_decoded_vertices(decoded.get(), 1, 1, &vertex) && vertex.position[0] == 1, "Copied vertex survives coordinator");
        require(summary.kind == (operation == Operation::generateMesh ? DUST3D_WORKER_DECODED_MESH : DUST3D_WORKER_DECODED_UV_MESH), "Original semantic variant preserved");
    }
    for (int testCase = 0; testCase < 4; ++testCase) {
        auto queue = create();
        const auto operation = testCase >= 2 ? Operation::generateUvMesh : Operation::generateMesh;
        submit(queue, 1, operation);
        begin(queue, 1);
        std::vector<uint8_t> body { 1 };
        if (testCase == 3) {
            body = encodeUvMeshPayload(sample, {});
            const auto offset = uvMeshPrefixBytes + encodeMeshPayload(sample.geometry, {}).size();
            require(offset < body.size(), "Inner UV version offset is bounded");
            body[offset] = 255;
        }
        frameBody(queue, testCase == 1 ? FrameKind::failure : FrameKind::response, std::move(body));
        auto control = receipt(1);
        control.operation = static_cast<uint32_t>(operation);
        Dust3DWorkerDecodeResult result;
        Decoded decoded(dust3d_worker_service_queue_admit(queue.get(), { 9, 101 }, control, 4096, &result), dust3d_worker_decoded_destroy);
        const Dust3DWorkerDetailSource expected[] = { DUST3D_WORKER_DETAIL_MESH, DUST3D_WORKER_DETAIL_FAILURE, DUST3D_WORKER_DETAIL_UV_MESH, DUST3D_WORKER_DETAIL_UV };
        require(!decoded && result.status == DUST3D_WORKER_DECODE_CODEC_REJECTED && result.detail_source == expected[testCase], "Original codec category retained");
        submit(queue, 2);
        begin(queue, 2, 10);
    }
}
void controlFailures()
{
    for (int testCase = 0; testCase < 5; ++testCase) {
        auto queue = create();
        submit(queue, 1);
        begin(queue, 1);
        frameFailure(queue);
        auto control = receipt(1);
        Dust3DWorkerServiceObservation observed { 9, 101 };
        if (testCase == 0)
            observed.connection_generation = 10;
        if (testCase == 1)
            control.process_identifier = 102;
        if (testCase == 2)
            control.request_identity = 2;
        if (testCase == 3)
            control.runtime_status = 7;
        if (testCase == 4)
            control.operation = 65537;
        Dust3DWorkerDecodeResult result;
        Decoded decoded(dust3d_worker_service_queue_admit(queue.get(), observed, control, 4096, &result), dust3d_worker_decoded_destroy);
        require(!decoded, "Invalid control rejected");
        require(result.status == (testCase == 4 ? DUST3D_WORKER_DECODE_PROTOCOL_REJECTED : DUST3D_WORKER_DECODE_ADMISSION_REJECTED), "Typed cause category");
        submit(queue, 2);
        begin(queue, 2, 10);
    }
}
}
int main()
{
    latestSource();
    retirementAndRecovery();
    controlFailures();
    geometryOwnershipAndCodecErrors();
    invalidInputsAndBypass();
    std::cout << "Pure C service coordinator ownership/current-source contracts passed\n";
}
