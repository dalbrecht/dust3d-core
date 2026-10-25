// SPDX-License-Identifier: MIT
#include <Dust3DWorkerRuntime.h>
#include <stdint.h>
#include <string.h>

int main(void)
{
    Dust3DWorkerQueueResult result;
    Dust3DWorkerWireLimits limits = { 64, 64, 64 };
    Dust3DWorkerServiceQueue* queue = dust3d_worker_service_queue_create(limits, &result);
    if (!queue || result.status != DUST3D_WORKER_QUEUE_READY)
        return 1;
    uint8_t request[67] = { 0 };
    memcpy(request, "DS3WORK", 7);
    request[9] = 1;
    request[11] = 64;
    request[17] = 1;
    request[19] = 1;
    request[31] = 42;
    request[40] = 1;
    request[63] = 3;
    memcpy(request + 64, "ds3", 3);
    result = dust3d_worker_service_queue_submit(queue, request, sizeof(request));
    if (result.status != DUST3D_WORKER_QUEUE_READY)
        return 2;
    request[64] = 'x';
    result = dust3d_worker_service_queue_begin(queue, 0);
    if (result.status != DUST3D_WORKER_QUEUE_REJECTED)
        return 3;
    result = dust3d_worker_service_queue_begin(queue, 9);
    if (result.status != DUST3D_WORKER_QUEUE_BEGUN)
        return 4;
    size_t size = 0;
    const uint8_t* bytes = dust3d_worker_service_queue_request_bytes(queue, &size);
    if (!bytes || size != sizeof(request) || bytes[64] != 'd')
        return 5;
    dust3d_worker_service_queue_destroy(queue);
    dust3d_worker_service_queue_destroy(NULL);
    return 0;
}
