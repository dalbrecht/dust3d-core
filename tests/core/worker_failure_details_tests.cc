// SPDX-License-Identifier: MIT
#include <iostream>
#include <native/Engine/WorkerFailure.h>
#include <native/Engine/WorkerWire.h>
#include <stdexcept>
using namespace dust3d::worker;
namespace {
void require(bool value, const char* cause)
{
    if (!value)
        throw std::runtime_error(cause);
}
template <typename Action>
void reject(FailurePayloadErrorCode expected, Action action)
{
    try {
        action();
    } catch (const FailurePayloadError& cause) {
        require(cause.code() == expected && cause.what()[0], "Detailed failure cause remains typed");
        return;
    }
    throw std::runtime_error("Invalid detailed failure accepted");
}
}
int main()
{
    try {
        FailurePayload value { FailureStatus::unsupportedAsset, { 'a', 0, 255 } };
        const auto legacy = encodeFailurePayload(value, {});
        require(detail::readUnsigned(legacy.data(), 2) == 1 && !decodeFailurePayload(legacy.data(), legacy.size(), {}).detail,
            "Existing v1 errors retain their exact schema and absent details");
        value.detail = FailureDetail { FailureDomain::imageDecoder, -42, 3, 1 };
        auto bytes = encodeFailurePayload(value, {});
        require(detail::readUnsigned(bytes.data(), 2) == 2 && bytes.size() == detailedFailurePrefixBytes + value.cause.size(), "Details use explicit v2 prefix");
        const auto copy = decodeFailurePayload(bytes.data(), bytes.size(), {});
        require(copy.detail && copy.detail->domain == FailureDomain::imageDecoder && copy.detail->code == -42
                && copy.detail->status == 3 && copy.detail->causeTruncated == 1 && copy.cause == value.cause,
            "Original signed code, status, truncation and opaque cause bytes survive");
        require(encodeFailurePayload(copy, {}) == bytes, "Detailed failure bits round trip exactly");
        FailurePayloadLimits exact;
        exact.maximumWireBytes = bytes.size();
        exact.maximumCauseBytes = value.cause.size();
        require(encodeFailurePayload(copy, exact) == bytes, "Exact detailed error quota accepts");
        --exact.maximumWireBytes;
        reject(FailurePayloadErrorCode::limitExceeded, [&] { encodeFailurePayload(copy, exact); });
        for (uint16_t domain : { uint16_t(0), uint16_t(8) }) {
            auto invalid = value;
            invalid.detail->domain = static_cast<FailureDomain>(domain);
            reject(FailurePayloadErrorCode::invalidLayout, [&] { encodeFailurePayload(invalid, {}); });
        }
        auto invalid = value;
        invalid.detail->causeTruncated = 2;
        reject(FailurePayloadErrorCode::invalidLayout, [&] { encodeFailurePayload(invalid, {}); });
        for (size_t offset : { size_t(6), size_t(20) }) {
            auto wire = bytes;
            detail::writeUnsigned(wire.data() + offset, offset == 6 ? 8 : 2, offset == 6 ? 2 : 4);
            reject(FailurePayloadErrorCode::invalidLayout, [&] { decodeFailurePayload(wire.data(), wire.size(), {}); });
        }
        auto shortPrefix = bytes;
        shortPrefix.resize(detailedFailurePrefixBytes - 1);
        reject(FailurePayloadErrorCode::truncated, [&] { decodeFailurePayload(shortPrefix.data(), shortPrefix.size(), {}); });
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
    return 0;
}
