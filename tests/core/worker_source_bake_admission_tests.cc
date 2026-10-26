// SPDX-License-Identifier: MIT
#include <iostream>
#include <native/Engine/WorkerAdmission.h>
#include <native/Engine/WorkerQueue.h>
using namespace dust3d::worker;
namespace {
SourceBakedPayload sample()
{
    SourceBakedPayload value;
    value.geometry.vertices = { { { -0.0F, 0, 0 }, { 0, 0, 1 }, { 1, 1, 1, 1 } },
        { { 1, 0, 0 }, { 0, 0, 1 }, { 1, 1, 1, 1 } }, { { 0, 1, 0 }, { 0, 0, 1 }, { 1, 1, 1, 1 } } };
    value.geometry.nodeCount = 3;
    value.geometry.partCount = 1;
    value.geometry.warning = "original κ";
    value.surface.models = { "é", "é" };
    value.surface.components = { { "Original", 1 }, { "Generated", 2 } };
    BakedSurfaceTriangle triangle;
    triangle.uv = { { { -0.0, 0 }, { 1, 0 }, { 0, 1 } } };
    triangle.uvKnown = triangle.policyKnown = triangle.tangentKnown = triangle.isDoubleSided = 1;
    triangle.alphaMode = 2;
    triangle.alphaCutoff = 1.5;
    triangle.tangent = { -0.0F, 1, 0 };
    triangle.handedness = -1;
    triangle.modelIndex = 1;
    triangle.componentIndex = 0;
    value.surface.triangles = { triangle };
    value.textures.maps[0] = { { 2, 2, std::vector<uint8_t>(16, 17) }, { 1, 1, { 1, 2, 3, 0 } } };
    value.textures.maximumSamplingLod = 0.5;
    return value;
}
}
int main()
{
    try {
        Header request;
        request.operation = Operation::generateSourceBaked;
        request.identity.document[0] = 1;
        request.identity.request = 17;
        request.payloadBytes = 1;
        auto response = request;
        response.kind = FrameKind::response;
        auto payload = encodeSourceBakedPayload(sample(), {});
        response.payloadBytes = payload.size();
        const auto wire = encodeFrame({ response, payload }, {});
        PendingReply child(request);
        auto accepted = child.complete(request.identity, { true, 0 }, wire.data(), wire.size());
        const auto& value = std::get<SourceBakedPayload>(accepted);
        if (value.textures.maps[0].size() != 2 || value.textures.maximumSamplingLod != 0.5 || value.surface.models[1] != "é")
            throw std::runtime_error("Child admission must retain coherent source output and actual mip cap");
        bool replayRejected = false;
        try {
            (void)child.complete(request.identity, { true, 0 }, wire.data(), wire.size());
        } catch (const AdmissionError& cause) {
            replayRejected = cause.code() == AdmissionErrorCode::retired;
        }
        if (!replayRejected)
            throw std::runtime_error("Source reply publishes only once");
        PendingReply bounded(request);
        SourceBakedLimits tight;
        tight.maximumDecodedBytes = 1;
        bool quotaRejected = false;
        try {
            (void)bounded.complete(request.identity, { true, 0 }, wire.data(), wire.size(), {}, {}, {}, {}, {}, {}, {}, {}, tight);
        } catch (const SourceBakedError& cause) {
            quotaRejected = cause.code() == SourceBakedErrorCode::limitExceeded;
        }
        if (!quotaRejected || !bounded.isRetired())
            throw std::runtime_error("Independent source quota rejection retires pending work");
        RequestQueue queue;
        queue.submit({ request, { 1 } });
        if (!queue.beginService(3))
            throw std::runtime_error("Queue records request");
        ServiceObservation observed { 3, 123, true };
        ServiceReceipt receipt {};
        receipt.runtimeStatus = receipt.admissionStatus = 0;
        receipt.operation = request.operation;
        receipt.request = request.identity.request;
        receipt.process = 123;
        const auto queued = queue.completeService(request.identity, observed, receipt, wire.data(), wire.size());
        if (encodeSourceBakedPayload(std::get<SourceBakedPayload>(queued), {}) != payload)
            throw std::runtime_error("Service queue retains all source-baked fields exactly");
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
    return 0;
}
