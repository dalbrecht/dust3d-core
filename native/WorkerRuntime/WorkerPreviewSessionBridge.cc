// SPDX-License-Identifier: MIT
#include "WorkerJobIO.h"
#include "WorkerPreviewSessionIO.h"
#include "WorkerSessionInternal.h"
#include "WorkerSessionOutputInternal.h"
#include <Dust3DWorkerPreviewSession.h>
#include <algorithm>
#include <memory>
using namespace dust3d::worker;
namespace {
PreviewChunkIdentity identity(Dust3DWorkerPreviewSessionScope scope, PreviewChunkDirection direction)
{
    PreviewChunkIdentity value;
    value.direction = direction;
    std::copy(std::begin(scope.identity.document), std::end(scope.identity.document), value.request.document.begin());
    value.request.request = scope.identity.request;
    value.request.revision = scope.identity.revision;
    value.connectionGeneration = scope.connection_generation;
    return value;
}
PreviewArchiveContext controls(Dust3DWorkerPreviewArchiveContext value) { return { value.archive_bytes, value.maximum_source_bytes, value.maximum_indexed_bytes }; }
PreviewChunkPolicy policy(Dust3DWorkerPreviewChunkPolicy value) { return { value.maximum_transfer_bytes, value.chunk_payload_bytes }; }
void reject(const std::exception_ptr& exception, Dust3DWorkerPreviewSessionResult& receipt) noexcept
{
    auto* result = &receipt;
    *result = {};
    result->status = DUST3D_WORKER_PREVIEW_SESSION_INTERNAL_FAILED;
    try {
        std::rethrow_exception(exception);
    } catch (const WorkerJobIOError& cause) {
        result->status = DUST3D_WORKER_PREVIEW_SESSION_IO_FAILED;
        result->detail_source = DUST3D_WORKER_DETAIL_SYSTEM;
        result->system_error = cause.systemError();
        result->io_stage = cause.stage() == WorkerJobStage::input ? DUST3D_WORKER_IO_INPUT : DUST3D_WORKER_IO_OUTPUT;
        size_t count = 0;
        while (count < sizeof(result->message) - 1 && cause.what()[count]) {
            result->message[count] = cause.what()[count];
            ++count;
        }
        result->message[count] = 0;
        result->message_truncated = cause.what()[count] != 0;
    } catch (...) {
        Dust3DWorkerDecodeResult cause {};
        bridge::rejectSessionOutputException(std::current_exception(), cause);
        result->detail_source = cause.detail_source;
        result->detail_code = cause.detail_code;
        result->message_truncated = cause.message_truncated;
        std::copy(std::begin(cause.message), std::end(cause.message), std::begin(result->message));
        if (cause.detail_source == DUST3D_WORKER_DETAIL_PREVIEW_CHUNK)
            result->status = DUST3D_WORKER_PREVIEW_SESSION_CHUNK_REJECTED;
        else if (cause.detail_source == DUST3D_WORKER_DETAIL_PREVIEW_RANGE)
            result->status = DUST3D_WORKER_PREVIEW_SESSION_RANGE_REJECTED;
        else if (cause.status == DUST3D_WORKER_DECODE_CODEC_REJECTED)
            result->status = DUST3D_WORKER_PREVIEW_SESSION_CODEC_REJECTED;
        else if (cause.status == DUST3D_WORKER_DECODE_LIMIT_EXCEEDED)
            result->status = DUST3D_WORKER_PREVIEW_SESSION_ALLOCATION_FAILED;
        else if (cause.status == DUST3D_WORKER_DECODE_PROTOCOL_REJECTED)
            result->status = DUST3D_WORKER_PREVIEW_SESSION_PROTOCOL_REJECTED;
        else if (cause.status == DUST3D_WORKER_DECODE_ADMISSION_REJECTED)
            result->status = DUST3D_WORKER_PREVIEW_SESSION_ADMISSION_REJECTED;
    }
}
}
extern "C" Dust3DWorkerSessionOutput* dust3d_worker_preview_session_read(int32_t input, Dust3DWorkerPreviewSessionScope scope, Dust3DWorkerPreviewArchiveContext context, Dust3DWorkerPreviewChunkPolicy chunking, Dust3DWorkerPreviewSessionResult* result) noexcept
{
    if (!result)
        return nullptr;
    *result = {};
    result->status = DUST3D_WORKER_PREVIEW_SESSION_INTERNAL_FAILED;
    try {
        static_assert(sizeof(int) == sizeof(int32_t), "Preview descriptors require 32-bit int.");
        auto value = readPreviewSessionChannel(input, identity(scope, PreviewChunkDirection::session), controls(context), policy(chunking));
        auto* owner = bridge::adoptSessionOutput(std::move(value));
        result->status = DUST3D_WORKER_PREVIEW_SESSION_OK;
        return owner;
    } catch (...) {
        reject(std::current_exception(), *result);
    }
    return nullptr;
}
extern "C" Dust3DWorkerPreviewSessionResult dust3d_worker_preview_session_serve(Dust3DWorkerSession* owner, int32_t input, int32_t output, Dust3DWorkerPreviewSessionScope scope, Dust3DWorkerPreviewArchiveContext context, Dust3DWorkerPreviewChunkPolicy inputPolicy, Dust3DWorkerPreviewChunkPolicy outputPolicy) noexcept
{
    Dust3DWorkerPreviewSessionResult result {};
    result.status = DUST3D_WORKER_PREVIEW_SESSION_INTERNAL_FAILED;
    try {
        if (!owner)
            throw ProtocolError(ProtocolErrorCode::invalidInput, "Preview serving requires a live worker-local session.");
        servePreviewSessionJob(input, output, owner->value, identity(scope, PreviewChunkDirection::archive), controls(context), policy(inputPolicy), policy(outputPolicy));
        result.status = DUST3D_WORKER_PREVIEW_SESSION_OK;
    } catch (...) {
        reject(std::current_exception(), result);
    }
    return result;
}
