/* SPDX-License-Identifier: MIT */
#include <Dust3DWorkerRuntime.h>
#include <string.h>
int main(void)
{
    uint8_t header[64] = { 'D', 'S', '3', 'W', 'O', 'R', 'K', 0 };
    header[9] = 1;
    header[11] = 64;
    header[17] = 1;
    header[19] = 1;
    header[31] = 42;
    header[40] = 1;
    Dust3DWorkerReplyResult result;
    const Dust3DWorkerWireLimits limits = { 8, 8, 8 };
    Dust3DWorkerReplyCollector* owner = dust3d_worker_reply_create(header, sizeof(header), limits, &result);
    if (!owner || result.status != DUST3D_WORKER_REPLY_READY)
        return 1;
    size_t count = 123;
    if (dust3d_worker_reply_bytes(owner, &count) || count)
        return 2;
    result = dust3d_worker_reply_next_read(owner, 65536, &count);
    if (result.status != DUST3D_WORKER_REPLY_READY || count != 64)
        return 3;
    header[17] = 2;
    result = dust3d_worker_reply_append(owner, header, 64);
    if (result.status != DUST3D_WORKER_REPLY_READY)
        return 4;
    if (dust3d_worker_reply_bytes(owner, &count) || count)
        return 5;
    result = dust3d_worker_reply_finish(owner);
    const uint8_t* bytes = dust3d_worker_reply_bytes(owner, &count);
    if (result.status != DUST3D_WORKER_REPLY_FRAMED || !bytes || count != 64 || memcmp(header, bytes, 64))
        return 6;
    dust3d_worker_reply_destroy(owner);
    dust3d_worker_reply_destroy(NULL);
    result = dust3d_worker_reply_finish(NULL);
    if (result.status != DUST3D_WORKER_REPLY_PROTOCOL_REJECTED || !result.message[0]
        || result.detail_source != DUST3D_WORKER_DETAIL_FRAME || !memchr(result.message, 0, sizeof(result.message)))
        return 7;
    return 0;
}
