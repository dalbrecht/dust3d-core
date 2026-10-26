// SPDX-License-Identifier: MIT
#include "WorkerSourceRequest.h"
#include "WorkerUvRequest.h"
#include <Dust3DWorkerRuntime.h>
#include <algorithm>
#include <iterator>
#include <memory>
#include <new>
using namespace dust3d::worker;
struct Dust3DWorkerRequest {
    std::vector<uint8_t> bytes;
};
namespace {
void copyCause(Dust3DWorkerRequestResult& result, const char* cause) noexcept
{
    if (!cause)
        return;
    size_t size = 0;
    while (size < sizeof(result.message) - 1 && cause[size]) {
        result.message[size] = cause[size];
        ++size;
    }
    result.message[size] = 0;
    result.message_truncated = cause[size] != 0;
}
const uint8_t* borrow(const Dust3DWorkerRequest* request, size_t* size, bool headerOnly) noexcept
{
    if (!size)
        return nullptr;
    *size = 0;
    if (!request)
        return nullptr;
    *size = headerOnly ? headerBytes : request->bytes.size();
    return request->bytes.data();
}
}
extern "C" Dust3DWorkerRequest* dust3d_worker_request_create(Dust3DWorkerOperation operation,
    Dust3DWorkerRequestIdentity identity, const uint8_t* archive, size_t size,
    const Dust3DWorkerRequestUvOptions* options, Dust3DWorkerWireLimits limits, Dust3DWorkerRequestResult* result) noexcept
{
    if (!result)
        return nullptr;
    *result = {};
    result->status = DUST3D_WORKER_REQUEST_INTERNAL_FAILED;
    try {
        if (!archive || !size)
            throw ProtocolError(ProtocolErrorCode::invalidInput, "Worker request archive is null or empty.");
        if (size > maximumPayloadBytes)
            throw ProtocolError(ProtocolErrorCode::limitExceeded, "Worker archive exceeds supported request size.");
        Header request;
        if (operation == DUST3D_WORKER_OPERATION_MESH)
            request.operation = Operation::generateMesh;
        else if (operation == DUST3D_WORKER_OPERATION_UV_MESH)
            request.operation = Operation::generateUvMesh;
        else if (operation == DUST3D_WORKER_OPERATION_INDEXED_MESH)
            request.operation = Operation::generateIndexedMesh;
        else
            throw ProtocolError(ProtocolErrorCode::unsupportedOperation, "Worker request operation is unknown.");
        if (operation != DUST3D_WORKER_OPERATION_UV_MESH && options)
            throw ProtocolError(ProtocolErrorCode::invalidInput, "Base/indexed mesh request cannot carry UV options.");
        UvRequestLimits uvLimits;
        uvLimits.maximumWireBytes = limits.maximum_input_bytes;
        const auto imageCount = options ? options->image_count : 0;
        if (imageCount > uvLimits.maximumImages)
            throw UvRequestError(UvRequestErrorCode::limitExceeded, "Worker UV image count exceeds policy.");
        if (imageCount && !options->images)
            throw UvRequestError(UvRequestErrorCode::invalidInput, "Worker UV image span is null.");
        request.payloadBytes = size;
        if (operation == DUST3D_WORKER_OPERATION_UV_MESH)
            request.payloadBytes += uvRequestPrefixBytes + imageCount * uvRequestImageBytes;
        std::copy(std::begin(identity.document), std::end(identity.document), request.identity.document.begin());
        request.identity.request = identity.request;
        request.identity.revision = identity.revision;
        const auto header = encodeHeader(request, { limits.maximum_input_bytes, limits.maximum_output_bytes, limits.maximum_failure_bytes });
        auto owner = std::make_unique<Dust3DWorkerRequest>();
        if (operation != DUST3D_WORKER_OPERATION_UV_MESH)
            owner->bytes.assign(archive, archive + size);
        else {
            std::vector<UvRequestImage> images;
            images.reserve(imageCount);
            for (size_t i = 0; i < imageCount; ++i) {
                const auto& image = options->images[i];
                if (!image.identity || (image.identity_size != 36 && image.identity_size != 38))
                    throw UvRequestError(UvRequestErrorCode::invalidOptions, "Worker UV identity text span is invalid.");
                images.push_back({ std::string(image.identity, image.identity_size), image.width, image.height });
            }
            owner->bytes = encodeUvRequestView({ archive, size, images, options ? options->texture_size : 0, options ? options->metadata_budget : 0 }, uvLimits);
        }
        owner->bytes.insert(owner->bytes.begin(), header.begin(), header.end());
        result->status = DUST3D_WORKER_REQUEST_OK;
        return owner.release();
    } catch (const ProtocolError& cause) {
        result->status = DUST3D_WORKER_REQUEST_PROTOCOL_REJECTED;
        result->detail_source = DUST3D_WORKER_DETAIL_FRAME;
        result->detail_code = static_cast<int32_t>(cause.code());
        copyCause(*result, cause.what());
    } catch (const UvRequestError& cause) {
        result->status = DUST3D_WORKER_REQUEST_UV_REJECTED;
        result->detail_source = DUST3D_WORKER_DETAIL_UV_REQUEST;
        result->detail_code = static_cast<int32_t>(cause.code());
        copyCause(*result, cause.what());
    } catch (const std::bad_alloc& cause) {
        result->status = DUST3D_WORKER_REQUEST_LIMIT_EXCEEDED;
        result->detail_source = DUST3D_WORKER_DETAIL_RUNTIME;
        copyCause(*result, cause.what());
    } catch (const std::exception& cause) {
        result->detail_source = DUST3D_WORKER_DETAIL_RUNTIME;
        copyCause(*result, cause.what());
    } catch (...) {
        result->detail_source = DUST3D_WORKER_DETAIL_RUNTIME;
        copyCause(*result, "Worker request exception has no available diagnostic.");
    }
    return nullptr;
}
extern "C" const uint8_t* dust3d_worker_request_bytes(const Dust3DWorkerRequest* request, size_t* size) noexcept { return borrow(request, size, false); }
namespace {
Dust3DWorkerRequest* createSourceRequest(Operation operation, Dust3DWorkerRequestIdentity identity,
    const uint8_t* archive, size_t size, uint64_t maximum, Dust3DWorkerWireLimits limits, Dust3DWorkerRequestResult* result) noexcept
{
    if (!result)
        return nullptr;
    *result = {};
    result->status = DUST3D_WORKER_REQUEST_INTERNAL_FAILED;
    try {
        if (!archive || !size)
            throw SourceRequestError(SourceRequestErrorCode::invalidInput, "Source request archive is null or empty.");
        if (size > maximumPayloadBytes - sourceRequestPrefixBytes)
            throw SourceRequestError(SourceRequestErrorCode::limitExceeded, "Source archive exceeds supported request size.");
        Header request;
        request.operation = operation;
        request.payloadBytes = sourceRequestPrefixBytes + size;
        std::copy(std::begin(identity.document), std::end(identity.document), request.identity.document.begin());
        request.identity.request = identity.request;
        request.identity.revision = identity.revision;
        const auto header = encodeHeader(request, { limits.maximum_input_bytes, limits.maximum_output_bytes, limits.maximum_failure_bytes });
        auto owner = std::make_unique<Dust3DWorkerRequest>();
        owner->bytes = encodeSourceRequestView({ archive, size, maximum }, { limits.maximum_input_bytes, limits.maximum_input_bytes });
        owner->bytes.insert(owner->bytes.begin(), header.begin(), header.end());
        result->status = DUST3D_WORKER_REQUEST_OK;
        return owner.release();
    } catch (const ProtocolError& cause) {
        result->status = DUST3D_WORKER_REQUEST_PROTOCOL_REJECTED;
        result->detail_source = DUST3D_WORKER_DETAIL_FRAME;
        result->detail_code = static_cast<int32_t>(cause.code());
        copyCause(*result, cause.what());
    } catch (const SourceRequestError& cause) {
        result->status = DUST3D_WORKER_REQUEST_SOURCE_REJECTED;
        result->detail_source = DUST3D_WORKER_DETAIL_SOURCE_REQUEST;
        result->detail_code = static_cast<int32_t>(cause.code());
        copyCause(*result, cause.what());
    } catch (const std::bad_alloc& cause) {
        result->status = DUST3D_WORKER_REQUEST_LIMIT_EXCEEDED;
        result->detail_source = DUST3D_WORKER_DETAIL_RUNTIME;
        copyCause(*result, cause.what());
    } catch (const std::exception& cause) {
        result->detail_source = DUST3D_WORKER_DETAIL_RUNTIME;
        copyCause(*result, cause.what());
    } catch (...) {
        result->detail_source = DUST3D_WORKER_DETAIL_RUNTIME;
        copyCause(*result, "Source request exception has no available diagnostic.");
    }
    return nullptr;
}
}
extern "C" Dust3DWorkerRequest* dust3d_worker_source_request_create(Dust3DWorkerRequestIdentity identity,
    const uint8_t* archive, size_t size, uint64_t maximum, Dust3DWorkerWireLimits limits, Dust3DWorkerRequestResult* result) noexcept
{
    return createSourceRequest(Operation::generateSourcedMesh, identity, archive, size, maximum, limits, result);
}
extern "C" Dust3DWorkerRequest* dust3d_worker_indexed_source_request_create(Dust3DWorkerRequestIdentity identity,
    const uint8_t* archive, size_t size, uint64_t maximum, Dust3DWorkerWireLimits limits, Dust3DWorkerRequestResult* result) noexcept
{
    return createSourceRequest(Operation::generateIndexedSourcedMesh, identity, archive, size, maximum, limits, result);
}
extern "C" const uint8_t* dust3d_worker_request_header(const Dust3DWorkerRequest* request, size_t* size) noexcept { return borrow(request, size, true); }
extern "C" void dust3d_worker_request_destroy(Dust3DWorkerRequest* request) noexcept { delete request; }
