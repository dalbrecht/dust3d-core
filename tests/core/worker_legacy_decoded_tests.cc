// SPDX-License-Identifier: MIT
#include <Dust3DWorkerRuntime.h>
#include <cmath>
#include <iostream>
#include <memory>
#include <native/Engine/WorkerFailure.h>
#include <native/Engine/WorkerLegacyBaked.h>
#include <native/Engine/WorkerLegacyRequest.h>
#include <stdexcept>
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
    request.operation = Operation::generateLegacyBaked;
    request.identity.document[0] = 1;
    request.identity.request = 7;
    const auto requestBody = encodeLegacyRequest({ { 1 }, {} }, {});
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
    service.operation = DUST3D_WORKER_OPERATION_LEGACY_BAKED;
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
        require(!value && result.status == DUST3D_WORKER_DECODE_CODEC_REJECTED && result.detail_source == DUST3D_WORKER_DETAIL_LEGACY_BAKED
                && result.detail_code == static_cast<int32_t>(LegacyBakedErrorCode::limitExceeded),
            "C legacy policy rejects aggregate output before returning an owner");
        return value;
    }
    require(value && result.status == DUST3D_WORKER_DECODE_OK, "C semantic owner admits legacy values");
    return value;
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
        auto body = encodeLegacyBakedPayload(sample(), {});
        auto decoded = decode(body);
        const auto summary = dust3d_worker_decoded_summary(decoded.get());
        const auto baked = dust3d_worker_decoded_baked_summary(decoded.get());
        require(summary.kind == DUST3D_WORKER_DECODED_LEGACY_BAKED && summary.vertex_count == 6
                && summary.triangle_count == 2 && summary.chart_count == 1 && summary.gradient_count == 1 && baked.tangent_count == 2,
            "Distinct legacy kind exposes full existing geometry/UV metadata");
        Dust3DWorkerTextureTangent tangent {};
        require(dust3d_worker_decoded_baked_tangent(decoded.get(), 0, &tangent) && tangent.known
                && tangent.handedness == -1 && std::signbit(tangent.direction[0]),
            "Exact tangent bits survive C copying");
        Dust3DWorkerTextureView image {};
        require(dust3d_worker_decoded_baked_level_count(decoded.get(), 0) == 1
                && dust3d_worker_decoded_baked_texture(decoded.get(), 0, 0, &image) && image.byte_count == 4
                && image.width == 1 && image.height == 1 && image.row_stride == 4 && image.pixels[0] == 17 && !image.pixels[3],
            "Borrowed original base-map pixels survive collector release");
        Dust3DWorkerUvTriangle triangle {};
        Dust3DWorkerUvChart chart {};
        Dust3DWorkerUvGradient gradient {};
        require(dust3d_worker_decoded_triangle(decoded.get(), 0, &triangle) && triangle.known
                && dust3d_worker_decoded_chart(decoded.get(), 0, &chart) && chart.flipped
                && dust3d_worker_decoded_gradient(decoded.get(), 0, &gradient) && gradient.resolution == 64,
            "Existing UV accessors expose complete legacy triangle/chart/gradient fields");
        size_t keyBytes = 0;
        const char* key = dust3d_worker_decoded_component(decoded.get(), 0, &keyBytes);
        require(key && std::string(key, keyBytes) == "original-key", "Exact legacy component catalog survives");
        require(baked.pixel_bytes == 8 && baked.maximum_sampling_lod == 0, "Summary reports actual admitted base-map storage and sampling cap");
        Dust3DWorkerTextureView rejected = image;
        require(!dust3d_worker_decoded_baked_texture(decoded.get(), UINT32_MAX, 0, &rejected) && !rejected.pixels
                && !dust3d_worker_decoded_baked_texture(decoded.get(), 0, 1, &rejected)
                && !dust3d_worker_decoded_baked_texture(decoded.get(), 4, 0, &rejected)
                && !dust3d_worker_decoded_baked_texture(decoded.get(), 0, 0, nullptr),
            "Invalid role/level/absent-map/output rejects and clears spans");
        require(!dust3d_worker_decoded_baked_tangent(decoded.get(), 2, &tangent) && !tangent.known
                && !dust3d_worker_decoded_baked_tangent(decoded.get(), 0, nullptr),
            "Invalid tangent requests clear scalar outputs");
        require(!dust3d_worker_decoded_baked_level_count(nullptr, 0) && !dust3d_worker_decoded_baked_level_count(decoded.get(), UINT32_MAX)
                && !dust3d_worker_decoded_baked_summary(nullptr).tangent_count,
            "Null and unknown role summaries remain empty");
        for (int route = 0; route < 3; ++route) {
            auto admitted = decode(body, false, 4096, false, route);
            require(dust3d_worker_decoded_summary(admitted.get()).kind == DUST3D_WORKER_DECODED_LEGACY_BAKED, "All C admission routes preserve the distinct legacy kind");
            const auto logical = inspectLegacyBakedPayload(body.data(), body.size(), {}).logicalBytes;
            (void)decode(body, false, logical - 1, true, route);
            FailurePayload failure { FailureStatus::unsupportedAsset, { 'a', 0, 255 }, FailureDetail { FailureDomain::legacyProducer, -42, 3, 1 } };
            auto failed = decode(encodeFailurePayload(failure, {}), true, 4096, false, route);
            require(dust3d_worker_decoded_failure_detail(failed.get()).code == -42 && !dust3d_worker_decoded_baked_summary(failed.get()).tangent_count
                    && !dust3d_worker_decoded_baked_texture(failed.get(), 0, 0, &rejected),
                "Original detailed failures never masquerade as baked output");
        }
        const std::vector<uint8_t> copied(image.pixels, image.pixels + image.byte_count);
        decoded.reset();
        require(copied == std::vector<uint8_t>({ 17, 33, 49, 0 }), "Caller-copied pixels outlive decoded owner");
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
    return 0;
}
