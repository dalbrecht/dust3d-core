// SPDX-License-Identifier: MIT
#include <Dust3DBridge.h>
#include <algorithm>
#include <array>
#include <iostream>
#include <iterator>
#include <memory>
#include <native/Engine/WorkerFailure.h>
#include <stdexcept>
#include <vector>

namespace {
using namespace dust3d::worker;
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
template <typename Action>
void reject(FailurePayloadErrorCode code, Action action)
{
    try {
        action();
    } catch (const FailurePayloadError& cause) {
        require(cause.code() == code && cause.what()[0], "Failure payload errors preserve a typed cause");
        return;
    }
    throw std::runtime_error("Invalid worker failure payload was admitted");
}
void wireAndOwnership()
{
    FailurePayload value { FailureStatus::invalidDocument, { 'b', 'a', 'd' } };
    auto wire = encodeFailurePayload(value, {});
    const std::array<uint8_t, 19> expected { 0, 1, 0, 16, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 'b', 'a', 'd' };
    require(wire.size() == expected.size() && std::equal(expected.begin(), expected.end(), wire.begin()), "Independent failure wire fixture must match exactly");
    auto copied = decodeFailurePayload(wire.data(), wire.size(), {});
    value.cause.assign(3, 0);
    wire.assign(wire.size(), 255);
    require(copied.status == FailureStatus::invalidDocument && copied.cause == std::vector<uint8_t>({ 'b', 'a', 'd' }), "Decoded failure owns exact original cause bytes");
    uint8_t wireStatus = 1;
    for (const auto status : { FailureStatus::invalidDocument, FailureStatus::unsupportedAsset, FailureStatus::generationFailed,
             FailureStatus::limitExceeded, FailureStatus::noGeometry, FailureStatus::cancelled }) {
        const auto empty = encodeFailurePayload({ status, {} }, {});
        const auto result = decodeFailurePayload(empty.data(), empty.size(), {});
        require(empty[4] == 0 && empty[5] == wireStatus++ && result.status == status && result.cause.empty(), "Every typed non-success status supports an originally empty diagnostic");
    }
    const FailurePayload opaque { FailureStatus::generationFailed, { 0, 255, 27, 0xc0, 0x80, 1 } };
    wire = encodeFailurePayload(opaque, {});
    require(decodeFailurePayload(wire.data(), wire.size(), {}).cause == opaque.cause, "Malformed text and control bytes are retained as opaque data, never interpreted");
}
void bounds()
{
    const FailurePayload value { FailureStatus::generationFailed, { 1, 2, 3 } };
    const FailurePayloadLimits exact { 3, 19 };
    const auto wire = encodeFailurePayload(value, exact);
    require(decodeFailurePayload(wire.data(), wire.size(), exact).cause == value.cause, "Exact cause and complete-wire quotas succeed");
    for (const auto& less : { FailurePayloadLimits { 2, 19 }, FailurePayloadLimits { 3, 18 }, FailurePayloadLimits { 3, 1 } }) {
        reject(FailurePayloadErrorCode::limitExceeded, [&] { encodeFailurePayload(value, less); });
        reject(FailurePayloadErrorCode::limitExceeded, [&] { decodeFailurePayload(wire.data(), wire.size(), less); });
    }
    for (const auto& invalid : { FailurePayloadLimits { 0, 19 }, FailurePayloadLimits { maximumFailureCauseBytes + 1, 19 },
             FailurePayloadLimits { 3, 0 }, FailurePayloadLimits { 3, maximumDiagnosticBytes + 1 } }) {
        reject(FailurePayloadErrorCode::invalidLimits, [&] { encodeFailurePayload(value, invalid); });
        reject(FailurePayloadErrorCode::invalidLimits, [&] { decodeFailurePayload(wire.data(), wire.size(), invalid); });
    }
    auto advertised = wire;
    std::fill(advertised.begin() + 12, advertised.begin() + 16, 255);
    reject(FailurePayloadErrorCode::limitExceeded, [&] { decodeFailurePayload(advertised.data(), advertised.size(), {}); });
}
void malformed()
{
    const auto wire = encodeFailurePayload({ FailureStatus::cancelled, { 's', 't', 'o', 'p' } }, {});
    for (size_t size = 0; size < wire.size(); ++size)
        reject(FailurePayloadErrorCode::truncated, [&] { decodeFailurePayload(wire.data(), size, {}); });
    reject(FailurePayloadErrorCode::truncated, [&] { decodeFailurePayload(nullptr, 0, {}); });
    reject(FailurePayloadErrorCode::invalidInput, [&] { decodeFailurePayload(nullptr, wire.size(), {}); });
    auto extra = wire;
    extra.push_back(0);
    reject(FailurePayloadErrorCode::trailingData, [&] { decodeFailurePayload(extra.data(), extra.size(), {}); });
    auto mutate = [&](size_t offset, uint8_t byte, FailurePayloadErrorCode code) {
        auto changed = wire;
        changed[offset] = byte;
        reject(code, [&] { decodeFailurePayload(changed.data(), changed.size(), {}); });
    };
    mutate(1, 3, FailurePayloadErrorCode::unsupportedVersion);
    mutate(3, 15, FailurePayloadErrorCode::invalidLayout);
    mutate(7, 1, FailurePayloadErrorCode::invalidLayout);
    mutate(11, 1, FailurePayloadErrorCode::invalidLayout);
    mutate(5, 0, FailurePayloadErrorCode::invalidStatus);
    mutate(5, 7, FailurePayloadErrorCode::invalidStatus);
    for (const auto invalid : { uint16_t { 0 }, uint16_t { 7 }, uint16_t { 65535 } })
        reject(FailurePayloadErrorCode::invalidStatus, [&] { encodeFailurePayload({ static_cast<FailureStatus>(invalid), {} }, {}); });
}
void nativeCause()
{
    const std::array<uint8_t, 3> malformedArchive { 1, 2, 3 };
    Dust3DError error {};
    std::unique_ptr<Dust3DMesh, decltype(&dust3d_mesh_destroy)> owner(dust3d_mesh_load(malformedArchive.data(), malformedArchive.size(), &error), dust3d_mesh_destroy);
    require(owner == nullptr && error.status == DUST3D_STATUS_INVALID_DOCUMENT && error.message[0], "Ordinary invalid archive returns its native typed cause");
    owner.reset();
    const auto end = std::find(std::begin(error.message), std::end(error.message), '\0');
    const std::vector<uint8_t> cause(error.message, end);
    auto payload = encodeFailurePayload({ FailureStatus::invalidDocument, cause }, {});
    Header request;
    request.identity.document[0] = 1;
    request.identity.request = 4;
    request.identity.revision = 2;
    request.payloadBytes = malformedArchive.size();
    auto response = request;
    response.kind = FrameKind::failure;
    response.payloadBytes = payload.size();
    auto wire = encodeFrame({ response, payload }, {});
    auto framed = decodeFrame(wire.data(), wire.size(), {});
    validateResponseIdentity(framed.header, request);
    auto decoded = decodeFailurePayload(framed.payload.data(), framed.payload.size(), {});
    wire.clear();
    payload.clear();
    framed.payload.clear();
    require(decoded.status == FailureStatus::invalidDocument && decoded.cause == cause, "Real native diagnostic survives framing, identity validation and owner release without replacement");
}
}
int main()
{
    try {
        wireAndOwnership();
        bounds();
        malformed();
        nativeCause();
        std::cout << "Worker failure payload contracts passed\n";
        return 0;
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}
