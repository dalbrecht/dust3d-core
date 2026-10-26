/* SPDX-License-Identifier: MIT */
#include <Dust3DWorkerImageDecoder.h>
int main(void)
{
    uint8_t input[4] = { 1, 2, 3, 0 };
    uint8_t output[4] = { 0 };
    Dust3DWorkerImageResult result = { 0 };
    Dust3DWorkerImage* image = dust3d_worker_image_create(1, 1, input, 4, 4, &result);
    if (!image || result.status != DUST3D_WORKER_IMAGE_OK)
        return 1;
    if (!dust3d_worker_image_copy_pixels(image, 0, output, 4) || output[0] != 1 || output[3] != 0) {
        dust3d_worker_image_destroy(image);
        return 1;
    }
    dust3d_worker_image_destroy(image);
    return DUST3D_WORKER_IMAGE_ABI_VERSION == 1 ? 0 : 1;
}
