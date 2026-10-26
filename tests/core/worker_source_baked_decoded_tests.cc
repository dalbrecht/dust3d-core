// SPDX-License-Identifier: MIT
#include <Dust3DWorkerRuntime.h>
#include <iostream>
#include <memory>
#include <native/Engine/WorkerSourceBakeRequest.h>
#include <native/Engine/WorkerSourceBaked.h>
using namespace dust3d::worker;
namespace {
void require(bool value, const char* cause)
{
    if (!value)
        throw std::runtime_error(cause);
}
using Decoded = std::unique_ptr<Dust3DWorkerDecodedReply, decltype(&dust3d_worker_decoded_destroy)>;
Decoded decode(std::vector<uint8_t> bytes, bool failure = false, uint64_t maximum = 4096, bool isLimited = false, int route = 0)
{
    Header request;
    request.operation = Operation::generateSourceBaked;
    request.identity.document[0] = 1;
    request.identity.request = 7;
    const auto requestBody = encodeSourceBakeRequest({ { { 1 }, {} }, {} }, {});
    request.payloadBytes = requestBody.size();
    const auto expected = encodeHeader(request, {});
    auto response = request;
    response.kind = failure ? FrameKind::failure : FrameKind::response;
    response.payloadBytes = bytes.size();
    auto wire = encodeFrame({ response, bytes }, {});
    Dust3DWorkerReplyResult receipt {};
    std::unique_ptr<Dust3DWorkerReplyCollector, decltype(&dust3d_worker_reply_destroy)> framed(nullptr, dust3d_worker_reply_destroy);
    std::unique_ptr<Dust3DWorkerServiceQueue, decltype(&dust3d_worker_service_queue_destroy)> queue(nullptr, dust3d_worker_service_queue_destroy);
    Dust3DWorkerReplyCollector* collector;
    if (route == 2) {
        Dust3DWorkerQueueResult state {};
        queue.reset(dust3d_worker_service_queue_create({ 4096, 4096, 4096 }, &state));
        const auto submitted = encodeFrame({ request, requestBody }, {});
        require(queue && dust3d_worker_service_queue_submit(queue.get(), submitted.data(), submitted.size()).status == DUST3D_WORKER_QUEUE_READY
                && dust3d_worker_service_queue_begin(queue.get(), 11).status == DUST3D_WORKER_QUEUE_BEGUN,
            "Pure queue fixture freezes legacy request identity");
        collector = dust3d_worker_service_queue_reply(queue.get());
    } else {
        framed.reset(route == 1 ? dust3d_worker_service_reply_create(expected.data(), expected.size(), 11, { 4096, 4096, 4096 }, &receipt)
                                : dust3d_worker_reply_create(expected.data(), expected.size(), { 4096, 4096, 4096 }, &receipt));
        collector = framed.get();
    }
    require(collector && dust3d_worker_reply_append(collector, wire.data(), wire.size()).status == DUST3D_WORKER_REPLY_READY
            && dust3d_worker_reply_finish(collector).status == DUST3D_WORKER_REPLY_FRAMED,
        "Synthetic legacy frame validates and reaches fixture EOF");
    Dust3DWorkerDecodeResult result {};
    Dust3DWorkerServiceReceipt service {};
    service.operation = DUST3D_WORKER_OPERATION_SOURCE_BAKED;
    service.request_identity = request.identity.request;
    service.process_identifier = 123;
    Dust3DWorkerRequestIdentity active {};
    active.document[0] = 1;
    active.request = request.identity.request;
    Decoded value(route == 2 ? dust3d_worker_service_queue_admit(queue.get(), { 11, 123 }, service, maximum, &result)
            : route == 1     ? dust3d_worker_service_admit(collector, active, { 11, 123 }, service, maximum, &result)
                             : dust3d_worker_reply_decode(collector, maximum, &result),
        dust3d_worker_decoded_destroy);
    if (isLimited) {
        require(!value && result.status == DUST3D_WORKER_DECODE_CODEC_REJECTED && result.detail_source == DUST3D_WORKER_DETAIL_SOURCE_BAKED
                && result.detail_code == static_cast<int32_t>(SourceBakedErrorCode::limitExceeded),
            "C legacy policy rejects aggregate output before returning an owner");
        return value;
    }
    require(value && result.status == DUST3D_WORKER_DECODE_OK, "C semantic owner admits legacy values");
    return value;
}
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
        const auto bytes = encodeSourceBakedPayload(sample(), {});
        for (int route : { 0, 1, 2 }) {
            const auto owner = decode(bytes, false, 4096, false, route);
            const auto summary = dust3d_worker_decoded_summary(owner.get());
            const auto baked = dust3d_worker_decoded_baked_summary(owner.get());
            require(summary.kind == DUST3D_WORKER_DECODED_SOURCE_BAKED && summary.vertex_count == 3 && summary.triangle_count == 1
                    && baked.tangent_count == 1 && baked.pixel_bytes == 20 && baked.maximum_sampling_lod == 0.5,
                "Distinct source kind and actual mip storage survive C child/service/queue decoding");
            const auto surface = dust3d_worker_decoded_baked_surface_summary(owner.get());
            require(surface.model_count == 2 && surface.component_count == 2 && surface.triangle_count == 1,
                "Source catalogs preserve complete distinct counts");
            Dust3DWorkerBakedTriangle triangle {};
            require(dust3d_worker_decoded_baked_triangle(owner.get(), 0, &triangle)
                    && triangle.uv_known && triangle.policy_known && triangle.alpha_mode == 2 && triangle.alpha_cutoff == 1.5
                    && triangle.is_double_sided && triangle.model_index == 1 && triangle.component_index == 0,
                "Packed source UV and policy fields survive independent C value copy");
            size_t identitySize = 0;
            const auto* model = dust3d_worker_decoded_baked_model(owner.get(), 1, &identitySize);
            require(model && std::string(model, identitySize) == "é", "Exact Unicode model identity survives C borrowing");
            const auto* component = dust3d_worker_decoded_baked_component(owner.get(), 1, &identitySize);
            require(component && std::string(component, identitySize) == "Generated"
                    && dust3d_worker_decoded_baked_component_kind(owner.get(), 1) == 2,
                "Generated membership remains explicit without inventing provenance");
            Dust3DWorkerTextureTangent tangent {};
            require(dust3d_worker_decoded_baked_tangent(owner.get(), 0, &tangent) && tangent.known && tangent.handedness == -1,
                "Source triangle tangent survives C value copy");
            Dust3DWorkerTextureView view {};
            require(dust3d_worker_decoded_baked_level_count(owner.get(), 0) == 2
                    && dust3d_worker_decoded_baked_texture(owner.get(), 0, 1, &view) && view.byte_count == 4 && view.pixels[0] == 1,
                "Owned source mip pixels survive collector destruction");
        }
        Dust3DWorkerBakedTriangle cleared {};
        cleared.alpha_mode = 99;
        size_t clearedSize = 99;
        require(!dust3d_worker_decoded_baked_triangle(nullptr, 0, &cleared) && cleared.alpha_mode == 0
                && !dust3d_worker_decoded_baked_triangle(nullptr, 0, nullptr)
                && !dust3d_worker_decoded_baked_model(nullptr, 0, &clearedSize) && clearedSize == 0
                && !dust3d_worker_decoded_baked_component(nullptr, 0, nullptr),
            "Rejected source accessor calls clear outputs and avoid borrowed storage");
        FailurePayload failure { FailureStatus::unsupportedAsset, { 'a', 0, 255 }, FailureDetail { FailureDomain::imageDecoder, -42, 3, 1 },
            FailureResourceContext { FailureResourceRole::importedImage, "é" } };
        const auto failedOwner = decode(encodeFailurePayload(failure, {}), true);
        const auto context = dust3d_worker_decoded_failure_context(failedOwner.get());
        require(context.role == 2 && context.identity && std::string(context.identity, context.identity_size) == "é",
            "Contextual failure C view preserves exact image role and resource identity");
        require(!dust3d_worker_decoded_failure_context(nullptr).identity,
            "Missing failure context returns an empty view");
        const auto shape = inspectSourceBakedPayload(bytes.data(), bytes.size(), {});
        (void)decode(bytes, false, shape.logicalBytes - 1, true);
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
    return 0;
}
