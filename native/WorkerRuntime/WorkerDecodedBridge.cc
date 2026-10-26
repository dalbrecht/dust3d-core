// SPDX-License-Identifier: MIT
#include "WorkerAdmission.h"
#include "WorkerDecodedInternal.h"
#include "WorkerFailure.h"
#include "WorkerReplyInternal.h"
#include "WorkerServiceInternal.h"
#include "WorkerUvMesh.h"
#include <algorithm>
#include <iterator>
#include <memory>
#include <new>
#include <utility>
#include <variant>
using namespace dust3d::worker;
struct Dust3DWorkerDecodedReply {
    AdmittedReply value;
};
namespace dust3d::worker::bridge {
Dust3DWorkerDecodedReply* adoptDecodedReply(AdmittedReply value)
{
    return new Dust3DWorkerDecodedReply { std::move(value) };
}
}
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
    if (auto* pair = std::get_if<SourcedMeshPayload>(&reply->value))
        return &pair->geometry;
    return nullptr;
}
const IndexedMeshPayload* indexedGeometry(const Dust3DWorkerDecodedReply* reply) noexcept
{
    if (!reply)
        return nullptr;
    if (const auto* mesh = std::get_if<IndexedMeshPayload>(&reply->value))
        return mesh;
    if (const auto* pair = std::get_if<IndexedSourcesPayload>(&reply->value))
        return &pair->geometry;
    return nullptr;
}
const std::vector<MeshVertex>* vertexBuffer(const Dust3DWorkerDecodedReply* reply) noexcept
{
    if (const auto* mesh = geometry(reply))
        return &mesh->vertices;
    if (const auto* mesh = indexedGeometry(reply))
        return &mesh->vertices;
    return nullptr;
}
const UvPayload* uv(const Dust3DWorkerDecodedReply* reply) noexcept
{
    if (!reply)
        return nullptr;
    const auto* pair = std::get_if<UvMeshPayload>(&reply->value);
    return pair ? &pair->uv : nullptr;
}
const SourceHintPayload* sources(const Dust3DWorkerDecodedReply* reply) noexcept
{
    if (!reply)
        return nullptr;
    if (const auto* pair = std::get_if<SourcedMeshPayload>(&reply->value))
        return &pair->sources;
    if (const auto* pair = std::get_if<IndexedSourcesPayload>(&reply->value))
        return &pair->sources;
    return nullptr;
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
    } catch (const SourceHintPayloadError& cause) {
        rejectCodec(*result, DUST3D_WORKER_DETAIL_SOURCE_HINTS, cause);
    } catch (const SourcedMeshPayloadError& cause) {
        rejectCodec(*result, DUST3D_WORKER_DETAIL_SOURCED_MESH, cause);
    } catch (const IndexedMeshPayloadError& cause) {
        rejectCodec(*result, DUST3D_WORKER_DETAIL_INDEXED_MESH, cause);
    } catch (const IndexedSourcesPayloadError& cause) {
        rejectCodec(*result, DUST3D_WORKER_DETAIL_INDEXED_SOURCES, cause);
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
        const auto policy = bridge::semanticPolicies(owner->policy, maximum);
        if (header.kind == FrameKind::failure)
            return decodeFailurePayload(body, size, policy.failure);
        switch (header.operation) {
        case Operation::generateMesh:
            return decodeMeshPayload(body, size, policy.mesh);
        case Operation::generateUvMesh:
            return decodeUvMeshPayload(body, size, policy.uv);
        case Operation::generateSourcedMesh:
            return decodeSourcedMeshPayload(body, size, policy.sources);
        case Operation::generateIndexedSourcedMesh:
            return decodeIndexedSourcesPayload(body, size, policy.indexed.sourcedMesh);
        case Operation::generateIndexedMesh:
            return decodeIndexedMeshPayload(body, size, policy.indexed.mesh);
        }
        throw ProtocolError(ProtocolErrorCode::unsupportedOperation, "Decoded operation is unsupported.");
    });
}
extern "C" Dust3DWorkerDecodedReply* dust3d_worker_service_admit(Dust3DWorkerReplyCollector* owner,
    Dust3DWorkerRequestIdentity active, Dust3DWorkerServiceObservation observed,
    Dust3DWorkerServiceReceipt receipt, uint64_t maximum, Dust3DWorkerDecodeResult* result) noexcept
{
    return protectDecoded(owner, maximum, result, [=](Dust3DWorkerReplyCollector& value, uint64_t maximum) -> AdmittedReply {
        if (!value.serviceConnectionGeneration)
            throw ProtocolError(ProtocolErrorCode::invalidInput, "Service admission requires a service-specific collector.");
        if (value.isQueueBound)
            throw ProtocolError(ProtocolErrorCode::invalidInput, "Queue-bound collector requires queue admission.");
        const auto identity = bridge::copyRequestIdentity(active);
        const auto control = bridge::copyServiceReceipt(receipt);
        const auto policy = bridge::semanticPolicies(value.policy, maximum);
        PendingServiceReply pending(value.expected, value.serviceConnectionGeneration, value.policy);
        return pending.complete(identity, { observed.connection_generation, observed.process_identifier, value.isFramed },
            control, value.bytes.data(), value.bytes.size(), value.policy, policy.mesh, policy.failure, policy.uv, policy.sources, policy.indexed);
    });
}

extern "C" Dust3DWorkerDecodedSummary dust3d_worker_decoded_summary(const Dust3DWorkerDecodedReply* reply) noexcept
{
    Dust3DWorkerDecodedSummary result {};
    if (const auto* mesh = indexedGeometry(reply)) {
        result.kind = sources(reply) ? DUST3D_WORKER_DECODED_INDEXED_SOURCED_MESH : DUST3D_WORKER_DECODED_INDEXED_MESH;
        result.vertex_count = mesh->vertices.size();
        result.triangle_count = mesh->indices.size() / 3;
        result.node_count = mesh->nodeCount;
        result.part_count = mesh->partCount;
    } else if (const auto* mesh = geometry(reply)) {
        result.kind = DUST3D_WORKER_DECODED_MESH;
        result.vertex_count = mesh->vertices.size();
        result.node_count = mesh->nodeCount;
        result.part_count = mesh->partCount;
        if (sources(reply))
            result.kind = DUST3D_WORKER_DECODED_SOURCED_MESH;
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
extern "C" Dust3DWorkerSourceSummary dust3d_worker_decoded_source_summary(const Dust3DWorkerDecodedReply* reply) noexcept
{
    const auto* value = sources(reply);
    if (!value)
        return {};
    return { value->identities.size(), value->corners.size(), value->influences.size() };
}
extern "C" int dust3d_worker_decoded_source_corner(const Dust3DWorkerDecodedReply* reply, size_t index, Dust3DWorkerSourceCorner* output) noexcept
{
    if (!output)
        return 0;
    *output = {};
    const auto* value = sources(reply);
    if (!value || index >= value->corners.size())
        return 0;
    const auto& corner = value->corners[index];
    *output = { static_cast<uint32_t>(corner.kind), corner.firstInfluence, corner.influenceCount };
    return 1;
}
extern "C" int dust3d_worker_decoded_source_influence(const Dust3DWorkerDecodedReply* reply, size_t index, Dust3DWorkerSourceInfluence* output) noexcept
{
    if (!output)
        return 0;
    *output = {};
    const auto* value = sources(reply);
    if (!value || index >= value->influences.size())
        return 0;
    const auto& influence = value->influences[index];
    *output = { static_cast<uint32_t>(influence.kind), influence.identityIndex, influence.weight };
    return 1;
}
namespace {
const char* borrowSourceIdentity(const Dust3DWorkerDecodedReply* reply, size_t index, size_t* size, bool isPart) noexcept
{
    if (!size)
        return nullptr;
    *size = 0;
    const auto* value = sources(reply);
    if (!value || index >= value->identities.size())
        return nullptr;
    const auto& text = isPart ? value->identities[index].part : value->identities[index].node;
    *size = text.size();
    return text.c_str();
}
}
extern "C" const char* dust3d_worker_decoded_source_node(const Dust3DWorkerDecodedReply* reply, size_t index, size_t* size) noexcept
{
    return borrowSourceIdentity(reply, index, size, false);
}
extern "C" const char* dust3d_worker_decoded_source_part(const Dust3DWorkerDecodedReply* reply, size_t index, size_t* size) noexcept
{
    return borrowSourceIdentity(reply, index, size, true);
}
extern "C" size_t dust3d_worker_decoded_index_count(const Dust3DWorkerDecodedReply* reply) noexcept
{
    const auto* mesh = indexedGeometry(reply);
    return mesh ? mesh->indices.size() : 0;
}
extern "C" int dust3d_worker_decoded_indices(const Dust3DWorkerDecodedReply* reply, size_t first, size_t count, uint32_t* output) noexcept
{
    const auto* mesh = indexedGeometry(reply);
    if (!mesh || first > mesh->indices.size() || count > mesh->indices.size() - first || (count && !output))
        return 0;
    if (count)
        std::copy_n(mesh->indices.data() + first, count, output);
    return 1;
}
extern "C" int dust3d_worker_decoded_vertices(const Dust3DWorkerDecodedReply* reply, size_t first, size_t count, Dust3DWorkerVertex* output) noexcept
{
    const auto* vertices = vertexBuffer(reply);
    if (!vertices || first > vertices->size() || count > vertices->size() - first || (count && !output))
        return 0;
    for (size_t i = 0; i < count; ++i) {
        const auto& source = (*vertices)[first + i];
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
    const auto* indexed = indexedGeometry(reply);
    const auto* warning = mesh ? &mesh->warning : indexed ? &indexed->warning
                                                          : nullptr;
    if (!warning)
        return nullptr;
    *size = warning->size();
    return warning->data();
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
