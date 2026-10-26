// SPDX-License-Identifier: MIT
#include <iostream>
#include <native/Engine/WorkerAdmission.h>
#include <native/Engine/WorkerQueue.h>
#include <stdexcept>
using namespace dust3d::worker;
namespace {
void require(bool value, const char* cause)
{
    if (!value)
        throw std::runtime_error(cause);
}
ImportedRenderPayload sample()
{
    ImportedRenderPayload value;
    value.surface.geometry.vertices = { { { 0, 0, 0 }, { 0, 0, 1 }, { 1, 1, 1, 1 } },
        { { 1, 0, 0 }, { 0, 0, 1 }, { 1, 1, 1, 1 } }, { { 0, 1, 0 }, { 0, 0, 1 }, { 1, 1, 1, 1 } } };
    value.surface.geometry.nodeCount = 3;
    value.surface.geometry.partCount = 1;
    value.surface.models = { "Model" };
    ImportedSurfaceTriangle triangle;
    triangle.modelIndex = 0;
    value.surface.triangles = { triangle };
    ImportedMaterial material;
    material.modelId = "Model";
    value.materials.materials = { material };
    value.images = { std::nullopt };
    return value;
}
}
int main()
{
    try {
        Header request;
        request.operation = Operation::generateImportedRender;
        request.identity.document[0] = 1;
        request.identity.request = 7;
        request.payloadBytes = 1;
        const auto value = sample();
        auto bytes = encodeImportedRenderPayload(value, {});
        auto header = request;
        header.kind = FrameKind::response;
        header.payloadBytes = bytes.size();
        auto wire = encodeFrame({ header, bytes }, {});
        PendingReply pending(request);
        auto admitted = pending.complete(request.identity, { true, 0 }, wire.data(), wire.size());
        require(std::get<ImportedRenderReply>(admitted).value->materials.materials[0].modelId == "Model", "Child admission retains complete imported ownership");
        ServiceObservation observed { 3, 123, true };
        ServiceReceipt receipt {};
        receipt.runtimeStatus = receipt.admissionStatus = 0;
        receipt.operation = request.operation;
        receipt.request = request.identity.request;
        receipt.process = 123;
        PendingServiceReply service(request, 3);
        require(std::get<ImportedRenderReply>(service.complete(request.identity, observed, receipt, wire.data(), wire.size())).value.has_value(), "Service admission uses exact imported semantics");
        RequestQueue queue;
        queue.submit({ request, { 1 } });
        require(queue.beginService(3).has_value(), "Queue fixture captures authoritative identity");
        require(std::get<ImportedRenderReply>(queue.completeService(request.identity, observed, receipt, wire.data(), wire.size())).value.has_value(), "Queue admission retains imported output");
        header.payloadBytes = 0;
        wire = encodeFrame({ header, {} }, {});
        PendingReply absent(request);
        require(!std::get<ImportedRenderReply>(absent.complete(request.identity, { true, 0 }, wire.data(), wire.size())).value, "Empty imported reply is successful absence, not failure or base mesh");
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
