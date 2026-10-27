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
        void validateMetadata(uint64_t archive, uint64_t budget)
        {
            if (!archive)
                reject(SourceRequestErrorCode::invalidInput, "Source request archive is empty.");
            if (budget > maximumPayloadBytes)
                reject(SourceRequestErrorCode::invalidOptions, "Source request budget exceeds 256MiB.");
            if (archive > std::numeric_limits<size_t>::max() - sourceRequestPrefixBytes)
                reject(SourceRequestErrorCode::limitExceeded, "Source request archive length is not addressable.");
        }
        size_t validateSize(uint64_t archive, uint64_t budget, const SourceRequestLimits& limits)
        {
            validateMetadata(archive, budget);
            if (sourceRequestPrefixBytes > limits.maximumWireBytes || archive > limits.maximumWireBytes - sourceRequestPrefixBytes
                || archive > limits.maximumDecodedBytes)
                reject(SourceRequestErrorCode::limitExceeded, "Source request exceeds its wire or retained archive quota.");
            return static_cast<size_t>(sourceRequestPrefixBytes + archive);
        }
    }
    SourceRequestError::SourceRequestError(SourceRequestErrorCode code, const char* cause)
        : std::runtime_error(cause)
        , m_code(code)
    {
    }
    std::array<uint8_t, sourceRequestPrefixBytes> encodeSourceRequestPrefix(const SourceRequestMetadata& value)
    {
        validateMetadata(value.archiveBytes, value.maximumSourceBytes);
        std::array<uint8_t, sourceRequestPrefixBytes> bytes {};
        detail::writeUnsigned(bytes.data(), 1, 4);
        detail::writeUnsigned(bytes.data() + 8, value.maximumSourceBytes, 8);
        detail::writeUnsigned(bytes.data() + 16, value.archiveBytes, 8);
        return bytes;
    }
    SourceRequestMetadata inspectSourceRequestPrefix(const uint8_t* bytes, size_t size)
    {
        if (!bytes)
            reject(SourceRequestErrorCode::invalidInput, "Source request wire span is null.");
        if (size < sourceRequestPrefixBytes)
            reject(SourceRequestErrorCode::truncated, "Source request prefix is incomplete.");
        if (size > sourceRequestPrefixBytes)
            reject(SourceRequestErrorCode::trailingData, "Source request prefix contains trailing bytes.");
        if (detail::readUnsigned(bytes, 4) != 1)
            reject(SourceRequestErrorCode::unsupportedVersion, "Source request version is unsupported.");
        if (detail::readUnsigned(bytes + 4, 4))
            reject(SourceRequestErrorCode::invalidLayout, "Source request reserved field must be zero.");
        SourceRequestMetadata result { detail::readUnsigned(bytes + 16, 8), detail::readUnsigned(bytes + 8, 8) };
        validateMetadata(result.archiveBytes, result.maximumSourceBytes);
        return result;
    }
    std::vector<uint8_t> encodeSourceRequestView(const SourceRequestView& value, const SourceRequestLimits& limits)
    {
        validateLimits(limits);
        if (!value.archive)
            reject(SourceRequestErrorCode::invalidInput, "Source request archive span is null.");
        const auto size = validateSize(value.archiveBytes, value.maximumSourceBytes, limits);
        std::vector<uint8_t> bytes(size, 0);
        const auto prefix = encodeSourceRequestPrefix({ value.archiveBytes, value.maximumSourceBytes });
        std::copy(prefix.begin(), prefix.end(), bytes.begin());
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
        const auto metadata = inspectSourceRequestPrefix(bytes, sourceRequestPrefixBytes);
        const auto expected = validateSize(metadata.archiveBytes, metadata.maximumSourceBytes, limits);
        if (size < expected)
            reject(SourceRequestErrorCode::truncated, "Source request archive is incomplete.");
        if (size > expected)
            reject(SourceRequestErrorCode::trailingData, "Source request contains trailing bytes.");
        return { { bytes + sourceRequestPrefixBytes, bytes + size }, metadata.maximumSourceBytes };
    }
}
}
