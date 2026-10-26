/* SPDX-License-Identifier: MIT */
#include <Dust3DWorkerRuntime.h>
#include <string.h>
int main(void)
{
    Dust3DWorkerRequestIdentity identity = { { 1 }, 42, 7 };
    const uint8_t archive[3] = { 0, 255, 42 };
    Dust3DWorkerRequestResult result;
    Dust3DWorkerRequest* owner = dust3d_worker_request_create(DUST3D_WORKER_OPERATION_MESH, identity, archive, 3, NULL, (Dust3DWorkerWireLimits) { 64, 64, 64 }, &result);
    if (!owner || result.status != DUST3D_WORKER_REQUEST_OK)
        return 1;
    size_t size = 0;
    const uint8_t* header = dust3d_worker_request_header(owner, &size);
    if (!header || size != 64 || memcmp(header, "DS3WORK", 7) || header[17] != 1 || header[19] != 1 || header[31] != 42 || header[39] != 7 || header[40] != 1 || header[63] != 3)
        return 2;
    const uint8_t* bytes = dust3d_worker_request_bytes(owner, &size);
    if (!bytes || size != 67 || memcmp(bytes + 64, archive, 3))
        return 3;
    dust3d_worker_request_destroy(owner);
    Dust3DWorkerRequestImportedOptions imported = { 0 };
    imported.struct_size = sizeof(imported);
    imported.maximum_decoded_pixel_bytes = 16;
    owner = dust3d_worker_imported_request_create(identity, archive, sizeof(archive), &imported,
        (Dust3DWorkerWireLimits) { 4096, 4096, 4096 }, &result);
    if (!owner || result.status != DUST3D_WORKER_REQUEST_OK)
        return 4;
    bytes = dust3d_worker_request_bytes(owner, &size);
    if (!bytes || size != 147 || bytes[19] != 6 || bytes[65] != 1 || bytes[67] != 80
        || memcmp(bytes + 144, archive, sizeof(archive)))
        return 5;
    dust3d_worker_request_destroy(owner);
    dust3d_worker_request_destroy(NULL);
    return 0;
}
