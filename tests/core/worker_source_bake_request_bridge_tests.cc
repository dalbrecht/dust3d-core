// SPDX-License-Identifier: MIT
#include <Dust3DWorkerRuntime.h>
#include <iostream>
#include <memory>
#include <native/Engine/WorkerSourceBakeRequest.h>
using namespace dust3d::worker;
int main()
{
    try {
        const uint8_t archive[] = { 0, 255, 42 };
        Dust3DWorkerRequestIdentity identity {};
        identity.document[0] = 1;
        identity.request = 7;
        Dust3DWorkerRequestSourceBakeOptions options {};
        options.struct_size = sizeof(options);
        options.legacy.struct_size = sizeof(options.legacy);
        options.legacy.color_size = 8;
        options.maximum_chart_count = 12;
        options.should_generate_mips = 1;
        options.maximum_levels = 3;
        options.maximum_output_wire_bytes = 8192;
        Dust3DWorkerRequestResult result {};
        std::unique_ptr<Dust3DWorkerRequest, decltype(&dust3d_worker_request_destroy)> owner(
            dust3d_worker_source_bake_request_create(identity, archive, sizeof(archive), &options, { 4096, 4096, 4096 }, &result), dust3d_worker_request_destroy);
        if (!owner || result.status != DUST3D_WORKER_REQUEST_OK)
            throw std::runtime_error("C source request factory must own complete framing");
        size_t size = 0;
        const auto* bytes = dust3d_worker_request_bytes(owner.get(), &size);
        const auto frame = decodeFrame(bytes, size, {});
        const auto value = decodeSourceBakeRequest(frame.payload.data(), frame.payload.size(), {});
        if (frame.header.operation != Operation::generateSourceBaked || value.legacy.archive != std::vector<uint8_t>({ 0, 255, 42 })
            || value.legacy.options.colorSize != 8 || value.options.maximumChartCount != 12 || value.options.maximumOutputWireBytes != 4096)
            throw std::runtime_error("C source request preserves exact archive/settings and tightens output policy");
        owner.reset(dust3d_worker_request_create(DUST3D_WORKER_OPERATION_SOURCE_BAKED, identity, archive, sizeof(archive), nullptr,
            { 4096, 4096, 4096 }, &result));
        if (!owner || result.status != DUST3D_WORKER_REQUEST_OK)
            throw std::runtime_error("Generic source factory delegates to the same default source codec");
        options.legacy.struct_size = 0;
        owner.reset(dust3d_worker_source_bake_request_create(identity, archive, sizeof(archive), &options, { 4096, 4096, 4096 }, &result));
        if (owner || result.status != DUST3D_WORKER_REQUEST_SOURCE_BAKE_REJECTED
            || result.detail_source != DUST3D_WORKER_DETAIL_SOURCE_BAKE_REQUEST)
            throw std::runtime_error("Nested C option layout rejects with original source request category");
        if (dust3d_worker_source_bake_request_create(identity, archive, sizeof(archive), nullptr, { 4096, 4096, 4096 }, nullptr))
            throw std::runtime_error("Missing result storage rejects without returned ownership");
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
    return 0;
}
