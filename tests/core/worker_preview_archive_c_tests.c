/* SPDX-License-Identifier: MIT */
#include <Dust3DWorkerPreviewArchive.h>
#include <stddef.h>
int main(void)
{
    size_t size = 9;
    if (dust3d_worker_preview_archive_bytes(NULL, &size) || size || dust3d_worker_preview_archive_bytes(NULL, NULL))
        return 1;
    if (dust3d_worker_preview_archive_context(NULL).archive_bytes)
        return 2;
    dust3d_worker_preview_archive_destroy(NULL);
    Dust3DWorkerPreviewArchiveScope scope = { 0 };
    Dust3DWorkerPreviewArchiveContext context = { 1, 0, 0 };
    Dust3DWorkerPreviewChunkPolicy policy = { 25, 4 };
    if (dust3d_worker_preview_archive_read(-1, scope, context, policy, NULL))
        return 3;
    Dust3DWorkerPreviewArchiveResult result = { 0 };
    if (dust3d_worker_preview_archive_read(-1, scope, context, policy, &result) || result.status != DUST3D_WORKER_PREVIEW_ARCHIVE_IO_FAILED || !result.system_error)
        return 4;
    return 0;
}
