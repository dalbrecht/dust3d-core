// SPDX-License-Identifier: MIT
#include "WorkerSourceBakeRequest.h"
#include "WorkerWire.h"
#include <algorithm>
namespace dust3d {
namespace worker {
    namespace {
        void validate(const SourceBakeRequestOptions& value)
        {
            for (const auto quota : { value.maximumMaterialNativeBytes, value.maximumSamplerNativeBytes, value.maximumAtlasNativeBytes, value.maximumOutputNativeBytes,
                     value.maximumOutputCopyBytes, value.maximumOutputWireBytes })
                if (quota > maximumPayloadBytes)
                    throw LegacyRequestError(LegacyRequestErrorCode::invalidOptions, "Source request byte policy exceeds supported bounds.");
            if (value.maximumChartCount > 100000 || value.maximumRasterSamples > 1000000000 || value.shouldGenerateMips > 1 || value.maximumLevels > 14)
                throw LegacyRequestError(LegacyRequestErrorCode::invalidOptions, "Source request work settings exceed supported bounds.");
        }
        LegacyRequestLimits nestedLimits(const LegacyRequestLimits& limits)
        {
            if (limits.maximumWireBytes <= sourceBakeRequestPrefixBytes || limits.maximumWireBytes > maximumPayloadBytes
                || !limits.maximumDecodedBytes || limits.maximumDecodedBytes > maximumPayloadBytes)
                throw LegacyRequestError(LegacyRequestErrorCode::invalidLimits, "Source request envelope quotas are invalid.");
            return { limits.maximumWireBytes - sourceBakeRequestPrefixBytes, limits.maximumDecodedBytes };
        }
        uint64_t tighten(uint64_t requested, uint64_t standard, uint64_t host) { return std::min(host, requested ? requested : standard); }
    }
    std::vector<uint8_t> encodeSourceBakeRequestView(const LegacyRequestView& legacy, const SourceBakeRequestOptions& options, const LegacyRequestLimits& limits)
    {
        validate(options);
        auto nested = encodeLegacyRequestView(legacy, nestedLimits(limits));
        std::vector<uint8_t> bytes(sourceBakeRequestPrefixBytes + nested.size(), 0);
        detail::writeUnsigned(bytes.data(), 1, 2);
        detail::writeUnsigned(bytes.data() + 2, sourceBakeRequestPrefixBytes, 2);
        const auto& v = options;
        const std::array<uint64_t, 10> fields { v.maximumMaterialNativeBytes, v.maximumSamplerNativeBytes, v.maximumAtlasNativeBytes, v.maximumOutputNativeBytes,
            v.maximumChartCount, v.maximumRasterSamples, v.shouldGenerateMips, v.maximumLevels, v.maximumOutputCopyBytes, v.maximumOutputWireBytes };
        for (size_t index = 0; index < fields.size(); ++index)
            detail::writeUnsigned(bytes.data() + 16 + index * 8, fields[index], 8);
        std::copy(nested.begin(), nested.end(), bytes.begin() + sourceBakeRequestPrefixBytes);
        return bytes;
    }
    std::vector<uint8_t> encodeSourceBakeRequest(const SourceBakeRequest& value, const LegacyRequestLimits& limits)
    {
        return encodeSourceBakeRequestView({ value.legacy.archive.data(), value.legacy.archive.size(), value.legacy.options }, value.options, limits);
    }
    SourceBakeRequest decodeSourceBakeRequest(const uint8_t* bytes, size_t size, const LegacyRequestLimits& limits)
    {
        const auto innerLimits = nestedLimits(limits);
        if (!bytes || size < sourceBakeRequestPrefixBytes)
            throw LegacyRequestError(LegacyRequestErrorCode::truncated, "Source request prefix is incomplete.");
        if (detail::readUnsigned(bytes, 2) != 1)
            throw LegacyRequestError(LegacyRequestErrorCode::unsupportedVersion, "Source request version is unsupported.");
        if (detail::readUnsigned(bytes + 2, 2) != sourceBakeRequestPrefixBytes || detail::readUnsigned(bytes + 4, 4) || detail::readUnsigned(bytes + 8, 8))
            throw LegacyRequestError(LegacyRequestErrorCode::invalidLayout, "Source request prefix/reserved fields are invalid.");
        std::array<uint64_t, 10> fields;
        for (size_t index = 0; index < fields.size(); ++index)
            fields[index] = detail::readUnsigned(bytes + 16 + index * 8, 8);
        SourceBakeRequestOptions options { fields[0], fields[1], fields[2], fields[3], fields[4], fields[5], fields[6], fields[7], fields[8], fields[9] };
        validate(options);
        return { decodeLegacyRequest(bytes + sourceBakeRequestPrefixBytes, size - sourceBakeRequestPrefixBytes, innerLimits), options };
    }
    NativeSourceProducerOptions applySourceBakeRequestOptions(const SourceBakeRequest& value, const NativeSourceProducerOptions& receiver)
    {
        validate(value.options);
        validateNativeSourceProducerOptions(receiver);
        auto result = receiver;
        const NativeSourceProducerOptions defaults;
        result.preparation.procedural = applyLegacyRequestOptions(value.legacy.options, receiver.preparation.procedural);
        result.preparation.maximumEncodedImageBytes = tighten(value.legacy.options.maximumEncodedImageBytes, defaults.preparation.maximumEncodedImageBytes, receiver.preparation.maximumEncodedImageBytes);
        result.preparation.maximumEncodedTotalBytes = tighten(value.legacy.options.maximumEncodedTotalBytes, defaults.preparation.maximumEncodedTotalBytes, receiver.preparation.maximumEncodedTotalBytes);
        result.preparation.maximumDecodedImageBytes = tighten(value.legacy.options.maximumDecodedPixelBytes, defaults.preparation.maximumDecodedImageBytes, receiver.preparation.maximumDecodedImageBytes);
        const auto& v = value.options;
        result.maximumMaterialNativeBytes = tighten(v.maximumMaterialNativeBytes, defaults.maximumMaterialNativeBytes, receiver.maximumMaterialNativeBytes);
        result.maximumSamplerNativeBytes = tighten(v.maximumSamplerNativeBytes, defaults.maximumSamplerNativeBytes, receiver.maximumSamplerNativeBytes);
        result.maximumAtlasNativeBytes = tighten(v.maximumAtlasNativeBytes, defaults.maximumAtlasNativeBytes, receiver.maximumAtlasNativeBytes);
        result.maximumOutputNativeBytes = tighten(v.maximumOutputNativeBytes, defaults.maximumOutputNativeBytes, receiver.maximumOutputNativeBytes);
        result.output.maximumDecodedBytes = tighten(v.maximumOutputCopyBytes, defaults.output.maximumDecodedBytes, receiver.output.maximumDecodedBytes);
        result.output.maximumWireBytes = tighten(v.maximumOutputWireBytes, defaults.output.maximumWireBytes, receiver.output.maximumWireBytes);
        result.maximumChartCount = v.maximumChartCount;
        result.maximumRasterSamples = v.maximumRasterSamples;
        result.shouldGenerateMips = v.shouldGenerateMips != 0;
        result.maximumLevels = v.maximumLevels;
        validateNativeSourceProducerOptions(result);
        return result;
    }
}
}
