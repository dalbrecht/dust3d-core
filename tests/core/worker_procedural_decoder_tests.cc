// SPDX-License-Identifier: MIT
#include <Dust3DWorkerRuntime.h>
#include <iostream>
#include <native/WorkerRuntime/WorkerImageDecoder.h>
#include <stdexcept>
using namespace dust3d::worker;
namespace {
Dust3DWorkerImage* decode(void* context, const Dust3DWorkerImageInput* input, Dust3DWorkerImageResult* result)
{
    auto* calls = static_cast<size_t*>(context);
    ++*calls;
    if (!input || input->encoding != 1 || input->encoded_count != 3 || input->model_count != 38) {
        *result = dust3d_worker_image_error_create(DUST3D_WORKER_IMAGE_INVALID_INPUT, 17, nullptr, 0);
        return nullptr;
    }
    const uint8_t pixels[] = { 17, 23, 41, 0 };
    return dust3d_worker_image_create(1, 1, pixels, sizeof(pixels), input->maximum_decoded_bytes, result);
}
}
int main()
{
    try {
        Dust3DWorkerDecoders invalid {};
        if (dust3d_worker_serve_with_decoders(-1, -1, &invalid).status != DUST3D_WORKER_JOB_CODEC_REJECTED)
            throw std::runtime_error("Invalid paired layout must reject before channel access");
        invalid.struct_size = sizeof(invalid);
        invalid.procedural.struct_size = sizeof(Dust3DWorkerImageDecoder);
        if (dust3d_worker_serve_with_decoders(-1, -1, &invalid).status != DUST3D_WORKER_JOB_CODEC_REJECTED)
            throw std::runtime_error("Partial present procedural slot must reject before channel access");
        invalid.procedural = {};
        if (dust3d_worker_serve_with_decoders(-1, -1, &invalid).status != DUST3D_WORKER_JOB_PROTOCOL_REJECTED)
            throw std::runtime_error("Valid unavailable slots proceed to independent channel validation");
        size_t calls = 0;
        CallbackProceduralImageDecoder decoder({ sizeof(Dust3DWorkerImageDecoder), &calls, decode });
        ProceduralImageInput input { "{eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee}", "images/example.png", { 1, 2, 3 } };
        const auto value = decoder.decode(input, 3, 4);
        if (calls != 1 || value.width != 1 || value.height != 1 || value.pixels != std::vector<uint8_t>({ 17, 23, 41, 0 }))
            throw std::runtime_error("Explicit procedural callback preserves independent straight pixel ownership");
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
    return 0;
}
