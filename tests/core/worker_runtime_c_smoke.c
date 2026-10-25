/* SPDX-License-Identifier: MIT */
#include <Dust3DWorkerRuntime.h>
#include <string.h>
int main(void)
{
    if (dust3d_worker_runtime_abi_version() != DUST3D_WORKER_RUNTIME_ABI_VERSION)
        return 1;
    const Dust3DWorkerJobResult result = dust3d_worker_serve(-1, -1);
    if (result.status != DUST3D_WORKER_JOB_PROTOCOL_REJECTED || result.operation != DUST3D_WORKER_OPERATION_UNKNOWN
        || result.request_identity != 0 || result.system_error != 0 || result.detail_source != DUST3D_WORKER_DETAIL_FRAME
        || !result.message[0] || !memchr(result.message, 0, sizeof(result.message)) || result.message_truncated)
        return 2;
    return 0;
}
