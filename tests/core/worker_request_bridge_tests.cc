// SPDX-License-Identifier: MIT
#include <Dust3DWorkerRuntime.h>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <memory>
#include <native/Engine/WorkerUvRequest.h>
#include <stdexcept>
using namespace dust3d::worker;
namespace {
void require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}
using Owner = std::unique_ptr<Dust3DWorkerRequest, decltype(&dust3d_worker_request_destroy)>;
Dust3DWorkerRequestIdentity identity()
{
    Dust3DWorkerRequestIdentity value {};
    value.document[0] = 1;
    value.document[15] = 255;
    value.request = 42;
    value.revision = 7;
    return value;
}
std::vector<uint8_t> copied(const Owner& owner)
{
    size_t size = 0;
    const auto* bytes = dust3d_worker_request_bytes(owner.get(), &size);
    require(bytes && size > 64, "Complete immutable request frame available");
    return { bytes, bytes + size };
}
void baseAndUv()
{
    std::vector<uint8_t> archive { 0, 255, 42 };
    Dust3DWorkerRequestResult result {};
    Owner owner(dust3d_worker_request_create(DUST3D_WORKER_OPERATION_MESH, identity(), archive.data(), archive.size(), nullptr, { 128, 128, 64 }, &result), dust3d_worker_request_destroy);
    require(owner && result.status == DUST3D_WORKER_REQUEST_OK && !result.message[0] && result.detail_source == DUST3D_WORKER_DETAIL_NONE, "Base encoder succeeds with owned receipt");
    size_t size = 0;
    const auto* header = dust3d_worker_request_header(owner.get(), &size);
    require(header && size == 64 && header[17] == 1 && header[19] == 1 && header[31] == 42 && header[39] == 7 && header[40] == 1 && header[55] == 255, "Independent header fields preserve parent identity");
    auto bytes = copied(owner);
    owner.reset();
    auto decoded = decodeFrame(bytes.data(), bytes.size(), {});
    require(decoded.payload == archive && decoded.header.identity.request == 42 && decoded.header.identity.revision == 7, "Owned base frame survives C owner destruction");
    std::string key = "{ABCDEF01-2345-6789-abcd-0123456789AB}";
    Dust3DWorkerRequestImage image { key.data(), key.size(), 17, 8192 };
    Dust3DWorkerRequestUvOptions options { 128, 65536, &image, 1 };
    owner.reset(dust3d_worker_request_create(DUST3D_WORKER_OPERATION_UV_MESH, identity(), archive.data(), archive.size(), &options, { 128, 128, 64 }, &result));
    require(owner && result.status == DUST3D_WORKER_REQUEST_OK, "UV encoder accepts resolved image values");
    bytes = copied(owner);
    owner.reset();
    archive.assign(archive.size(), 19);
    key.assign(key.size(), '!');
    options = {};
    image = {};
    decoded = decodeFrame(bytes.data(), bytes.size(), {});
    const auto uv = decodeUvRequest(decoded.payload.data(), decoded.payload.size(), {});
    require(decoded.header.operation == Operation::generateUvMesh && uv.archive == std::vector<uint8_t>({ 0, 255, 42 }) && uv.textureSize == 128 && uv.metadataBudget == 65536 && uv.images.size() == 1 && uv.images[0].identity == "{ABCDEF01-2345-6789-abcd-0123456789AB}" && uv.images[0].width == 17 && uv.images[0].height == 8192, "UV archive/options/original key independently owned");
    const uint8_t opaque[] = { 1 };
    owner.reset(dust3d_worker_request_create(DUST3D_WORKER_OPERATION_UV_MESH, identity(), opaque, 1, nullptr, { 64, 64, 64 }, &result));
    require(owner && result.status == DUST3D_WORKER_REQUEST_OK, "NULL UV options retain defaults");
    bytes = copied(owner);
    decoded = decodeFrame(bytes.data(), bytes.size(), {});
    const auto defaults = decodeUvRequest(decoded.payload.data(), decoded.payload.size(), {});
    require(!defaults.textureSize && !defaults.metadataBudget && defaults.images.empty(), "Native defaults remain explicit zero options");
    require(!dust3d_worker_request_bytes(owner.get(), nullptr) && !dust3d_worker_request_header(owner.get(), nullptr), "NULL required output count rejects");
    size = 123;
    require(!dust3d_worker_request_bytes(nullptr, &size) && !size, "NULL frame owner clears size");
    size = 123;
    require(!dust3d_worker_request_header(nullptr, &size) && !size, "NULL header owner clears size");
    dust3d_worker_request_destroy(nullptr);
}
void reject(Dust3DWorkerRequest* owner, const Dust3DWorkerRequestResult& result, Dust3DWorkerRequestStatus status, Dust3DWorkerDetailSource source, int code)
{
    Owner unexpected(owner, dust3d_worker_request_destroy);
    require(!unexpected && result.status == status && result.detail_source == source && result.detail_code == code && result.message[0] && !result.message[511] && !result.message_truncated, "Original typed bounded encoder rejection crosses C");
}
void invalidRequests()
{
    const uint8_t archive[] = { 1, 2, 3 };
    auto request = identity();
    Dust3DWorkerRequestResult result {};
    const Dust3DWorkerWireLimits limits { 128, 128, 64 };
    require(!dust3d_worker_request_create(DUST3D_WORKER_OPERATION_MESH, request, archive, 3, nullptr, limits, nullptr), "Missing result cannot create owner");
    auto check = [&](Dust3DWorkerOperation op, const uint8_t* data, size_t size, const Dust3DWorkerRequestUvOptions* uv, Dust3DWorkerWireLimits policy, Dust3DWorkerRequestStatus status, Dust3DWorkerDetailSource source, int code) {
        auto* owner = dust3d_worker_request_create(op, request, data, size, uv, policy, &result);
        reject(owner, result, status, source, code);
    };
    auto protocol = [&](ProtocolErrorCode code, const uint8_t* data, size_t size, Dust3DWorkerWireLimits policy) { check(DUST3D_WORKER_OPERATION_MESH, data, size, nullptr, policy, DUST3D_WORKER_REQUEST_PROTOCOL_REJECTED, DUST3D_WORKER_DETAIL_FRAME, static_cast<int>(code)); };
    protocol(ProtocolErrorCode::invalidInput, nullptr, 3, limits);
    const auto saved = result;
    protocol(ProtocolErrorCode::invalidInput, archive, 0, limits);
    protocol(ProtocolErrorCode::invalidLimits, archive, 3, { 0, 128, 64 });
    protocol(ProtocolErrorCode::limitExceeded, archive, 3, { 2, 128, 64 });
    require(saved.detail_code == static_cast<int>(ProtocolErrorCode::invalidInput) && saved.message[0], "Original receipt independent across calls");
    check(DUST3D_WORKER_OPERATION_UNKNOWN, archive, 3, nullptr, limits, DUST3D_WORKER_REQUEST_PROTOCOL_REJECTED, DUST3D_WORKER_DETAIL_FRAME, static_cast<int>(ProtocolErrorCode::unsupportedOperation));
    Dust3DWorkerRequestUvOptions options {};
    check(DUST3D_WORKER_OPERATION_MESH, archive, 3, &options, limits, DUST3D_WORKER_REQUEST_PROTOCOL_REJECTED, DUST3D_WORKER_DETAIL_FRAME, static_cast<int>(ProtocolErrorCode::invalidInput));
    request.request = 0;
    protocol(ProtocolErrorCode::invalidIdentity, archive, 3, limits);
    request = identity();
    std::fill(std::begin(request.document), std::end(request.document), 0);
    protocol(ProtocolErrorCode::invalidIdentity, archive, 3, limits);
    request = identity();
    auto uv = [&](UvRequestErrorCode code) { check(DUST3D_WORKER_OPERATION_UV_MESH, archive, 3, &options, limits, DUST3D_WORKER_REQUEST_UV_REJECTED, DUST3D_WORKER_DETAIL_UV_REQUEST, static_cast<int>(code)); };
    options.image_count = 1;
    uv(UvRequestErrorCode::invalidInput);
    options = {};
    options.texture_size = 8193;
    uv(UvRequestErrorCode::invalidOptions);
    options = {};
    options.metadata_budget = maximumPayloadBytes + 1;
    uv(UvRequestErrorCode::invalidOptions);
    std::string key = "abcdef01-2345-6789-abcd-0123456789ab";
    Dust3DWorkerRequestImage images[] = { { nullptr, 36, 1, 1 }, { key.data(), key.size(), 1, 1 } };
    options = { 0, 0, images, 1 };
    uv(UvRequestErrorCode::invalidOptions);
    images[0] = { key.data(), 35, 1, 1 };
    uv(UvRequestErrorCode::invalidOptions);
    images[0] = { key.data(), key.size(), 0, 1 };
    uv(UvRequestErrorCode::invalidOptions);
    images[0] = { key.data(), key.size(), 1, 8193 };
    uv(UvRequestErrorCode::invalidOptions);
    images[0] = { key.data(), key.size(), 1, 1 };
    key[0] = '!';
    uv(UvRequestErrorCode::invalidOptions);
    key[0] = 'a';
    std::string alias = "{ABCDEF01-2345-6789-ABCD-0123456789AB}";
    images[1] = { alias.data(), alias.size(), 1, 1 };
    options.image_count = 2;
    check(DUST3D_WORKER_OPERATION_UV_MESH, archive, 3, &options, { 256, 128, 64 }, DUST3D_WORKER_REQUEST_UV_REJECTED, DUST3D_WORKER_DETAIL_UV_REQUEST, static_cast<int>(UvRequestErrorCode::invalidOptions));
    options = {};
    check(DUST3D_WORKER_OPERATION_UV_MESH, archive, 3, &options, { 34, 128, 64 }, DUST3D_WORKER_REQUEST_PROTOCOL_REJECTED, DUST3D_WORKER_DETAIL_FRAME, static_cast<int>(ProtocolErrorCode::limitExceeded));
}
void borrowedView()
{
    const uint8_t archive[] = { 0, 255, 42 };
    const std::vector<UvRequestImage> images { { "abcdef01-2345-6789-abcd-0123456789ab", 17, 32 } };
    UvRequest owned;
    owned.archive = { 0, 255, 42 };
    owned.images = images;
    owned.textureSize = 128;
    owned.metadataBudget = 65536;
    const auto encoded = encodeUvRequestView({ archive, 3, images, 128, 65536 }, {});
    require(encoded == encodeUvRequest(owned, {}), "Borrowed and owned codec share exact format");
    try {
        encodeUvRequestView({ nullptr, 3, images, 128, 65536 }, {});
        throw std::runtime_error("Null borrowed archive accepted");
    } catch (const UvRequestError& cause) {
        require(cause.code() == UvRequestErrorCode::invalidInput, "Borrowed span rejects before copy");
    }
}
}
int main()
{
    try {
        baseAndUv();
        invalidRequests();
        borrowedView();
        std::cout << "C request ownership and borrowed UV encoding passed\n";
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}
