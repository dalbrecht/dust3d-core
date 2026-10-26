// SPDX-License-Identifier: MIT
#include <Dust3DWorkerRuntime.h>
#include <cstring>
#include <iostream>
#include <memory>
#include <native/Engine/WorkerFailure.h>
#include <native/Engine/WorkerImportedRender.h>
#include <native/Engine/WorkerImportedRequest.h>
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
    request.operation = Operation::generateImportedRender;
    request.identity.document[0] = 1;
    request.identity.request = 7;
    const auto requestBody = encodeImportedRequest({ { 1 }, {} }, {});
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
            "Pure queue fixture freezes imported request identity");
        collector = dust3d_worker_service_queue_reply(queue.get());
    } else {
        framed.reset(route == 1 ? dust3d_worker_service_reply_create(expected.data(), expected.size(), 11, { 4096, 4096, 4096 }, &receipt)
                                : dust3d_worker_reply_create(expected.data(), expected.size(), { 4096, 4096, 4096 }, &receipt));
        collector = framed.get();
    }
    require(collector && dust3d_worker_reply_append(collector, wire.data(), wire.size()).status == DUST3D_WORKER_REPLY_READY
            && dust3d_worker_reply_finish(collector).status == DUST3D_WORKER_REPLY_FRAMED,
        "Synthetic imported frame validates and reaches fixture EOF");
    Dust3DWorkerDecodeResult result {};
    Dust3DWorkerServiceReceipt service {};
    service.operation = DUST3D_WORKER_OPERATION_IMPORTED_RENDER;
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
        require(!value && result.status == DUST3D_WORKER_DECODE_CODEC_REJECTED && result.detail_source == DUST3D_WORKER_DETAIL_IMPORTED_RENDER
                && result.detail_code == static_cast<int32_t>(ImportedRenderErrorCode::limitExceeded),
            "C imported policy rejects aggregate output before returning an owner");
        return value;
    }
    require(value && result.status == DUST3D_WORKER_DECODE_OK, "C semantic owner admits imported values");
    return value;
}
ImportedRenderPayload sample()
{
    ImportedRenderPayload value;
    value.surface.geometry.vertices = { { { 0, 0, 0 }, { 0, 0, 1 }, { 1, 1, 1, 1 } },
        { { 1, 0, 0 }, { 0, 0, 1 }, { 1, 1, 1, 1 } }, { { 0, 1, 0 }, { 0, 0, 1 }, { 1, 1, 1, 1 } } };
    value.surface.geometry.nodeCount = 3;
    value.surface.geometry.partCount = 1;
    value.surface.geometry.warning = "original warning";
    value.surface.models = { "é" };
    value.surface.components = { { "Component", 2 } };
    ImportedSurfaceTriangle triangle;
    triangle.modelIndex = triangle.componentIndex = 0;
    triangle.uvKnown = 1;
    triangle.uv = { { { -2, 0 }, { 4, 0 }, { 0, 7 } } };
    triangle.colors = { { { 1, 0, 0, 0 }, { 0, 1, 0, 0.5 }, { 0, 0, 1, 1 } } };
    triangle.colorEncoding = 1;
    value.surface.triangles = { triangle };
    ImportedMaterial material;
    material.modelId = "é";
    material.baseColor = { 0.1, 0.2, 0.3, 0.4 };
    material.alphaMode = 2;
    material.alphaCutoff = 1.5;
    material.isDoubleSided = 1;
    material.sampler = ImportedMaterialSampler { 9728, 9987, 33071, 33648 };
    material.image = ImportedEncodedImage { ImportedImageEncoding::jpeg, { 7, 8, 9 } };
    value.materials.materials = { material };
    value.images = { ImportedRenderImage { 1, 1, { 17, 23, 41, 0 } } };
    return value;
}
}
int main()
{
    try {
        for (int route : { 1, 2 }) {
            auto owned = decode(encodeImportedRenderPayload(sample(), {}), false, 4096, false, route);
            Dust3DWorkerImportedMaterial copied {};
            require(dust3d_worker_decoded_imported_material(owned.get(), 0, &copied) && copied.image_encoding == 2,
                "Service and queue owners preserve imported definitions after collectors release");
            auto noImported = decode({}, false, 4096, false, route);
            require(!dust3d_worker_decoded_imported_summary(noImported.get()).has_value, "Service and queue preserve successful absence");
            FailurePayload original { FailureStatus::unsupportedAsset, { 'a', 0, 255 }, FailureDetail { FailureDomain::imageDecoder, -42, 3, 1 } };
            auto receipt = decode(encodeFailurePayload(original, {}), true, 4096, false, route);
            require(dust3d_worker_decoded_failure_detail(receipt.get()).code == -42, "Service and queue retain detailed callback codes");
            require(!decode(encodeImportedRenderPayload(sample(), {}), false, 1, true, route), "Both C admission routes enforce imported aggregate policy");
        }
        auto source = sample();
        auto value = decode(encodeImportedRenderPayload(source, {}));
        source = {};
        const auto summary = dust3d_worker_decoded_summary(value.get());
        const auto imported = dust3d_worker_decoded_imported_summary(value.get());
        require(summary.kind == DUST3D_WORKER_DECODED_IMPORTED_RENDER && summary.vertex_count == 3 && summary.triangle_count == 1
                && imported.has_value && imported.model_count == 1 && imported.component_count == 1 && imported.decoded_pixel_bytes == 4,
            "Imported kind and copied pixel accounting remain distinct");
        Dust3DWorkerVertex vertices[3] {};
        require(dust3d_worker_decoded_vertices(value.get(), 0, 3, vertices) && vertices[1].position[0] == 1, "Existing geometry copy supports imported output");
        size_t count = 0;
        const auto* model = dust3d_worker_decoded_imported_model(value.get(), 0, &count);
        require(count == 2 && std::memcmp(model, "é", 2) == 0, "Model identity retains exact UTF8 bytes");
        const auto* component = dust3d_worker_decoded_imported_component(value.get(), 0, &count);
        require(std::string(component, count) == "Component" && dust3d_worker_decoded_imported_component_kind(value.get(), 0) == 2, "Component spelling and catalog membership survive");
        Dust3DWorkerImportedTriangle triangle {};
        require(dust3d_worker_decoded_imported_triangle(value.get(), 0, &triangle) && triangle.uv[0][0] == -2 && triangle.colors[1][3] == 0.5
                && triangle.color_encoding == 1 && triangle.model_index == 0 && triangle.uv_known == 1,
            "Authored triangle source scalars copy without conversion");
        Dust3DWorkerImportedMaterial material {};
        require(dust3d_worker_decoded_imported_material(value.get(), 0, &material) && material.base_color[0] == 0.1
                && material.alpha_mode == 2 && material.alpha_cutoff == 1.5 && material.is_double_sided
                && material.has_sampler && material.has_magnification_filter && material.magnification_filter == 9728
                && material.has_minification_filter && material.minification_filter == 9987 && material.wrap_s == 33071 && material.wrap_t == 33648
                && material.has_image && material.image_encoding == 2 && material.encoded_count == 3 && material.encoded[0] == 7,
            "Complete material and encoded sampler/image intent remain owner-scoped");
        Dust3DWorkerImportedImage image {};
        require(dust3d_worker_decoded_imported_image(value.get(), 0, &image) && image.width == 1 && image.height == 1
                && image.row_stride == 4 && image.byte_count == 4 && image.pixels[0] == 17 && !image.pixels[3],
            "Straight transparent source pixels stay independently owned");
        require(!dust3d_worker_decoded_imported_triangle(value.get(), 1, &triangle) && !triangle.uv_known
                && !dust3d_worker_decoded_imported_triangle(value.get(), 0, nullptr)
                && !dust3d_worker_decoded_imported_material(value.get(), 1, &material) && !material.model_id
                && !dust3d_worker_decoded_imported_material(value.get(), 0, nullptr),
            "Out-of-range and missing outputs reject with cleared receipts");
        Dust3DWorkerImportedImage missingImage {};
        require(!dust3d_worker_decoded_imported_image(value.get(), 1, &missingImage) && !missingImage.pixels
                && !dust3d_worker_decoded_imported_image(value.get(), 0, nullptr),
            "Image getter rejects invalid slots without borrowing storage");
        require(!dust3d_worker_decoded_imported_model(value.get(), 1, &count) && !count
                && !dust3d_worker_decoded_imported_model(value.get(), 0, nullptr)
                && !dust3d_worker_decoded_imported_component(value.get(), 1, &count) && !count
                && !dust3d_worker_decoded_imported_component(value.get(), 0, nullptr)
                && !dust3d_worker_decoded_imported_component_kind(value.get(), 1),
            "Catalog getters preserve bounded index and required size contracts");
        const auto* warning = dust3d_worker_decoded_warning(value.get(), &count);
        require(std::string(warning, count) == "original warning" && !dust3d_worker_decoded_failure_detail(value.get()).has_detail,
            "Imported geometry warning survives while unrelated failure fields remain absent");
        std::vector<uint8_t> pixels(image.pixels, image.pixels + image.byte_count);
        value.reset();
        require(pixels[0] == 17, "Swift-style explicit pixel copy outlives the decoded owner");
        auto absent = decode({});
        require(dust3d_worker_decoded_summary(absent.get()).kind == DUST3D_WORKER_DECODED_IMPORTED_RENDER
                && !dust3d_worker_decoded_imported_summary(absent.get()).has_value,
            "Successful absence preserves imported kind");
        require(!dust3d_worker_decoded_imported_model(absent.get(), 0, &count) && !count
                && !dust3d_worker_decoded_imported_triangle(absent.get(), 0, &triangle)
                && !dust3d_worker_decoded_imported_material(absent.get(), 0, &material)
                && !dust3d_worker_decoded_imported_image(absent.get(), 0, &missingImage),
            "Absent imported values expose no borrowed buffers");
        auto plain = sample();
        plain.materials.materials[0].image.reset();
        plain.materials.materials[0].sampler.reset();
        plain.images[0].reset();
        auto untextured = decode(encodeImportedRenderPayload(plain, {}));
        require(dust3d_worker_decoded_imported_material(untextured.get(), 0, &material) && !material.has_sampler && !material.has_image
                && dust3d_worker_decoded_imported_image(untextured.get(), 0, &missingImage) && !missingImage.pixels
                && !dust3d_worker_decoded_imported_summary(untextured.get()).decoded_pixel_bytes,
            "Untextured material and decoded-image absence stay explicit");
        require(!decode(encodeImportedRenderPayload(sample(), {}), false, 1, true), "Tight aggregate C policy rejects pure imported output");
        FailurePayload failure { FailureStatus::unsupportedAsset, { 'a', 0, 255 }, FailureDetail { FailureDomain::imageDecoder, -42, 3, 1 } };
        auto failed = decode(encodeFailurePayload(failure, {}), true);
        const auto detail = dust3d_worker_decoded_failure_detail(failed.get());
        require(detail.has_detail && detail.domain == 2 && detail.code == -42 && detail.status == 3 && detail.cause_truncated,
            "Original failure metadata is available without changing legacy result layouts");
        require(!dust3d_worker_decoded_imported_summary(failed.get()).has_value
                && !dust3d_worker_decoded_imported_material(failed.get(), 0, &material)
                && !dust3d_worker_decoded_imported_summary(nullptr).has_value
                && !dust3d_worker_decoded_failure_detail(nullptr).has_detail
                && !dust3d_worker_decoded_imported_component_kind(nullptr, 0),
            "Null and wrong-kind access remains scalar and nonthrowing");
        FailurePayload legacy { FailureStatus::generationFailed, {} };
        auto legacyError = decode(encodeFailurePayload(legacy, {}), true);
        require(!dust3d_worker_decoded_failure_detail(legacyError.get()).has_detail, "Legacy errors retain absent detailed metadata");
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
    return 0;
}
