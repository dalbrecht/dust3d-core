// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_LEGACY_REQUEST_H_
#define DUST3D_NATIVE_WORKER_LEGACY_REQUEST_H_
#include "WorkerLegacyProducer.h"
namespace dust3d {
namespace worker {
    constexpr size_t legacyRequestPrefixBytes = 128;
    /// Zero byte quotas select maintained defaults; positive byte quotas only tighten
    /// receiver admission. Dimensions/grid are explicit bake settings with native zero
    /// defaults, independently bounded by 8192/16000000, not inferred byte-budget caps.
    struct LegacyRequestOptions {
        uint64_t maximumUvNativeBytes = 0, maximumBakeNativeBytes = 0;
        uint64_t maximumEncodedImageBytes = 0, maximumEncodedTotalBytes = 0;
        uint64_t maximumInputCopyBytes = 0, maximumDecodedPixelBytes = 0;
        uint64_t maximumOutputCopyBytes = 0, maximumOutputWireBytes = 0;
        uint64_t atlasTextureSize = 0, colorSize = 0, normalSize = 0, materialSize = 0;
        uint64_t maximumSpatialGridEntries = 0, maximumAssetNameBytes = 0;
    };
    struct LegacyRequest {
        std::vector<uint8_t> archive;
        LegacyRequestOptions options;
    };
    struct LegacyRequestView {
        const uint8_t* archive;
        size_t archiveBytes;
        LegacyRequestOptions options;
    };
    /// Retained charge is archive bytes only; root/options/capacity/workspace/RSS excluded.
    struct LegacyRequestLimits {
        uint64_t maximumWireBytes = maximumPayloadBytes, maximumDecodedBytes = maximumPayloadBytes;
    };
    enum class LegacyRequestErrorCode { invalidInput,
        invalidLimits,
        invalidOptions,
        unsupportedVersion,
        invalidLayout,
        truncated,
        trailingData,
        limitExceeded };
    class LegacyRequestError : public std::runtime_error {
    public:
        LegacyRequestError(LegacyRequestErrorCode code, const char* cause);
        LegacyRequestErrorCode code() const noexcept { return m_code; }

    private:
        LegacyRequestErrorCode m_code;
    };
    /// Validate all option ranges without executing native code; throws LegacyRequestError.
    /// Example: validateLegacyRequestOptions({});
    void validateLegacyRequestOptions(const LegacyRequestOptions& options);
    /// Validate receiver/request and return tightened byte policies and explicit validated request bake settings.
    /// Throws original policy errors or LegacyRequestError. No allocation/native generation.
    /// Example: auto policy = applyLegacyRequestOptions(options, receiver);
    LegacyProducerOptions applyLegacyRequestOptions(const LegacyRequestOptions& options, const LegacyProducerOptions& receiver);
    /// Encode actual stable borrowed archive/options into independent explicit-endian bytes.
    /// Archive remains opaque; validates framing/quotas/options before output allocation.
    /// Throws LegacyRequestError or allocation errors. Example: auto bytes = encodeLegacyRequestView({data, size, options}, {});
    std::vector<uint8_t> encodeLegacyRequestView(const LegacyRequestView& value, const LegacyRequestLimits& limits);
    /// Owned-value equivalent of the borrowed encoder, with identical errors and bytes.
    /// Example: auto bytes = encodeLegacyRequest(request, {});
    std::vector<uint8_t> encodeLegacyRequest(const LegacyRequest& value, const LegacyRequestLimits& limits);
    /// Validate complete exact framing/options/quotas before copying opaque archive bytes.
    /// Input must remain readable/stable for the call; no native engine executes.
    /// Throws LegacyRequestError or allocation errors. Example: auto request = decodeLegacyRequest(bytes.data(), bytes.size(), {});
    LegacyRequest decodeLegacyRequest(const uint8_t* bytes, size_t size, const LegacyRequestLimits& limits);
}
}
#endif
