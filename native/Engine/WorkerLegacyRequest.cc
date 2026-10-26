// SPDX-License-Identifier: MIT
#include "WorkerLegacyRequest.h"
#include "WorkerWire.h"
#include <algorithm>
#include <limits>
namespace dust3d {
namespace worker {
    namespace {
        static_assert(maximumPayloadBytes <= std::numeric_limits<size_t>::max(), "Legacy request spans must fit size_t.");
        [[noreturn]] void reject(LegacyRequestErrorCode code, const char* cause) { throw LegacyRequestError(code, cause); }
        std::array<uint64_t, 14> fields(const LegacyRequestOptions& value)
        {
            return { value.maximumUvNativeBytes, value.maximumBakeNativeBytes, value.maximumEncodedImageBytes,
                value.maximumEncodedTotalBytes, value.maximumInputCopyBytes, value.maximumDecodedPixelBytes,
                value.maximumOutputCopyBytes, value.maximumOutputWireBytes, value.atlasTextureSize, value.colorSize,
                value.normalSize, value.materialSize, value.maximumSpatialGridEntries, value.maximumAssetNameBytes };
        }
        void validateLimits(const LegacyRequestLimits& limits)
        {
            if (!limits.maximumWireBytes || limits.maximumWireBytes > maximumPayloadBytes || !limits.maximumDecodedBytes || limits.maximumDecodedBytes > maximumPayloadBytes)
                reject(LegacyRequestErrorCode::invalidLimits, "Legacy request quotas are outside supported positive bounds.");
        }
        size_t plan(uint64_t archive, const LegacyRequestOptions& options, const LegacyRequestLimits& limits)
        {
            validateLegacyRequestOptions(options);
            if (!archive)
                reject(LegacyRequestErrorCode::invalidInput, "Legacy request archive is empty.");
            if (legacyRequestPrefixBytes > limits.maximumWireBytes || archive > limits.maximumWireBytes - legacyRequestPrefixBytes || archive > limits.maximumDecodedBytes)
                reject(LegacyRequestErrorCode::limitExceeded, "Legacy request exceeds its wire or retained archive quota.");
            return static_cast<size_t>(legacyRequestPrefixBytes + archive);
        }
        uint64_t tighten(uint64_t requested, uint64_t standard, uint64_t receiver) { return std::min(receiver, requested ? requested : standard); }
    }
    LegacyRequestError::LegacyRequestError(LegacyRequestErrorCode code, const char* cause)
        : std::runtime_error(cause)
        , m_code(code)
    {
    }
    void validateLegacyRequestOptions(const LegacyRequestOptions& options)
    {
        const auto values = fields(options);
        for (size_t index = 0; index < values.size(); ++index) {
            const uint64_t maximum = index >= 8 && index <= 11 ? 8192 : index == 12 ? 16000000
                                                                                    : maximumPayloadBytes;
            if (values[index] > maximum)
                reject(LegacyRequestErrorCode::invalidOptions, "Legacy request byte quota or bake setting exceeds its supported range.");
        }
    }
    LegacyProducerOptions applyLegacyRequestOptions(const LegacyRequestOptions& options, const LegacyProducerOptions& receiver)
    {
        validateLegacyProducerOptions(receiver);
        validateLegacyRequestOptions(options);
        const LegacyProducerOptions defaults;
        auto result = receiver;
        result.maximumUvNativeBytes = tighten(options.maximumUvNativeBytes, defaults.maximumUvNativeBytes, receiver.maximumUvNativeBytes);
        result.maximumBakeNativeBytes = tighten(options.maximumBakeNativeBytes, defaults.maximumBakeNativeBytes, receiver.maximumBakeNativeBytes);
        result.maximumDecodedImageBytes = tighten(options.maximumDecodedPixelBytes, defaults.maximumDecodedImageBytes, receiver.maximumDecodedImageBytes);
        result.images.maximumEncodedImageBytes = tighten(options.maximumEncodedImageBytes, defaults.images.maximumEncodedImageBytes, receiver.images.maximumEncodedImageBytes);
        result.images.maximumEncodedTotalBytes = tighten(options.maximumEncodedTotalBytes, defaults.images.maximumEncodedTotalBytes, receiver.images.maximumEncodedTotalBytes);
        result.images.maximumDecodedBytes = tighten(options.maximumInputCopyBytes, defaults.images.maximumDecodedBytes, receiver.images.maximumDecodedBytes);
        result.images.maximumAssetNameBytes = tighten(options.maximumAssetNameBytes, defaults.images.maximumAssetNameBytes, receiver.images.maximumAssetNameBytes);
        auto& output = result.output;
        output.maximumDecodedBytes = tighten(options.maximumOutputCopyBytes, defaults.output.maximumDecodedBytes, output.maximumDecodedBytes);
        output.maximumWireBytes = tighten(options.maximumOutputWireBytes, defaults.output.maximumWireBytes, output.maximumWireBytes);
        output.geometry.maximumDecodedBytes = std::min(output.geometry.maximumDecodedBytes, output.maximumDecodedBytes);
        output.geometry.maximumWireBytes = std::min(output.geometry.maximumWireBytes, output.maximumWireBytes);
        output.uv.maximumDecodedBytes = std::min(output.uv.maximumDecodedBytes, output.maximumDecodedBytes);
        output.uv.maximumWireBytes = std::min(output.uv.maximumWireBytes, output.maximumWireBytes);
        output.textures.maximumDecodedBytes = std::min(output.textures.maximumDecodedBytes, output.maximumDecodedBytes);
        output.textures.maximumPixelBytes = std::min(output.textures.maximumPixelBytes, output.maximumDecodedBytes);
        output.textures.maximumWireBytes = std::min(output.textures.maximumWireBytes, output.maximumWireBytes);
        result.atlasTextureSize = options.atlasTextureSize;
        result.colorSize = options.colorSize;
        result.normalSize = options.normalSize;
        result.materialSize = options.materialSize;
        result.maximumSpatialGridEntries = options.maximumSpatialGridEntries;
        validateLegacyProducerOptions(result);
        return result;
    }
    std::vector<uint8_t> encodeLegacyRequestView(const LegacyRequestView& value, const LegacyRequestLimits& limits)
    {
        validateLimits(limits);
        if (!value.archive)
            reject(LegacyRequestErrorCode::invalidInput, "Legacy request archive span is missing.");
        const auto size = plan(value.archiveBytes, value.options, limits);
        std::vector<uint8_t> bytes(size, 0);
        detail::writeUnsigned(bytes.data(), 1, 2);
        detail::writeUnsigned(bytes.data() + 2, legacyRequestPrefixBytes, 2);
        detail::writeUnsigned(bytes.data() + 8, value.archiveBytes, 8);
        const auto budgets = fields(value.options);
        for (size_t index = 0; index < budgets.size(); ++index)
            detail::writeUnsigned(bytes.data() + 16 + index * 8, budgets[index], 8);
        std::copy(value.archive, value.archive + value.archiveBytes, bytes.begin() + legacyRequestPrefixBytes);
        return bytes;
    }
    std::vector<uint8_t> encodeLegacyRequest(const LegacyRequest& value, const LegacyRequestLimits& limits)
    {
        return encodeLegacyRequestView({ value.archive.data(), value.archive.size(), value.options }, limits);
    }
    LegacyRequest decodeLegacyRequest(const uint8_t* bytes, size_t size, const LegacyRequestLimits& limits)
    {
        validateLimits(limits);
        if (!bytes)
            reject(LegacyRequestErrorCode::invalidInput, "Legacy request wire span is missing.");
        if (size < legacyRequestPrefixBytes)
            reject(LegacyRequestErrorCode::truncated, "Legacy request prefix is incomplete.");
        if (detail::readUnsigned(bytes, 2) != 1)
            reject(LegacyRequestErrorCode::unsupportedVersion, "Legacy request version is unsupported.");
        if (detail::readUnsigned(bytes + 2, 2) != legacyRequestPrefixBytes || detail::readUnsigned(bytes + 4, 4))
            reject(LegacyRequestErrorCode::invalidLayout, "Legacy request prefix/reserved fields are invalid.");
        std::array<uint64_t, 14> budgets;
        for (size_t index = 0; index < budgets.size(); ++index)
            budgets[index] = detail::readUnsigned(bytes + 16 + index * 8, 8);
        LegacyRequestOptions options { budgets[0], budgets[1], budgets[2], budgets[3], budgets[4], budgets[5], budgets[6], budgets[7], budgets[8], budgets[9], budgets[10], budgets[11], budgets[12], budgets[13] };
        const auto expected = plan(detail::readUnsigned(bytes + 8, 8), options, limits);
        if (size < expected)
            reject(LegacyRequestErrorCode::truncated, "Legacy request archive is incomplete.");
        if (size > expected)
            reject(LegacyRequestErrorCode::trailingData, "Legacy request contains trailing bytes.");
        return { { bytes + legacyRequestPrefixBytes, bytes + size }, options };
    }
}
}
