// SPDX-License-Identifier: MIT
#include <Dust3DWorkerRuntime.h>
#include <iostream>
#include <memory>
#include <native/Engine/WorkerLegacyRequest.h>
#include <stdexcept>
using namespace dust3d::worker;
namespace {
void require(bool value, const char* cause)
{
    if (!value)
        throw std::runtime_error(cause);
}
}
int main()
{
    try {
        LegacyRequest value { { 0, 255, 42 }, {} };
        Dust3DWorkerRequestIdentity identity {};
        identity.document[0] = 1;
        identity.request = 7;
        Dust3DWorkerRequestLegacyOptions options {};
        options.struct_size = sizeof(options);
        options.maximum_uv_native_bytes = 1024;
        options.maximum_output_wire_bytes = 8192;
        Dust3DWorkerRequestResult result {};
        std::unique_ptr<Dust3DWorkerRequest, decltype(&dust3d_worker_request_destroy)> owner(
            dust3d_worker_legacy_request_create(identity, value.archive.data(), value.archive.size(), &options, { 4096, 4096, 4096 }, &result), dust3d_worker_request_destroy);
        require(owner && result.status == DUST3D_WORKER_REQUEST_OK, "Dedicated C request encoder owns explicit legacy options");
        size_t count = 0;
        const auto* span = dust3d_worker_request_bytes(owner.get(), &count);
        auto framed = decodeFrame(span, count, {});
        auto request = decodeLegacyRequest(framed.payload.data(), framed.payload.size(), {});
        require(framed.header.operation == Operation::generateLegacyBaked && request.archive == value.archive
                && request.options.maximumUvNativeBytes == 1024 && request.options.maximumOutputWireBytes == 4096,
            "C factory preserves archive and tightens requested output to authoritative wire policy");
        owner.reset(dust3d_worker_request_create(DUST3D_WORKER_OPERATION_LEGACY_BAKED, identity, value.archive.data(), value.archive.size(), nullptr, { 4096, 4096, 4096 }, &result));
        require(owner && result.status == DUST3D_WORKER_REQUEST_OK, "Generic legacy request uses the same explicit default codec");
        const Dust3DWorkerWireLimits wireLimits { 4096, 4096, 4096 };
        require(!dust3d_worker_legacy_request_create(identity, value.archive.data(), value.archive.size(), nullptr, wireLimits, nullptr),
            "Required result storage rejects before allocation");
        for (int guard = 0; guard < 5; ++guard) {
            auto invalid = options;
            auto invalidIdentity = identity;
            auto invalidLimits = wireLimits;
            if (guard == 2)
                invalid.maximum_encoded_image_bytes = maximumPayloadBytes + 1;
            if (guard == 3)
                invalidIdentity.request = 0;
            if (guard == 4)
                invalidLimits.maximum_input_bytes = legacyRequestPrefixBytes;
            owner.reset(dust3d_worker_legacy_request_create(invalidIdentity,
                guard == 0 ? nullptr : value.archive.data(), guard == 1 ? 0 : value.archive.size(), &invalid, invalidLimits, &result));
            require(!owner && result.status != DUST3D_WORKER_REQUEST_OK && result.message[0],
                "Scalar request guards reject with an original diagnostic before archive parsing");
        }
        Dust3DWorkerRequestUvOptions uv {};
        owner.reset(dust3d_worker_request_create(DUST3D_WORKER_OPERATION_LEGACY_BAKED, identity, value.archive.data(), value.archive.size(), &uv, wireLimits, &result));
        require(!owner && result.status == DUST3D_WORKER_REQUEST_PROTOCOL_REJECTED,
            "Legacy generic requests reject unrelated UV options");
        options.struct_size = 0;
        owner.reset(dust3d_worker_legacy_request_create(identity, value.archive.data(), value.archive.size(), &options, { 4096, 4096, 4096 }, &result));
        require(!owner && result.status == DUST3D_WORKER_REQUEST_LEGACY_REJECTED && result.detail_source == DUST3D_WORKER_DETAIL_LEGACY_REQUEST,
            "C layout failure retains its original typed request category");
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
    return 0;
}
