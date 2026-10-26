// SPDX-License-Identifier: MIT
#include <iostream>
#include <native/Engine/WorkerAdmission.h>
#include <native/Engine/WorkerLegacyBaked.h>
#include <native/Engine/WorkerQueue.h>
#include <stdexcept>
using namespace dust3d::worker;
namespace {
void require(bool value, const char* cause)
{
    if (!value)
        throw std::runtime_error(cause);
}
LegacyBakedPayload sample()
{
    LegacyBakedPayload value;
    value.geometry.vertices = { { { -0.0F, 0, 0 }, { 0, 0, 1 }, { 1, 0, 0, 1 } },
        { { 1, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0, 1 } }, { { 0, 1, 0 }, { 0, 0, 1 }, { 0, 0, 1, 1 } } };
    const auto first = value.geometry.vertices;
    value.geometry.vertices.insert(value.geometry.vertices.end(), first.begin(), first.end());
    value.geometry.nodeCount = 3;
    value.geometry.partCount = 1;
    value.geometry.warning = "original κ";
    UvTriangle mapped;
    mapped.uv = { { { -0.0, 0 }, { 1, 0 }, { 0, 1 } } };
    mapped.known = 1;
    mapped.association = UvAssociation::component;
    mapped.componentIndex = 0;
    value.uv.triangles = { mapped, UvTriangle {} };
    value.uv.components = { "original-key" };
    value.uv.packedTextureSize = 512;
    UvChart chart;
    chart.width = chart.height = 1;
    chart.color = { 1, 0.5, 0, 1 };
    chart.flipped = 1;
    chart.association = UvAssociation::component;
    chart.componentIndex = 0;
    chart.imageID = "{eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee}";
    value.uv.charts = { chart };
    UvGradient gradient;
    gradient.largeColor = { 1, 0, 0, 1 };
    gradient.smallColor = { 0, 0, 1, 0.5 };
    gradient.resolution = 64;
    value.uv.gradients = { gradient };
    value.tangents = { { { -0.0F, 1, 0 }, -1, 1 }, { { 0, 0, 0 }, 1, 0 } };
    value.textures.maps[0] = { { 1, 1, { 17, 33, 49, 0 } } };
    value.textures.maps[1] = { { 1, 1, { 128, 128, 255, 255 } } };
    return value;
}
}
int main()
{
    try {
        Header request;
        request.operation = Operation::generateLegacyBaked;
        request.identity.document[0] = 1;
        request.identity.request = 7;
        request.payloadBytes = 1;
        const auto value = sample();
        auto bytes = encodeLegacyBakedPayload(value, {});
        auto header = request;
        header.kind = FrameKind::response;
        header.payloadBytes = bytes.size();
        auto wire = encodeFrame({ header, bytes }, {});
        PendingReply pending(request);
        auto admitted = pending.complete(request.identity, { true, 0 }, wire.data(), wire.size());
        require(std::get<LegacyBakedPayload>(admitted).textures.maps[0][0].pixels[0] == 17, "Child admission retains complete legacy ownership");
        ServiceObservation observed { 3, 123, true };
        ServiceReceipt receipt {};
        receipt.runtimeStatus = receipt.admissionStatus = 0;
        receipt.operation = request.operation;
        receipt.request = request.identity.request;
        receipt.process = 123;
        PendingServiceReply service(request, 3);
        require(!std::get<LegacyBakedPayload>(service.complete(request.identity, observed, receipt, wire.data(), wire.size())).tangents.empty(), "Service admission uses exact legacy semantics");
        RequestQueue queue;
        queue.submit({ request, { 1 } });
        require(queue.beginService(3).has_value(), "Queue fixture captures authoritative identity");
        require(!std::get<LegacyBakedPayload>(queue.completeService(request.identity, observed, receipt, wire.data(), wire.size())).uv.gradients.empty(), "Queue admission retains legacy output");
        FailurePayload failure { FailureStatus::unsupportedAsset, { 'a', 0, 255 }, FailureDetail { FailureDomain::imageDecoder, -42, 3, 1 } };
        header.kind = FrameKind::failure;
        bytes = encodeFailurePayload(failure, {});
        header.payloadBytes = bytes.size();
        wire = encodeFrame({ header, bytes }, {});
        PendingReply failed(request);
        const auto error = std::get<FailurePayload>(failed.complete(request.identity, { true, 0 }, wire.data(), wire.size()));
        require(error.detail && error.detail->code == -42 && error.detail->status == 3 && error.cause == failure.cause, "Original callback failure metadata survives semantic admission");
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
    return 0;
}
