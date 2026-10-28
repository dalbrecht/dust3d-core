/* SPDX-License-Identifier: MIT */
#include <Dust3DWorkerPreviewSession.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
int main(void)
{
    Dust3DWorkerPreviewSessionScope scope = { 0 };
    scope.identity.document[0] = 1;
    scope.identity.request = 1;
    scope.connection_generation = 1;
    Dust3DWorkerPreviewArchiveContext context = { 1, 0, 0 };
    Dust3DWorkerPreviewChunkPolicy policy = { 4096, 64 };
    Dust3DWorkerPreviewSessionResult result = { 0 };
    assert(!dust3d_worker_preview_session_read(-1, scope, context, policy, NULL));
    assert(!dust3d_worker_preview_session_read(-1, scope, context, policy, &result));
    assert(result.status == DUST3D_WORKER_PREVIEW_SESSION_IO_FAILED && result.system_error == EINVAL && result.io_stage == DUST3D_WORKER_IO_INPUT);
    FILE* input = tmpfile();
    assert(input);
    assert(!dust3d_worker_preview_session_read(fileno(input), scope, context, policy, &result));
    assert(result.status == DUST3D_WORKER_PREVIEW_SESSION_CHUNK_REJECTED && result.message[0]);
    assert(fclose(input) == 0);
    result = dust3d_worker_preview_session_serve(NULL, -1, -1, scope, context, policy, policy);
    assert(result.status == DUST3D_WORKER_PREVIEW_SESSION_PROTOCOL_REJECTED && result.detail_source == DUST3D_WORKER_DETAIL_FRAME);
    dust3d_worker_session_output_destroy(NULL);
    return 0;
}
