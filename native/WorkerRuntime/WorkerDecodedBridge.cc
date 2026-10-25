// SPDX-License-Identifier: MIT
#include "WorkerAdmission.h"
#include "WorkerFailure.h"
#include "WorkerReplyInternal.h"
#include "WorkerUvMesh.h"
#include <algorithm>
#include <iterator>
#include <memory>
#include <new>
#include <variant>
using namespace dust3d::worker;
struct Dust3DWorkerDecodedReply {
    AdmittedReply value;
};
namespace {
void copyCause(Dust3DWorkerDecodeResult& result, const char* cause) noexcept
{
    if (!cause)
        return;
    size_t size = 0;
    while (size < sizeof(result.message) - 1 && cause[size]) {
        result.message[size] = cause[size];
        ++size;
    }
    result.message[size] = 0;
    result.message_truncated = cause[size] != 0;
}
template <typename Error>
void rejectCodec(Dust3DWorkerDecodeResult& result, Dust3DWorkerDetailSource source, const Error& cause) noexcept
{
    result.status = DUST3D_WORKER_DECODE_CODEC_REJECTED;
    result.detail_source = source;
    result.detail_code = static_cast<int32_t>(cause.code());
    copyCause(result, cause.what());
}
const MeshPayload* geometry(const Dust3DWorkerDecodedReply* reply) noexcept
{
    if (!reply)
        return nullptr;
    if (auto* mesh = std::get_if<MeshPayload>(&reply->value))
        return mesh;
    if (auto* pair = std::get_if<UvMeshPayload>(&reply->value))
        return &pair->geometry;
    return nullptr;
}
const UvPayload* uv(const Dust3DWorkerDecodedReply* reply) noexcept
{
    if (!reply)
        return nullptr;
    const auto* pair = std::get_if<UvMeshPayload>(&reply->value);
    return pair ? &pair->uv : nullptr;
}
}
namespace {
template <typename Action>
Dust3DWorkerDecodedReply* protectDecoded(Dust3DWorkerReplyCollector* owner, uint64_t maximum, Dust3DWorkerDecodeResult* result, Action action) noexcept
{
    if (!result)
        return nullptr;
    *result = {};
    result->status = DUST3D_WORKER_DECODE_INTERNAL_FAILED;
    try {
        if (!owner || owner->isRejected || !owner->isFramed || owner->isDecoded)
            throw ProtocolError(ProtocolErrorCode::invalidInput, "Semantic decoding requires one unused EOF-framed owner.");
        owner->isDecoded = true;
        if (!maximum || maximum > 128ULL * 1024 * 1024)
            throw ProtocolError(ProtocolErrorCode::invalidLimits, "Semantic decoded-byte budget is unsupported.");
        auto decoded = std::make_unique<Dust3DWorkerDecodedReply>();
        decoded->value = action(*owner, maximum);
        result->status = DUST3D_WORKER_DECODE_OK;
        return decoded.release();
    } catch (const AdmissionError& cause) {
        result->status = DUST3D_WORKER_DECODE_ADMISSION_REJECTED;
        result->detail_source = DUST3D_WORKER_DETAIL_ADMISSION;
        result->detail_code = static_cast<int32_t>(cause.code());
        copyCause(*result, cause.what());
    } catch (const ProtocolError& cause) {
        result->status = DUST3D_WORKER_DECODE_PROTOCOL_REJECTED;
        result->detail_source = DUST3D_WORKER_DETAIL_FRAME;
        result->detail_code = static_cast<int32_t>(cause.code());
        copyCause(*result, cause.what());
    } catch (const MeshPayloadError& cause) {
        rejectCodec(*result, DUST3D_WORKER_DETAIL_MESH, cause);
    } catch (const UvPayloadError& cause) {
        rejectCodec(*result, DUST3D_WORKER_DETAIL_UV, cause);
    } catch (const UvMeshPayloadError& cause) {
        rejectCodec(*result, DUST3D_WORKER_DETAIL_UV_MESH, cause);
    } catch (const FailurePayloadError& cause) {
        rejectCodec(*result, DUST3D_WORKER_DETAIL_FAILURE, cause);
    } catch (const std::bad_alloc& cause) {
        result->status = DUST3D_WORKER_DECODE_LIMIT_EXCEEDED;
        result->detail_source = DUST3D_WORKER_DETAIL_RUNTIME;
        copyCause(*result, cause.what());
    } catch (const std::exception& cause) {
        result->detail_source = DUST3D_WORKER_DETAIL_RUNTIME;
        copyCause(*result, cause.what());
    } catch (...) {
        result->detail_source = DUST3D_WORKER_DETAIL_RUNTIME;
        copyCause(*result, "Semantic worker exception has no available diagnostic.");
    }
    if (owner)
        owner->isRejected = true;
    return nullptr;
}
}
extern "C" Dust3DWorkerDecodedReply* dust3d_worker_reply_decode(Dust3DWorkerReplyCollector* owner, uint64_t maximum, Dust3DWorkerDecodeResult* result) noexcept
{
    return protectDecoded(owner, maximum, result, [](Dust3DWorkerReplyCollector& value, uint64_t maximum) -> AdmittedReply {
        auto* owner = &value;
        if (owner->serviceConnectionGeneration)
            throw ProtocolError(ProtocolErrorCode::invalidInput, "Service collector requires service admission.");
        const auto header = decodeHeader(owner->bytes.data(), headerBytes, owner->policy);
        validateResponseIdentity(header, owner->expected);
        const auto* body = owner->bytes.data() + headerBytes;
        const auto size = owner->bytes.size() - headerBytes;
        if (header.kind == FrameKind::failure) {
            FailurePayloadLimits policy;
            policy.maximumWireBytes = std::min(policy.maximumWireBytes, owner->policy.maximumFailureBytes);
            policy.maximumCauseBytes = std::min({ policy.maximumCauseBytes, maximum, policy.maximumWireBytes });
            return decodeFailurePayload(body, size, policy);
        } else if (header.operation == Operation::generateMesh) {
            MeshPayloadLimits policy;
            policy.maximumWireBytes = owner->policy.maximumOutputBytes;
            policy.maximumDecodedBytes = maximum;
            return decodeMeshPayload(body, size, policy);
        } else {
            UvMeshPayloadLimits policy;
            policy.maximumWireBytes = owner->policy.maximumOutputBytes;
            policy.maximumDecodedBytes = maximum;
            policy.geometry.maximumWireBytes = policy.maximumWireBytes;
            policy.geometry.maximumDecodedBytes = maximum;
            policy.uv.maximumWireBytes = policy.maximumWireBytes;
            policy.uv.maximumDecodedBytes = maximum;
            return decodeUvMeshPayload(body, size, policy);
        }
    });
}
extern "C" Dust3DWorkerDecodedReply* dust3d_worker_service_admit(Dust3DWorkerReplyCollector* owner,
    Dust3DWorkerRequestIdentity active, Dust3DWorkerServiceObservation observed,
    Dust3DWorkerServiceReceipt receipt, uint64_t maximum, Dust3DWorkerDecodeResult* result) noexcept
{
    return protectDecoded(owner, maximum, result, [=](Dust3DWorkerReplyCollector& value, uint64_t maximum) -> AdmittedReply {
        if (!value.serviceConnectionGeneration)
            throw ProtocolError(ProtocolErrorCode::invalidInput, "Service admission requires a service-specific collector.");
        if (receipt.operation != DUST3D_WORKER_OPERATION_MESH && receipt.operation != DUST3D_WORKER_OPERATION_UV_MESH)
            throw ProtocolError(ProtocolErrorCode::invalidInput, "Service receipt operation is unsupported.");
        RequestIdentity identity;
        std::copy(std::begin(active.document), std::end(active.document), identity.document.begin());
        identity.request = active.request;
        identity.revision = active.revision;
        ServiceReceipt control;
        control.runtimeStatus = receipt.runtime_status;
        control.admissionStatus = receipt.admission_status;
        control.operation = receipt.operation == DUST3D_WORKER_OPERATION_MESH ? Operation::generateMesh : Operation::generateUvMesh;
        control.request = receipt.request_identity;
        control.process = receipt.process_identifier;
        control.systemError = receipt.system_error;
        control.ioStage = receipt.io_stage;
        control.detailSource = receipt.detail_source;
        control.detailCode = receipt.detail_code;
        control.messageTruncated = receipt.message_truncated;
        control.finalizationError = receipt.finalization_error;
        MeshPayloadLimits mesh;
        mesh.maximumWireBytes = value.policy.maximumOutputBytes;
        mesh.maximumDecodedBytes = maximum;
        FailurePayloadLimits failure;
        failure.maximumWireBytes = std::min(failure.maximumWireBytes, value.policy.maximumFailureBytes);
        failure.maximumCauseBytes = std::min({ failure.maximumCauseBytes, maximum, failure.maximumWireBytes });
        UvMeshPayloadLimits uv;
        uv.maximumWireBytes = mesh.maximumWireBytes;
        uv.maximumDecodedBytes = maximum;
        uv.geometry = mesh;
        uv.uv.maximumWireBytes = uv.maximumWireBytes;
        uv.uv.maximumDecodedBytes = maximum;
        PendingServiceReply pending(value.expected, value.serviceConnectionGeneration, value.policy);
        return pending.complete(identity, { observed.connection_generation, observed.process_identifier, value.isFramed },
            control, value.bytes.data(), value.bytes.size(), value.policy, mesh, failure, uv);
    });
}

extern "C" Dust3DWorkerDecodedSummary dust3d_worker_decoded_summary(const Dust3DWorkerDecodedReply* reply) noexcept
{
    Dust3DWorkerDecodedSummary result {};
    if (const auto* mesh = geometry(reply)) {
        result.kind = DUST3D_WORKER_DECODED_MESH;
        result.vertex_count = mesh->vertices.size();
        result.node_count = mesh->nodeCount;
        result.part_count = mesh->partCount;
        if (const auto* atlas = uv(reply)) {
            result.kind = DUST3D_WORKER_DECODED_UV_MESH;
            result.triangle_count = atlas->triangles.size();
            result.chart_count = atlas->charts.size();
            result.gradient_count = atlas->gradients.size();
            result.component_count = atlas->components.size();
            result.packed_texture_size = atlas->packedTextureSize;
        }
    } else if (reply) {
        // Only fully constructed immutable variants are published to C callers.
        const auto& failure = std::get<FailurePayload>(reply->value);
        result.kind = DUST3D_WORKER_DECODED_FAILURE;
        result.failure_status = static_cast<uint32_t>(failure.status);
    }
    return result;
}
extern "C" int dust3d_worker_decoded_vertices(const Dust3DWorkerDecodedReply* reply, size_t first, size_t count, Dust3DWorkerVertex* output) noexcept
{
    const auto* mesh = geometry(reply);
    if (!mesh || first > mesh->vertices.size() || count > mesh->vertices.size() - first || (count && !output))
        return 0;
    for (size_t i = 0; i < count; ++i) {
        const auto& source = mesh->vertices[first + i];
        std::copy(source.position.begin(), source.position.end(), output[i].position);
        std::copy(source.normal.begin(), source.normal.end(), output[i].normal);
        std::copy(source.color.begin(), source.color.end(), output[i].color);
    }
    return 1;
}
extern "C" int dust3d_worker_decoded_triangle(const Dust3DWorkerDecodedReply* reply, size_t index, Dust3DWorkerUvTriangle* output) noexcept
{
    if (!output)
        return 0;
    *output = {};
    const auto* atlas = uv(reply);
    if (!atlas || index >= atlas->triangles.size())
        return 0;
    const auto& source = atlas->triangles[index];
    for (size_t i = 0; i < 3; ++i)
        std::copy(source.uv[i].begin(), source.uv[i].end(), output->uv[i]);
    output->known = source.known;
    output->association = static_cast<uint8_t>(source.association);
    output->component_index = source.componentIndex;
    return 1;
}
extern "C" int dust3d_worker_decoded_chart(const Dust3DWorkerDecodedReply* reply, size_t index, Dust3DWorkerUvChart* output) noexcept
{
    if (!output)
        return 0;
    *output = {};
    const auto* atlas = uv(reply);
    if (!atlas || index >= atlas->charts.size())
        return 0;
    const auto& source = atlas->charts[index];
    output->left = source.left;
    output->top = source.top;
    output->width = source.width;
    output->height = source.height;
    std::copy(source.color.begin(), source.color.end(), output->color);
    output->flipped = source.flipped;
    output->association = static_cast<uint8_t>(source.association);
    output->component_index = source.componentIndex;
    std::copy(source.imageID.begin(), source.imageID.end(), output->image_id);
    return 1;
}
extern "C" int dust3d_worker_decoded_gradient(const Dust3DWorkerDecodedReply* reply, size_t index, Dust3DWorkerUvGradient* output) noexcept
{
    if (!output)
        return 0;
    *output = {};
    const auto* atlas = uv(reply);
    if (!atlas || index >= atlas->gradients.size())
        return 0;
    const auto& source = atlas->gradients[index];
    output->chart_index = source.chartIndex;
    output->resolution = source.resolution;
    std::copy(source.largeColor.begin(), source.largeColor.end(), output->large_color);
    std::copy(source.smallColor.begin(), source.smallColor.end(), output->small_color);
    return 1;
}
extern "C" const char* dust3d_worker_decoded_warning(const Dust3DWorkerDecodedReply* reply, size_t* size) noexcept
{
    if (!size)
        return nullptr;
    *size = 0;
    const auto* mesh = geometry(reply);
    if (!mesh)
        return nullptr;
    *size = mesh->warning.size();
    return mesh->warning.data();
}
extern "C" const char* dust3d_worker_decoded_component(const Dust3DWorkerDecodedReply* reply, size_t index, size_t* size) noexcept
{
    if (!size)
        return nullptr;
    *size = 0;
    const auto* atlas = uv(reply);
    if (!atlas || index >= atlas->components.size())
        return nullptr;
    *size = atlas->components[index].size();
    return atlas->components[index].data();
}
extern "C" const uint8_t* dust3d_worker_decoded_failure_cause(const Dust3DWorkerDecodedReply* reply, size_t* size) noexcept
{
    if (!size)
        return nullptr;
    *size = 0;
    if (!reply)
        return nullptr;
    const auto* failure = std::get_if<FailurePayload>(&reply->value);
    if (!failure)
        return nullptr;
    *size = failure->cause.size();
    return failure->cause.data();
}
extern "C" void dust3d_worker_decoded_destroy(Dust3DWorkerDecodedReply* reply) noexcept { delete reply; }
