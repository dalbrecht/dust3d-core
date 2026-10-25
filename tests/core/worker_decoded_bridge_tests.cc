// SPDX-License-Identifier: MIT
#include <Dust3DWorkerRuntime.h>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <memory>
#include <native/Engine/WorkerFailure.h>
#include <native/Engine/WorkerUvMesh.h>
#include <stdexcept>
using namespace dust3d::worker;
namespace {
void require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}
using Framed = std::unique_ptr<Dust3DWorkerReplyCollector, decltype(&dust3d_worker_reply_destroy)>;
using Decoded = std::unique_ptr<Dust3DWorkerDecodedReply, decltype(&dust3d_worker_decoded_destroy)>;
Framed frame(const std::vector<uint8_t>& body, Operation operation = Operation::generateMesh, FrameKind kind = FrameKind::response, bool finish = true)
{
    Header header;
    header.identity.document[0] = 1;
    header.identity.request = 42;
    header.identity.revision = 7;
    header.operation = operation;
    auto expected = encodeHeader(header, {});
    Dust3DWorkerReplyResult receipt {};
    Framed owner(dust3d_worker_reply_create(expected.data(), expected.size(), { 4096, 4096, 4096 }, &receipt), dust3d_worker_reply_destroy);
    require(owner && receipt.status == DUST3D_WORKER_REPLY_READY, "Framed owner created");
    header.kind = kind;
    header.payloadBytes = body.size();
    auto wire = encodeFrame({ header, body }, {});
    require(dust3d_worker_reply_append(owner.get(), wire.data(), wire.size()).status == DUST3D_WORKER_REPLY_READY, "Frame copied");
    if (finish)
        require(dust3d_worker_reply_finish(owner.get()).status == DUST3D_WORKER_REPLY_FRAMED, "Actual EOF supplied");
    return owner;
}
Decoded decode(Framed& owner, uint64_t maximum = 1048576)
{
    Dust3DWorkerDecodeResult result {};
    Decoded value(dust3d_worker_reply_decode(owner.get(), maximum, &result), dust3d_worker_decoded_destroy);
    require(value && result.status == DUST3D_WORKER_DECODE_OK && result.detail_source == DUST3D_WORKER_DETAIL_NONE && !result.message[0], "Original semantic value decoded");
    return value;
}
UvMeshPayload sample()
{
    UvMeshPayload value;
    value.geometry.vertices = { { { 0, 0, 0 }, { 0, 0, 1 }, { 1, 0, 0, 1 } }, { { 1, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0, 1 } }, { { 0, 1, 0 }, { 0, 0, 1 }, { 0, 0, 1, 1 } } };
    value.geometry.nodeCount = 2;
    value.geometry.partCount = 1;
    value.geometry.warning = "ok\n";
    UvTriangle triangle;
    triangle.known = 1;
    triangle.association = UvAssociation::component;
    triangle.componentIndex = 0;
    triangle.uv = { { { 0.125, 0.25 }, { 0.75, 0.25 }, { 0.125, 0.875 } } };
    value.uv.triangles = { triangle };
    value.uv.components = { "original-κ" };
    value.uv.packedTextureSize = 128;
    UvChart chart;
    chart.left = 0.125;
    chart.top = 0.25;
    chart.width = 0.5;
    chart.height = 0.5;
    chart.color = { 0.1, 0.2, 0.3, 0.4 };
    chart.flipped = 1;
    chart.association = UvAssociation::component;
    chart.componentIndex = 0;
    chart.imageID = "{11111111-2222-3333-4444-555555555555}";
    value.uv.charts = { chart };
    UvGradient gradient;
    gradient.chartIndex = 0;
    gradient.resolution = 257;
    gradient.largeColor = { 1, 0, 0, 1 };
    gradient.smallColor = { 0, 1, 0, 0.5 };
    value.uv.gradients = { gradient };
    return value;
}
void geometryAndAtlas()
{
    auto source = sample();
    auto bytes = encodeUvMeshPayload(source, {});
    auto framed = frame(bytes, Operation::generateUvMesh);
    auto decoded = decode(framed);
    framed.reset();
    bytes.assign(bytes.size(), 0);
    source = {};
    auto summary = dust3d_worker_decoded_summary(decoded.get());
    require(summary.kind == DUST3D_WORKER_DECODED_UV_MESH && summary.vertex_count == 3 && summary.node_count == 2 && summary.part_count == 1 && summary.triangle_count == 1 && summary.chart_count == 1 && summary.gradient_count == 1 && summary.component_count == 1 && summary.packed_texture_size == 128 && !summary.failure_status, "Independent UV summary retains all dimensions");
    Dust3DWorkerVertex vertices[3] {};
    require(dust3d_worker_decoded_vertices(decoded.get(), 0, 3, vertices) && vertices[1].position[0] == 1 && vertices[2].color[2] == 1 && vertices[0].normal[2] == 1, "Independent geometry copies match fixture");
    const auto saved = vertices[0];
    require(!dust3d_worker_decoded_vertices(decoded.get(), SIZE_MAX, 1, vertices) && !dust3d_worker_decoded_vertices(decoded.get(), 0, SIZE_MAX, vertices) && !memcmp(&saved, &vertices[0], sizeof(saved)), "Invalid vertex ranges do not write");
    require(!dust3d_worker_decoded_vertices(decoded.get(), 0, 1, nullptr) && dust3d_worker_decoded_vertices(decoded.get(), 3, 0, nullptr), "Empty valid range does not require storage");
    Dust3DWorkerUvTriangle triangle {};
    Dust3DWorkerUvChart chart {};
    Dust3DWorkerUvGradient gradient {};
    require(dust3d_worker_decoded_triangle(decoded.get(), 0, &triangle) && triangle.uv[2][1] == 0.875 && triangle.known == 1 && triangle.association == 1 && triangle.component_index == 0, "UV double precision and association retained");
    require(dust3d_worker_decoded_chart(decoded.get(), 0, &chart) && chart.left == 0.125 && chart.top == 0.25 && chart.width == 0.5 && chart.height == 0.5 && chart.color[3] == 0.4 && chart.flipped == 1 && chart.association == 1 && chart.component_index == 0 && std::string(chart.image_id) == "{11111111-2222-3333-4444-555555555555}", "Chart scalar/color/image values retained");
    require(dust3d_worker_decoded_gradient(decoded.get(), 0, &gradient) && gradient.chart_index == 0 && gradient.resolution == 257 && gradient.large_color[0] == 1 && gradient.small_color[3] == 0.5, "Gradient endpoint metadata retained");
    size_t count = 123;
    auto text = dust3d_worker_decoded_component(decoded.get(), 0, &count);
    require(text && std::string(text, count) == "original-κ", "Original UTF8 component spelling retained");
    text = dust3d_worker_decoded_warning(decoded.get(), &count);
    require(text && std::string(text, count) == "ok\n", "Warning bytes copied independently");
    require(!dust3d_worker_decoded_triangle(decoded.get(), 1, &triangle) && !triangle.known && !dust3d_worker_decoded_chart(decoded.get(), 1, &chart) && !chart.width && !dust3d_worker_decoded_gradient(decoded.get(), 1, &gradient) && !gradient.resolution, "Invalid index clears value output");
    require(!dust3d_worker_decoded_triangle(decoded.get(), 0, nullptr) && !dust3d_worker_decoded_chart(decoded.get(), 0, nullptr) && !dust3d_worker_decoded_gradient(decoded.get(), 0, nullptr), "Missing value outputs reject");
    require(!dust3d_worker_decoded_component(decoded.get(), 1, &count) && !count && !dust3d_worker_decoded_failure_cause(decoded.get(), &count) && !count, "Wrong kind/index clears borrowed counts");
}
void baseFailureAndNull()
{
    auto source = sample();
    auto framed = frame(encodeMeshPayload(source.geometry, {}));
    auto decoded = decode(framed);
    require(dust3d_worker_decoded_summary(decoded.get()).kind == DUST3D_WORKER_DECODED_MESH && !dust3d_worker_decoded_summary(decoded.get()).triangle_count, "Base result remains distinct from UV");
    Dust3DWorkerUvTriangle triangle {};
    Dust3DWorkerUvChart chart {};
    Dust3DWorkerUvGradient gradient {};
    require(!dust3d_worker_decoded_triangle(decoded.get(), 0, &triangle) && !dust3d_worker_decoded_chart(decoded.get(), 0, &chart) && !dust3d_worker_decoded_gradient(decoded.get(), 0, &gradient), "Base has no UV access");
    auto failure = frame(encodeFailurePayload({ FailureStatus::unsupportedAsset, { 0, 255, 42 } }, {}), Operation::generateUvMesh, FrameKind::failure);
    auto failed = decode(failure);
    failure.reset();
    auto summary = dust3d_worker_decoded_summary(failed.get());
    require(summary.kind == DUST3D_WORKER_DECODED_FAILURE && summary.failure_status == 2 && !summary.vertex_count, "Native failure is separate from geometry");
    size_t count = 0;
    auto cause = dust3d_worker_decoded_failure_cause(failed.get(), &count);
    require(cause && count == 3 && cause[0] == 0 && cause[1] == 255 && cause[2] == 42, "Opaque failure preserves NUL/nonUTF8");
    Dust3DWorkerVertex vertex {};
    require(!dust3d_worker_decoded_vertices(failed.get(), 0, 0, &vertex), "Failure cannot expose geometry");
    require(!dust3d_worker_decoded_warning(failed.get(), &count) && !count && !dust3d_worker_decoded_component(failed.get(), 0, &count) && !count, "Failure has no mesh/UV strings");
    require(dust3d_worker_decoded_summary(nullptr).kind == DUST3D_WORKER_DECODED_NONE && !dust3d_worker_decoded_vertices(nullptr, 0, 0, nullptr), "Null scalar/copy consumers harmless");
    require(!dust3d_worker_decoded_triangle(nullptr, 0, &triangle) && !dust3d_worker_decoded_chart(nullptr, 0, &chart) && !dust3d_worker_decoded_gradient(nullptr, 0, &gradient), "Null value consumers clear output");
    require(!dust3d_worker_decoded_failure_cause(nullptr, &count) && !count && !dust3d_worker_decoded_warning(nullptr, &count) && !count && !dust3d_worker_decoded_component(nullptr, 0, &count) && !count, "Null byte consumers clear counts");
    require(!dust3d_worker_decoded_failure_cause(failed.get(), nullptr) && !dust3d_worker_decoded_warning(decoded.get(), nullptr) && !dust3d_worker_decoded_component(decoded.get(), 0, nullptr), "Null byte counts reject");
    dust3d_worker_decoded_destroy(nullptr);
}
void rejected(Framed& owner, uint64_t maximum, Dust3DWorkerDecodeStatus status, Dust3DWorkerDetailSource source, int code)
{
    Dust3DWorkerDecodeResult result {};
    require(!dust3d_worker_reply_decode(owner.get(), maximum, &result) && result.status == status && result.detail_source == source && result.detail_code == code && result.message[0] && !result.message[511] && !result.message_truncated, "Original semantic rejection retained without result");
    const auto saved = result;
    require(!dust3d_worker_reply_decode(owner.get(), 1048576, &result) && result.status == DUST3D_WORKER_DECODE_PROTOCOL_REJECTED, "Rejected semantic owner cannot replay");
    require(saved.detail_code == code && saved.message[0], "Original receipt self-owned across later calls");
    size_t size = 42;
    require(!dust3d_worker_reply_bytes(owner.get(), &size) && !size, "Semantic rejection hides framed bytes");
}
void policyAndMalformed()
{
    auto source = sample();
    auto mesh = encodeMeshPayload(source.geometry, {});
    auto pair = encodeUvMeshPayload(source, {});
    auto framed = frame(mesh, Operation::generateMesh, FrameKind::response, false);
    rejected(framed, 1048576, DUST3D_WORKER_DECODE_PROTOCOL_REJECTED, DUST3D_WORKER_DETAIL_FRAME, static_cast<int>(ProtocolErrorCode::invalidInput));
    for (uint64_t budget : { uint64_t(0), 128ULL * 1024 * 1024 + 1 }) {
        framed = frame(mesh);
        rejected(framed, budget, DUST3D_WORKER_DECODE_PROTOCOL_REJECTED, DUST3D_WORKER_DETAIL_FRAME, static_cast<int>(ProtocolErrorCode::invalidLimits));
    }
    const auto exact = inspectMeshPayload(mesh.data(), mesh.size(), {}).logicalBytes;
    framed = frame(mesh);
    auto exactMesh = decode(framed, exact);
    require(dust3d_worker_decoded_summary(exactMesh.get()).vertex_count == 3, "Exact mesh logical budget accepts");
    framed = frame(mesh);
    rejected(framed, exact - 1, DUST3D_WORKER_DECODE_CODEC_REJECTED, DUST3D_WORKER_DETAIL_MESH, static_cast<int>(MeshPayloadErrorCode::limitExceeded));
    const auto uv = encodeUvPayload(source.uv, 1, {});
    const auto aggregate = exact + inspectUvPayload(uv.data(), uv.size(), 1, {});
    framed = frame(pair, Operation::generateUvMesh);
    auto exactPair = decode(framed, aggregate);
    require(dust3d_worker_decoded_summary(exactPair.get()).chart_count == 1, "Exact aggregate logical budget accepts");
    framed = frame(pair, Operation::generateUvMesh);
    rejected(framed, aggregate - 1, DUST3D_WORKER_DECODE_CODEC_REJECTED, DUST3D_WORKER_DETAIL_UV_MESH, static_cast<int>(UvMeshPayloadErrorCode::limitExceeded));
    framed = frame({ 0 });
    rejected(framed, 1048576, DUST3D_WORKER_DECODE_CODEC_REJECTED, DUST3D_WORKER_DETAIL_MESH, static_cast<int>(MeshPayloadErrorCode::truncated));
    auto badPair = pair;
    badPair[1] = 2;
    framed = frame(badPair, Operation::generateUvMesh);
    rejected(framed, 1048576, DUST3D_WORKER_DECODE_CODEC_REJECTED, DUST3D_WORKER_DETAIL_UV_MESH, static_cast<int>(UvMeshPayloadErrorCode::unsupportedVersion));
    badPair = pair;
    badPair[24 + mesh.size() + 56 + 48] = 2;
    framed = frame(badPair, Operation::generateUvMesh);
    rejected(framed, 1048576, DUST3D_WORKER_DECODE_CODEC_REJECTED, DUST3D_WORKER_DETAIL_UV, static_cast<int>(UvPayloadErrorCode::invalidMetadata));
    framed = frame(encodeFailurePayload({ FailureStatus::generationFailed, { 1, 2 } }, {}), Operation::generateMesh, FrameKind::failure);
    rejected(framed, 1, DUST3D_WORKER_DECODE_CODEC_REJECTED, DUST3D_WORKER_DETAIL_FAILURE, static_cast<int>(FailurePayloadErrorCode::limitExceeded));
    Dust3DWorkerDecodeResult result {};
    require(!dust3d_worker_reply_decode(nullptr, 1048576, &result) && result.status == DUST3D_WORKER_DECODE_PROTOCOL_REJECTED, "Null framed owner rejects");
    framed = frame(mesh);
    require(!dust3d_worker_reply_decode(framed.get(), 1048576, nullptr), "Null receipt does not consume");
    auto decoded = decode(framed);
    require(!dust3d_worker_reply_decode(framed.get(), 1048576, &result) && result.status == DUST3D_WORKER_DECODE_PROTOCOL_REJECTED, "Successful semantic owner consumed only once");
    require(dust3d_worker_decoded_summary(decoded.get()).vertex_count == 3, "Existing semantic values survive replay rejection");
    require(dust3d_worker_reply_append(framed.get(), nullptr, 0).status == DUST3D_WORKER_REPLY_PROTOCOL_REJECTED, "Consumed stream cannot modify");
}
}
int main()
{
    try {
        geometryAndAtlas();
        baseFailureAndNull();
        policyAndMalformed();
        std::cout << "Semantic C mesh/UV/failure ownership passed\n";
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}
