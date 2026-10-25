// SPDX-License-Identifier: MIT
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <native/Engine/WorkerOperation.h>
#include <native/Engine/WorkerQueue.h>
#include <native/Engine/WorkerTransport.h>
#include <native/Engine/WorkerUvOperation.h>
#include <stdexcept>
using namespace dust3d::worker;
namespace {
void require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}
template <typename Error, typename Action>
void rejects(Action action)
{
    try {
        action();
    } catch (const Error&) {
        return;
    }
    throw std::runtime_error("Invalid UV completion admitted");
}
Frame makeRequest(const char* fixture)
{
    std::ifstream input(fixture, std::ios::binary);
    require(bool(input), "Licensed Capsule fixture opens");
    UvRequest request;
    request.archive = { std::istreambuf_iterator<char>(input), {} };
    require(request.archive.size() < 1024 * 1024, "Ordinary fixture stays small");
    request.textureSize = 64;
    Frame result;
    result.header.operation = Operation::generateUvMesh;
    result.header.identity.document[0] = 1;
    result.header.identity.request = 42;
    result.header.identity.revision = 3;
    result.payload = encodeUvRequest(request, {});
    result.header.payloadBytes = result.payload.size();
    return result;
}
void valueAdmission(const Frame& request, const Frame& response)
{
    const auto header = encodeHeader(request.header, {});
    require(header[18] == 0 && header[19] == 2 && decodeHeader(header.data(), header.size(), {}).operation == Operation::generateUvMesh, "Explicit UV opcode2 round trip");
    auto wire = encodeFrame(response, {});
    PendingReply pending(request.header);
    auto admitted = pending.complete(request.header.identity, { true, 0 }, wire.data(), wire.size());
    require(std::holds_alternative<UvMeshPayload>(admitted), "UV opcode admits paired type");
    rejects<AdmissionError>([&] { pending.complete(request.header.identity, { true, 0 }, wire.data(), wire.size()); });
    auto stale = request.header.identity;
    ++stale.revision;
    PendingReply stalePending(request.header);
    rejects<AdmissionError>([&] { stalePending.complete(stale, { true, 0 }, wire.data(), wire.size()); });
    PendingReply unsuccessful(request.header);
    rejects<AdmissionError>([&] { unsuccessful.complete(request.header.identity, { true, 1 }, wire.data(), wire.size()); });
    auto wrongOperation = response;
    wrongOperation.header.operation = Operation::generateMesh;
    const auto wrongWire = encodeFrame(wrongOperation, {});
    PendingReply wrongPending(request.header);
    rejects<ProtocolError>([&] { wrongPending.complete(request.header.identity, { true, 0 }, wrongWire.data(), wrongWire.size()); });
    UvMeshPayloadLimits tiny;
    tiny.maximumDecodedBytes = 1;
    PendingReply limited(request.header);
    rejects<UvMeshPayloadError>([&] { limited.complete(request.header.identity, { true, 0 }, wire.data(), wire.size(), {}, {}, {}, tiny); });
    RequestQueue queue;
    queue.submit(request);
    require(queue.begin().has_value(), "Queued UV request starts");
    auto result = queue.complete(request.header.identity, { true, 0 }, wire.data(), wire.size());
    require(std::holds_alternative<UvMeshPayload>(result), "Queue preserves UV operation decoder");
    RequestQueue limitedQueue;
    limitedQueue.submit(request);
    require(limitedQueue.begin().has_value(), "Limited queued request starts");
    rejects<UvMeshPayloadError>([&] { limitedQueue.complete(request.header.identity, { true, 0 }, wire.data(), wire.size(), {}, {}, tiny); });
    auto failure = response;
    failure.header.kind = FrameKind::failure;
    failure.payload = encodeFailurePayload({ FailureStatus::unsupportedAsset, { 'x', 255 } }, {});
    failure.header.payloadBytes = failure.payload.size();
    const auto failedWire = encodeFrame(failure, {});
    PendingReply failed(request.header);
    const auto cause = failed.complete(request.header.identity, { true, 0 }, failedWire.data(), failedWire.size());
    require(std::get<FailurePayload>(cause).cause == std::vector<uint8_t>({ 'x', 255 }), "UV failures retain original opaque diagnostic");
    auto wrongKind = request;
    wrongKind.header.kind = FrameKind::response;
    rejects<ProtocolError>([&] { generateUvMeshReply(wrongKind, {}); });
    auto wrongLength = request;
    ++wrongLength.header.payloadBytes;
    rejects<ProtocolError>([&] { generateUvMeshReply(wrongLength, {}); });
    auto baseRequest = request;
    baseRequest.header.operation = Operation::generateMesh;
    rejects<ProtocolError>([&] { generateUvMeshReply(baseRequest, {}); });
    rejects<ProtocolError>([&] { generateMeshReply(request, {}); });
}
void ordinaryProcess(const char* worker, const Frame& request, const Frame& expected)
{
    auto collected = runWorkerJob(worker, request);
    require(collected.outcome.exitedNormally && collected.outcome.exitCode == 0, "Actual fresh helper exits normally zero");
    const auto response = decodeFrame(collected.reply.data(), collected.reply.size(), {});
    validateResponseIdentity(response.header, request.header);
    require(response.payload == expected.payload, "Fresh helper preserves complete same-owner geometry/UV output");
    PendingReply pending(request.header);
    const auto admitted = pending.complete(request.header.identity, collected.outcome, collected.reply.data(), collected.reply.size());
    const auto& pair = std::get<UvMeshPayload>(admitted);
    require(pair.geometry.vertices.size() == 240 && pair.uv.triangles.size() == 80 && !pair.uv.charts.empty(), "Parent admits ordinary correlated UV pair");
    WorkerCleanupReport cleanup;
    WorkerAttemptReport attempt;
    WorkerSupervision supervision;
    supervision.cleanupReport = &cleanup;
    supervision.attemptReport = &attempt;
    const auto supervised = runSupervisedWorkerJob(worker, request, {}, supervision);
    require(supervised.outcome.exitedNormally && supervised.outcome.exitCode == 0 && !cleanup.attemptedStop && !attempt.captured, "Normal supervised UV helper requires no exceptional cleanup or partial report");
    PendingReply supervisedPending(request.header);
    require(std::holds_alternative<UvMeshPayload>(supervisedPending.complete(request.header.identity, supervised.outcome, supervised.reply.data(), supervised.reply.size())), "Supervised ordinary output still requires independent parent admission");
}
}
int main(int argc, char** argv)
{
    try {
        require(argc == 3, "Fresh helper and licensed fixture required");
        const auto request = makeRequest(argv[2]);
        const auto response = generateUvMeshReply(request, {});
        require(response.header.kind == FrameKind::response, "Ordinary direct UV reply succeeds");
        valueAdmission(request, response);
        ordinaryProcess(argv[1], request, response);
        std::cout << "UV worker dispatch and admission passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
