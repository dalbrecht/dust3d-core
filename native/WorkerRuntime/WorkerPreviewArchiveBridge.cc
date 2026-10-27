// SPDX-License-Identifier: MIT
#include "WorkerJobIO.h"
#include "WorkerPreviewArchiveIO.h"
#include <Dust3DWorkerPreviewArchive.h>
#include <algorithm>
#include <new>
#include <utility>
using namespace dust3d::worker;
struct Dust3DWorkerPreviewArchive {
    explicit Dust3DWorkerPreviewArchive(std::unique_ptr<PreviewArchiveCollector> value)
        : owner(std::move(value))
        , view(owner->view())
    {
    }
    const std::unique_ptr<PreviewArchiveCollector> owner;
    const PreviewArchiveView view;
};
namespace {
static_assert(sizeof(int) == sizeof(int32_t), "Preview descriptors require 32-bit int.");
PreviewChunkIdentity identity(Dust3DWorkerPreviewArchiveScope scope)
{
    PreviewChunkIdentity result;
    std::copy(std::begin(scope.identity.document), std::end(scope.identity.document), result.request.document.begin());
    result.request.request = scope.identity.request;
    result.request.revision = scope.identity.revision;
    result.connectionGeneration = scope.connection_generation;
    return result;
}
PreviewArchiveContext controls(Dust3DWorkerPreviewArchiveContext value) { return { value.archive_bytes, value.maximum_source_bytes, value.maximum_indexed_bytes }; }
PreviewChunkPolicy policy(Dust3DWorkerPreviewChunkPolicy value) { return { value.maximum_transfer_bytes, value.chunk_payload_bytes }; }
void copyCause(Dust3DWorkerPreviewArchiveResult& result, const char* cause) noexcept
{
    size_t count = 0;
    while (count < sizeof(result.message) - 1 && cause[count]) {
        result.message[count] = cause[count];
        ++count;
    }
    result.message[count] = 0;
    result.message_truncated = cause[count] != 0;
}
template <class Action>
Dust3DWorkerPreviewArchiveResult invoke(Action action) noexcept
{
    Dust3DWorkerPreviewArchiveResult result {};
    result.status = DUST3D_WORKER_PREVIEW_ARCHIVE_INTERNAL_FAILED;
    try {
        action();
        result.status = DUST3D_WORKER_PREVIEW_ARCHIVE_OK;
    } catch (const PreviewChunkError& cause) {
        result.status = DUST3D_WORKER_PREVIEW_ARCHIVE_CHUNK_REJECTED;
        result.detail_code = static_cast<int32_t>(cause.code());
        copyCause(result, cause.what());
    } catch (const SourceRequestError& cause) {
        result.status = DUST3D_WORKER_PREVIEW_ARCHIVE_SOURCE_REJECTED;
        result.detail_code = static_cast<int32_t>(cause.code());
        copyCause(result, cause.what());
    } catch (const WorkerJobIOError& cause) {
        result.status = DUST3D_WORKER_PREVIEW_ARCHIVE_IO_FAILED;
        result.system_error = cause.systemError();
        result.io_stage = cause.stage() == WorkerJobStage::input ? DUST3D_WORKER_IO_INPUT : DUST3D_WORKER_IO_OUTPUT;
        copyCause(result, cause.what());
    } catch (const std::bad_alloc& cause) {
        result.status = DUST3D_WORKER_PREVIEW_ARCHIVE_ALLOCATION_FAILED;
        copyCause(result, cause.what());
    } catch (const std::exception& cause) {
        copyCause(result, cause.what());
    } catch (...) {
        copyCause(result, "Unknown preview archive failure.");
    }
    return result;
}
}
extern "C" Dust3DWorkerPreviewArchiveResult dust3d_worker_preview_archive_write(int32_t output, Dust3DWorkerPreviewArchiveScope scope, Dust3DWorkerPreviewArchiveContext context, const uint8_t* archive, size_t size, Dust3DWorkerPreviewChunkPolicy chunking) noexcept
{
    return invoke([&] { writePreviewArchive(output, identity(scope), controls(context), archive, size, policy(chunking)); });
}
extern "C" Dust3DWorkerPreviewArchive* dust3d_worker_preview_archive_read(int32_t input, Dust3DWorkerPreviewArchiveScope scope, Dust3DWorkerPreviewArchiveContext context, Dust3DWorkerPreviewChunkPolicy chunking, Dust3DWorkerPreviewArchiveResult* result) noexcept
{
    if (!result)
        return nullptr;
    std::unique_ptr<Dust3DWorkerPreviewArchive> owner;
    *result = invoke([&] { owner = std::make_unique<Dust3DWorkerPreviewArchive>(readPreviewArchive(input, identity(scope), controls(context), policy(chunking))); });
    return owner.release();
}
extern "C" const uint8_t* dust3d_worker_preview_archive_bytes(const Dust3DWorkerPreviewArchive* owner, size_t* size) noexcept
{
    if (!size)
        return nullptr;
    *size = owner ? owner->view.source.archiveBytes : 0;
    return owner ? owner->view.source.archive : nullptr;
}
extern "C" Dust3DWorkerPreviewArchiveContext dust3d_worker_preview_archive_context(const Dust3DWorkerPreviewArchive* owner) noexcept
{
    if (!owner)
        return {};
    return { owner->view.source.archiveBytes, owner->view.source.maximumSourceBytes, owner->view.maximumIndexedBytes };
}
extern "C" void dust3d_worker_preview_archive_destroy(Dust3DWorkerPreviewArchive* owner) noexcept { delete owner; }
