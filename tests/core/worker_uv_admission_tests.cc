// SPDX-License-Identifier: MIT
#include <iostream>
#include <native/Engine/WorkerQueue.h>
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
    throw std::runtime_error("Invalid UV admission accepted");
}
}
int main()
{
    try {
        Header request;
        request.operation = Operation::generateUvMesh;
        request.identity.document[0] = 1;
        request.identity.request = 1;
        UvMeshPayload pair;
        pair.geometry.vertices = { { { 0, 0, 0 }, { 0, 0, 1 }, { 1, 1, 1, 1 } }, { { 1, 0, 0 }, { 0, 0, 1 }, { 1, 1, 1, 1 } }, { { 0, 1, 0 }, { 0, 0, 1 }, { 1, 1, 1, 1 } } };
        pair.uv.triangles.resize(1);
        auto header = request;
        header.kind = FrameKind::response;
        auto payload = encodeUvMeshPayload(pair, {});
        header.payloadBytes = payload.size();
        auto wire = encodeFrame({ header, payload }, {});
        PendingReply pending(request);
        require(std::holds_alternative<UvMeshPayload>(pending.complete(request.identity, { true, 0 }, wire.data(), wire.size())), "Pure UV opcode admits paired payload");
        auto malformed = payload;
        // UV prefix starts after paired prefix and exact geometry wire span.
        const auto uvOffset = uvMeshPrefixBytes + encodeMeshPayload(pair.geometry, {}).size();
        malformed.at(uvOffset + uvPrefixBytes + 48) = 2;
        header.payloadBytes = malformed.size();
        const auto malformedWire = encodeFrame({ header, malformed }, {});
        PendingReply bad(request);
        rejects<UvPayloadError>([&] { bad.complete(request.identity, { true, 0 }, malformedWire.data(), malformedWire.size()); });
        require(bad.isRetired(), "Rejected UV payload retires before parsing; no partial geometry");
        rejects<AdmissionError>([&] { bad.complete(request.identity, { true, 0 }, wire.data(), wire.size()); });
        auto mismatched = header;
        mismatched.operation = Operation::generateMesh;
        const auto mismatchedWire = encodeFrame({ mismatched, payload }, {});
        PendingReply foreign(request);
        rejects<ProtocolError>([&] { foreign.complete(request.identity, { true, 0 }, mismatchedWire.data(), mismatchedWire.size()); });
        auto stale = request.identity;
        ++stale.revision;
        PendingReply old(request);
        rejects<AdmissionError>([&] { old.complete(stale, { true, 0 }, wire.data(), wire.size()); });
        RequestQueue queue;
        queue.submit({ request, {} });
        require(queue.begin().has_value(), "Pure UV queue starts");
        UvMeshPayloadLimits tiny;
        tiny.maximumDecodedBytes = 1;
        rejects<UvMeshPayloadError>([&] { queue.complete(request.identity, { true, 0 }, wire.data(), wire.size(), {}, {}, tiny); });
        request.identity.request = 2;
        queue.submit({ request, {} });
        require(queue.begin().has_value(), "Rejected queue completion releases active slot");
        header = request;
        header.kind = FrameKind::failure;
        payload = encodeFailurePayload({ FailureStatus::unsupportedAsset, { 'a', 255 } }, {});
        header.payloadBytes = payload.size();
        wire = encodeFrame({ header, payload }, {});
        const auto failure = queue.complete(request.identity, { true, 0 }, wire.data(), wire.size());
        require(std::get<FailurePayload>(failure).cause == std::vector<uint8_t>({ 'a', 255 }), "UV typed failure preserves opaque cause");
        request.operation = Operation::generateMesh;
        pair.geometry.warning = "base";
        payload = encodeMeshPayload(pair.geometry, {});
        header = request;
        header.kind = FrameKind::response;
        header.payloadBytes = payload.size();
        wire = encodeFrame({ header, payload }, {});
        PendingReply base(request);
        require(std::holds_alternative<MeshPayload>(base.complete(request.identity, { true, 0 }, wire.data(), wire.size())), "Base opcode retains original decoder/type");
        std::cout << "Pure UV admission passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
