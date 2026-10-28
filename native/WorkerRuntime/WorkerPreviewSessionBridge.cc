// SPDX-License-Identifier: MIT
#include "WorkerJobIO.h"
#include "WorkerPreviewSessionIO.h"
#include "WorkerSessionOutputInternal.h"
#include <Dust3DWorkerPreviewSession.h>
#include <algorithm>
#include <memory>
using namespace dust3d::worker;
extern "C" Dust3DWorkerSessionOutput* dust3d_worker_preview_session_read(int32_t input, Dust3DWorkerPreviewSessionScope scope, Dust3DWorkerPreviewArchiveContext context, Dust3DWorkerPreviewChunkPolicy policy, Dust3DWorkerPreviewSessionResult* result) noexcept
{
    if (!result)
        return nullptr;
    *result = {};
    result->status = DUST3D_WORKER_PREVIEW_SESSION_INTERNAL_FAILED;
    try {
        static_assert(sizeof(int) == sizeof(int32_t), "Preview descriptors require 32-bit int.");
        PreviewChunkIdentity identity;
        identity.direction = PreviewChunkDirection::session;
        std::copy(std::begin(scope.identity.document), std::end(scope.identity.document), identity.request.document.begin());
        identity.request.request = scope.identity.request;
        identity.request.revision = scope.identity.revision;
        identity.connectionGeneration = scope.connection_generation;
        auto value = readPreviewSessionChannel(input, identity, { context.archive_bytes, context.maximum_source_bytes, context.maximum_indexed_bytes }, { policy.maximum_transfer_bytes, policy.chunk_payload_bytes });
        auto* owner = bridge::adoptSessionOutput(std::move(value));
        result->status = DUST3D_WORKER_PREVIEW_SESSION_OK;
        return owner;
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
    }
    return nullptr;
}
