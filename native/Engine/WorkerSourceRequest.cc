// SPDX-License-Identifier: MIT
#include "WorkerSourceRequest.h"
#include "WorkerWire.h"
#include <algorithm>
#include <limits>
namespace dust3d {
namespace worker {
    namespace {
        static_assert(sizeof(size_t) <= sizeof(uint64_t), "Source request sizes must fit wire counts.");
        [[noreturn]] void reject(SourceRequestErrorCode code, const char* cause)
        {
            throw SourceRequestError(code, cause);
        }
        void validateLimits(const SourceRequestLimits& limits)
        {
            if (!limits.maximumWireBytes || limits.maximumWireBytes > maximumPayloadBytes
                || !limits.maximumDecodedBytes || limits.maximumDecodedBytes > maximumPayloadBytes)
                reject(SourceRequestErrorCode::invalidLimits, "Source request quotas are outside supported positive bounds.");
        }
        size_t validateSize(uint64_t archive, uint64_t budget, const SourceRequestLimits& limits)
        {
            if (!archive)
                reject(SourceRequestErrorCode::invalidInput, "Source request archive is empty.");
            if (budget > maximumPayloadBytes)
                reject(SourceRequestErrorCode::invalidOptions, "Source request budget exceeds 256MiB.");
            if (sourceRequestPrefixBytes > limits.maximumWireBytes || archive > limits.maximumWireBytes - sourceRequestPrefixBytes
                || archive > limits.maximumDecodedBytes || archive > std::numeric_limits<size_t>::max() - sourceRequestPrefixBytes)
                reject(SourceRequestErrorCode::limitExceeded, "Source request exceeds its wire or retained archive quota.");
            return static_cast<size_t>(sourceRequestPrefixBytes + archive);
        }
    }
    SourceRequestError::SourceRequestError(SourceRequestErrorCode code, const char* cause)
        : std::runtime_error(cause)
        , m_code(code)
    {
    }
    std::vector<uint8_t> encodeSourceRequestView(const SourceRequestView& value, const SourceRequestLimits& limits)
    {
        validateLimits(limits);
        if (!value.archive)
            reject(SourceRequestErrorCode::invalidInput, "Source request archive span is null.");
        const auto size = validateSize(value.archiveBytes, value.maximumSourceBytes, limits);
        std::vector<uint8_t> bytes(size, 0);
        detail::writeUnsigned(bytes.data(), 1, 4);
        detail::writeUnsigned(bytes.data() + 8, value.maximumSourceBytes, 8);
        detail::writeUnsigned(bytes.data() + 16, value.archiveBytes, 8);
        std::copy(value.archive, value.archive + value.archiveBytes, bytes.begin() + sourceRequestPrefixBytes);
        return bytes;
    }
    std::vector<uint8_t> encodeSourceRequest(const SourceRequest& value, const SourceRequestLimits& limits)
    {
        return encodeSourceRequestView({ value.archive.data(), value.archive.size(), value.maximumSourceBytes }, limits);
    }
    SourceRequest decodeSourceRequest(const uint8_t* bytes, size_t size, const SourceRequestLimits& limits)
    {
        validateLimits(limits);
        if (!bytes)
            reject(SourceRequestErrorCode::invalidInput, "Source request wire span is null.");
        if (size < sourceRequestPrefixBytes)
            reject(SourceRequestErrorCode::truncated, "Source request prefix is incomplete.");
        if (detail::readUnsigned(bytes, 4) != 1)
            reject(SourceRequestErrorCode::unsupportedVersion, "Source request version is unsupported.");
        if (detail::readUnsigned(bytes + 4, 4))
            reject(SourceRequestErrorCode::invalidLayout, "Source request reserved field must be zero.");
        const auto budget = detail::readUnsigned(bytes + 8, 8);
        const auto expected = validateSize(detail::readUnsigned(bytes + 16, 8), budget, limits);
        if (size < expected)
            reject(SourceRequestErrorCode::truncated, "Source request archive is incomplete.");
        if (size > expected)
            reject(SourceRequestErrorCode::trailingData, "Source request contains trailing bytes.");
        return { { bytes + sourceRequestPrefixBytes, bytes + size }, budget };
    }
}
}
