// SPDX-License-Identifier: MIT
#include "WorkerFailure.h"
#include "WorkerImageDecoder.h"
#include "WorkerImportedRequest.h"
#include "WorkerIndexedSources.h"
#include "WorkerJobIO.h"
#include "WorkerLegacyRequest.h"
#include "WorkerSessionInternal.h"
#include "WorkerSessionOutputInternal.h"
#include "WorkerSourceRequest.h"
#include "WorkerSourcedMesh.h"
#include "WorkerUvMesh.h"
#include "WorkerUvRequest.h"
#include <Dust3DWorkerRuntime.h>
#include <cstdio>
#include <cstring>
#include <new>

namespace {
using namespace dust3d::worker;
static_assert(sizeof(int) == sizeof(int32_t), "Worker descriptors require 32-bit int.");
static_assert(sizeof(Dust3DWorkerJobStatus) == sizeof(int32_t), "Worker status requires default C enum width.");
void copyCause(Dust3DWorkerJobResult& result, const char* cause) noexcept
{
    if (!cause)
        return;
    size_t length = 0;
    while (length < sizeof(result.message) - 1 && cause[length]) {
        result.message[length] = cause[length];
        ++length;
    }
    result.message[length] = 0;
    result.message_truncated = cause[length] != 0;
}
template <typename Error>
void rejectCodec(Dust3DWorkerJobResult& result, Dust3DWorkerDetailSource source, const Error& cause) noexcept
{
    result.status = DUST3D_WORKER_JOB_CODEC_REJECTED;
    result.detail_source = source;
    result.detail_code = static_cast<int32_t>(cause.code());
    copyCause(result, cause.what());
}
}
extern "C" uint32_t dust3d_worker_runtime_abi_version() noexcept
{
    return DUST3D_WORKER_RUNTIME_ABI_VERSION;
}
namespace {
struct SessionInvocation {
    Dust3DWorkerSession* owner;
    uint64_t generation;
    Dust3DWorkerSessionOutputLimits limits;
};
Dust3DWorkerJobResult serve(int32_t input, int32_t output, const Dust3DWorkerImageDecoder* configuration, const Dust3DWorkerImageDecoder* proceduralConfiguration = nullptr, const SessionInvocation* session = nullptr) noexcept
{
    Dust3DWorkerJobResult result {};
    result.status = DUST3D_WORKER_JOB_INTERNAL_FAILED;
    WorkerJobContext context;
    try {
        std::optional<CallbackImportedImageDecoder> decoder;
        if (configuration)
            decoder.emplace(*configuration);
        std::optional<CallbackProceduralImageDecoder> procedural;
        if (proceduralConfiguration)
            procedural.emplace(*proceduralConfiguration);
        if (session) {
            if (!session->owner)
                throw ProtocolError(ProtocolErrorCode::invalidInput, "C worker session owner is null.");
            const auto limits = bridge::sessionOutputPolicy(session->limits);
            serveWorkerSessionJob(input, output, context, session->owner->value, session->generation, limits);
        } else
            serveWorkerJob(input, output, context, decoder ? &*decoder : nullptr, {}, procedural ? &*procedural : nullptr);
        result.status = DUST3D_WORKER_JOB_COMPLETE;
    } catch (const WorkerJobIOError& cause) {
        result.status = DUST3D_WORKER_JOB_IO_FAILED;
        result.detail_source = DUST3D_WORKER_DETAIL_SYSTEM;
        result.system_error = cause.systemError();
        result.io_stage = cause.stage() == WorkerJobStage::input ? DUST3D_WORKER_IO_INPUT : DUST3D_WORKER_IO_OUTPUT;
        const auto* original = std::strerror(cause.systemError());
        const auto count = std::snprintf(result.message, sizeof(result.message), "%s errno=%d cause=%s",
            cause.what(), cause.systemError(), original ? original : "Unavailable system description");
        if (count < 0)
            copyCause(result, cause.what());
        else
            result.message_truncated = count >= static_cast<int>(sizeof(result.message));
    } catch (const ProtocolError& cause) {
        result.status = DUST3D_WORKER_JOB_PROTOCOL_REJECTED;
        result.detail_source = DUST3D_WORKER_DETAIL_FRAME;
        result.detail_code = static_cast<int32_t>(cause.code());
        copyCause(result, cause.what());
    } catch (const SessionScopeError& cause) {
        result.status = DUST3D_WORKER_JOB_PROTOCOL_REJECTED;
        result.detail_source = DUST3D_WORKER_DETAIL_SESSION_SCOPE;
        result.detail_code = static_cast<int32_t>(cause.code());
        copyCause(result, cause.what());
    } catch (const NativeSessionError& cause) {
        result.detail_source = DUST3D_WORKER_DETAIL_NATIVE_SESSION;
        result.detail_code = cause.failure().detail ? cause.failure().detail->code : static_cast<int32_t>(cause.failure().status);
        copyCause(result, cause.what());
        if (cause.failure().detail && cause.failure().detail->causeTruncated)
            result.message_truncated = 1;
    } catch (const SessionOutputError& cause) {
        rejectCodec(result, DUST3D_WORKER_DETAIL_SESSION_OUTPUT, cause);
    } catch (const GenerationStatisticsError& cause) {
        rejectCodec(result, DUST3D_WORKER_DETAIL_GENERATION_STATISTICS, cause);
    } catch (const SourceRequestError& cause) {
        rejectCodec(result, DUST3D_WORKER_DETAIL_SOURCE_REQUEST, cause);
    } catch (const UvRequestError& cause) {
        rejectCodec(result, DUST3D_WORKER_DETAIL_UV_REQUEST, cause);
    } catch (const MeshPayloadError& cause) {
        rejectCodec(result, DUST3D_WORKER_DETAIL_MESH, cause);
    } catch (const UvPayloadError& cause) {
        rejectCodec(result, DUST3D_WORKER_DETAIL_UV, cause);
    } catch (const UvMeshPayloadError& cause) {
        rejectCodec(result, DUST3D_WORKER_DETAIL_UV_MESH, cause);
    } catch (const SourceHintPayloadError& cause) {
        rejectCodec(result, DUST3D_WORKER_DETAIL_SOURCE_HINTS, cause);
    } catch (const SourcedMeshPayloadError& cause) {
        rejectCodec(result, DUST3D_WORKER_DETAIL_SOURCED_MESH, cause);
    } catch (const IndexedMeshPayloadError& cause) {
        rejectCodec(result, DUST3D_WORKER_DETAIL_INDEXED_MESH, cause);
    } catch (const IndexedSourcesPayloadError& cause) {
        rejectCodec(result, DUST3D_WORKER_DETAIL_INDEXED_SOURCES, cause);
    } catch (const LegacyRequestError& cause) {
        rejectCodec(result, DUST3D_WORKER_DETAIL_LEGACY_REQUEST, cause);
    } catch (const LegacyProducerError& cause) {
        rejectCodec(result, DUST3D_WORKER_DETAIL_LEGACY_PRODUCER, cause);
    } catch (const LegacyBakedError& cause) {
        rejectCodec(result, DUST3D_WORKER_DETAIL_LEGACY_BAKED, cause);
    } catch (const ProceduralImageError& cause) {
        rejectCodec(result, DUST3D_WORKER_DETAIL_PROCEDURAL_IMAGES, cause);
    } catch (const BakedTexturesError& cause) {
        rejectCodec(result, DUST3D_WORKER_DETAIL_BAKED_TEXTURES, cause);
    } catch (const ImportedRequestError& cause) {
        rejectCodec(result, DUST3D_WORKER_DETAIL_IMPORTED_REQUEST, cause);
    } catch (const WorkerImageDecoderError& cause) {
        result.status = DUST3D_WORKER_JOB_CODEC_REJECTED;
        result.detail_source = DUST3D_WORKER_DETAIL_IMAGE_DECODER;
        result.detail_code = cause.result().code;
        copyCause(result, cause.what());
    } catch (const ImportedProducerError& cause) {
        rejectCodec(result, DUST3D_WORKER_DETAIL_IMPORTED_PRODUCER, cause);
    } catch (const ImportedSurfaceError& cause) {
        rejectCodec(result, DUST3D_WORKER_DETAIL_IMPORTED_SURFACE, cause);
    } catch (const ImportedMaterialsError& cause) {
        rejectCodec(result, DUST3D_WORKER_DETAIL_IMPORTED_MATERIALS, cause);
    } catch (const ImportedRenderError& cause) {
        rejectCodec(result, DUST3D_WORKER_DETAIL_IMPORTED_RENDER, cause);
    } catch (const FailurePayloadError& cause) {
        rejectCodec(result, DUST3D_WORKER_DETAIL_FAILURE, cause);
    } catch (const std::bad_alloc& cause) {
        result.status = DUST3D_WORKER_JOB_LIMIT_EXCEEDED;
        result.detail_source = DUST3D_WORKER_DETAIL_RUNTIME;
        copyCause(result, cause.what());
    } catch (const std::exception& cause) {
        result.detail_source = DUST3D_WORKER_DETAIL_RUNTIME;
        copyCause(result, cause.what());
    } catch (...) {
        result.detail_source = DUST3D_WORKER_DETAIL_RUNTIME;
        copyCause(result, "Worker runtime exception has no available diagnostic.");
    }
    result.request_identity = context.requestIdentity;
    if (context.operation == Operation::generateSessionIndexedSourcedMesh)
        result.operation = DUST3D_WORKER_OPERATION_SESSION_INDEXED_SOURCED_MESH;
    else if (context.operation == Operation::generateMesh)
        result.operation = DUST3D_WORKER_OPERATION_MESH;
    else if (context.operation == Operation::generateUvMesh)
        result.operation = DUST3D_WORKER_OPERATION_UV_MESH;
    else if (context.operation == Operation::generateSourcedMesh)
        result.operation = DUST3D_WORKER_OPERATION_SOURCED_MESH;
    else if (context.operation == Operation::generateIndexedSourcedMesh)
        result.operation = DUST3D_WORKER_OPERATION_INDEXED_SOURCED_MESH;
    else if (context.operation == Operation::generateIndexedMesh)
        result.operation = DUST3D_WORKER_OPERATION_INDEXED_MESH;
    if (context.operation == Operation::generateImportedRender)
        result.operation = DUST3D_WORKER_OPERATION_IMPORTED_RENDER;
    if (context.operation == Operation::generateSourceBaked)
        result.operation = DUST3D_WORKER_OPERATION_SOURCE_BAKED;
    if (context.operation == Operation::generateLegacyBaked)
        result.operation = DUST3D_WORKER_OPERATION_LEGACY_BAKED;
    return result;
}

}
extern "C" Dust3DWorkerJobResult dust3d_worker_serve(int32_t input, int32_t output) noexcept
{
    return serve(input, output, nullptr);
}
extern "C" Dust3DWorkerJobResult dust3d_worker_serve_with_image_decoder(int32_t input, int32_t output, const Dust3DWorkerImageDecoder* configuration) noexcept
{
    if (!configuration) {
        Dust3DWorkerJobResult result {};
        result.status = DUST3D_WORKER_JOB_CODEC_REJECTED;
        result.detail_source = DUST3D_WORKER_DETAIL_IMAGE_DECODER;
        copyCause(result, "Worker image decoder configuration is missing.");
        return result;
    }
    return serve(input, output, configuration);
}

extern "C" Dust3DWorkerJobResult dust3d_worker_serve_with_decoders(int32_t input, int32_t output, const Dust3DWorkerDecoders* configuration) noexcept
{
    if (!configuration || configuration->struct_size != sizeof(*configuration)) {
        Dust3DWorkerJobResult result {};
        result.status = DUST3D_WORKER_JOB_CODEC_REJECTED;
        result.detail_source = DUST3D_WORKER_DETAIL_IMAGE_DECODER;
        copyCause(result, "Worker purpose-specific decoder configuration is missing or invalid.");
        return result;
    }
    const auto present = [](const Dust3DWorkerImageDecoder& slot) { return slot.struct_size || slot.context || slot.decode; };
    return serve(input, output, present(configuration->imported) ? &configuration->imported : nullptr,
        present(configuration->procedural) ? &configuration->procedural : nullptr);
}

extern "C" Dust3DWorkerJobResult dust3d_worker_session_serve(Dust3DWorkerSession* owner, uint64_t generation,
    int32_t input, int32_t output, Dust3DWorkerSessionOutputLimits limits) noexcept
{
    const SessionInvocation invocation { owner, generation, limits };
    return serve(input, output, nullptr, nullptr, &invocation);
}
