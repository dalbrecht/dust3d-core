// SPDX-License-Identifier: MIT
#include <iostream>
#include <native/Engine/WorkerSourceBakeRequest.h>
using namespace dust3d::worker;
int main()
{
    try {
        SourceBakeRequest request;
        request.legacy.archive = { 1, 2, 3 };
        request.legacy.options.colorSize = 8;
        request.options.maximumChartCount = 12;
        request.options.shouldGenerateMips = 1;
        request.options.maximumLevels = 3;
        auto bytes = encodeSourceBakeRequest(request, {});
        auto decoded = decodeSourceBakeRequest(bytes.data(), bytes.size(), {});
        auto policy = applySourceBakeRequestOptions(decoded, {});
        if (decoded.legacy.archive != request.legacy.archive || policy.maximumChartCount != 12 || !policy.shouldGenerateMips
            || policy.maximumLevels != 3 || policy.preparation.procedural.colorSize != 8)
            throw std::runtime_error("Source request retains exact archive and explicit bake settings");
        const auto expectCodecError = [&](const std::vector<uint8_t>& wire, LegacyRequestErrorCode expected) {
            bool rejected = false;
            try {
                (void)decodeSourceBakeRequest(wire.data(), wire.size(), {});
            } catch (const LegacyRequestError& failure) {
                rejected = failure.code() == expected;
            }
            if (!rejected)
                throw std::runtime_error("Source codec must reject framing with its typed cause before native work");
        };
        auto reserved = bytes;
        reserved[8] = 1;
        expectCodecError(reserved, LegacyRequestErrorCode::invalidLayout);
        auto truncated = bytes;
        truncated.pop_back();
        expectCodecError(truncated, LegacyRequestErrorCode::truncated);
        auto trailing = bytes;
        trailing.push_back(0);
        expectCodecError(trailing, LegacyRequestErrorCode::trailingData);
        bool envelopeCounted = false;
        try {
            (void)encodeSourceBakeRequest(request, { bytes.size() - 1, maximumPayloadBytes });
        } catch (const LegacyRequestError& failure) {
            envelopeCounted = failure.code() == LegacyRequestErrorCode::limitExceeded;
        }
        if (!envelopeCounted)
            throw std::runtime_error("Both source and legacy prefixes count toward the wire quota");
        for (auto member : { &SourceBakeRequestOptions::maximumMaterialNativeBytes, &SourceBakeRequestOptions::maximumSamplerNativeBytes,
                 &SourceBakeRequestOptions::maximumAtlasNativeBytes, &SourceBakeRequestOptions::maximumOutputNativeBytes,
                 &SourceBakeRequestOptions::maximumOutputCopyBytes, &SourceBakeRequestOptions::maximumOutputWireBytes }) {
            auto invalid = request;
            invalid.options.*member = maximumPayloadBytes + 1;
            bool rejected = false;
            try {
                (void)encodeSourceBakeRequest(invalid, {});
            } catch (const LegacyRequestError& cause) {
                rejected = cause.code() == LegacyRequestErrorCode::invalidOptions;
            }
            if (!rejected)
                throw std::runtime_error("Every source byte quota has an independently validated range");
        }
        for (auto pair : { std::pair<uint64_t SourceBakeRequestOptions::*, uint64_t> { &SourceBakeRequestOptions::maximumChartCount, 100001 },
                 { &SourceBakeRequestOptions::maximumRasterSamples, 1000000001 }, { &SourceBakeRequestOptions::shouldGenerateMips, 2 },
                 { &SourceBakeRequestOptions::maximumLevels, 15 } }) {
            auto invalid = request;
            invalid.options.*pair.first = pair.second;
            bool rejected = false;
            try {
                (void)encodeSourceBakeRequest(invalid, {});
            } catch (const LegacyRequestError& cause) {
                rejected = cause.code() == LegacyRequestErrorCode::invalidOptions;
            }
            if (!rejected)
                throw std::runtime_error("Source work settings reject outside supported bounds before native work");
        }
        auto version = bytes;
        version[1] = 2;
        expectCodecError(version, LegacyRequestErrorCode::unsupportedVersion);
        auto prefix = bytes;
        prefix[3] = 95;
        expectCodecError(prefix, LegacyRequestErrorCode::invalidLayout);
        expectCodecError(std::vector<uint8_t>(31), LegacyRequestErrorCode::truncated);
        for (const auto& invalid : { LegacyRequestLimits { 96, 1 }, LegacyRequestLimits { maximumPayloadBytes + 1, 1 },
                 LegacyRequestLimits { 1024, 0 }, LegacyRequestLimits { 1024, maximumPayloadBytes + 1 } }) {
            bool rejected = false;
            try {
                (void)encodeSourceBakeRequest(request, invalid);
            } catch (const LegacyRequestError& cause) {
                rejected = cause.code() == LegacyRequestErrorCode::invalidLimits;
            }
            if (!rejected)
                throw std::runtime_error("Source envelope validates independent input policies");
        }
        NativeSourceProducerOptions host;
        host.maximumSamplerNativeBytes = 32;
        host.maximumPermittedChartCount = 10;
        decoded.options.maximumChartCount = 0;
        if (applySourceBakeRequestOptions(decoded, host).maximumSamplerNativeBytes != 32)
            throw std::runtime_error("Request cannot expand receiver quotas");
        decoded.options.maximumChartCount = 11;
        bool rejected = false;
        try {
            (void)applySourceBakeRequestOptions(decoded, host);
        } catch (const LegacyProducerError&) {
            rejected = true;
        }
        if (!rejected)
            throw std::runtime_error("Host chart cap is independent from requested setting");
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
    return 0;
}
