// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_IMPORTED_REQUEST_H_
#define DUST3D_NATIVE_WORKER_IMPORTED_REQUEST_H_
#include "WorkerImportedProducer.h"
namespace dust3d {
namespace worker {
    constexpr size_t importedRequestPrefixBytes = 80;
    /// Zero selects maintained defaults; positive values up to256MiB only tighten receiver policy.
    struct ImportedRequestOptions {
        uint64_t maximumMaterialNativeBytes = 0, maximumSurfaceNativeBytes = 0;
        uint64_t maximumFactsCopyBytes = 0, maximumSurfaceCopyBytes = 0;
        uint64_t maximumEncodedImageBytes = 0, maximumDecodedPixelBytes = 0;
        uint64_t maximumAggregateDecodedBytes = 0, maximumOutputWireBytes = 0;
    };
    struct ImportedRequest {
        std::vector<uint8_t> archive;
        ImportedRequestOptions options;
    };
    struct ImportedRequestView {
        const uint8_t* archive;
        size_t archiveBytes;
        ImportedRequestOptions options;
    };
    /// Retained charge is archive bytes only; root/options/capacity/workspace/RSS excluded.
    struct ImportedRequestLimits {
        uint64_t maximumWireBytes = maximumPayloadBytes, maximumDecodedBytes = maximumPayloadBytes;
    };
    enum class ImportedRequestErrorCode { invalidInput,
        invalidLimits,
        invalidOptions,
        unsupportedVersion,
        invalidLayout,
        truncated,
        trailingData,
        limitExceeded };
    class ImportedRequestError : public std::runtime_error {
    public:
        ImportedRequestError(ImportedRequestErrorCode code, const char* cause);
        ImportedRequestErrorCode code() const noexcept { return m_code; }

    private:
        ImportedRequestErrorCode m_code;
    };
    /// Validate all option ranges without executing native code; throws ImportedRequestError.
    /// Example: validateImportedRequestOptions({});
    void validateImportedRequestOptions(const ImportedRequestOptions& options);
    /// Validate receiver/request and return a policy no more permissive than either.
    /// Throws original policy errors or ImportedRequestError. No allocation/native generation.
    /// Example: auto policy = applyImportedRequestOptions(options, receiver);
    NativeImportedProducerLimits applyImportedRequestOptions(const ImportedRequestOptions& options, const NativeImportedProducerLimits& receiver);
    /// Encode actual stable borrowed archive/options into independent explicit-endian bytes.
    /// Archive remains opaque; validates framing/quotas/options before output allocation.
    /// Throws ImportedRequestError or allocation errors. Example: auto bytes = encodeImportedRequestView({data, size, options}, {});
    std::vector<uint8_t> encodeImportedRequestView(const ImportedRequestView& value, const ImportedRequestLimits& limits);
    /// Owned-value equivalent of the borrowed encoder, with identical errors and bytes.
    /// Example: auto bytes = encodeImportedRequest(request, {});
    std::vector<uint8_t> encodeImportedRequest(const ImportedRequest& value, const ImportedRequestLimits& limits);
    /// Validate complete exact framing/options/quotas before copying opaque archive bytes.
    /// Input must remain readable/stable for the call; no native engine executes.
    /// Throws ImportedRequestError or allocation errors. Example: auto request = decodeImportedRequest(bytes.data(), bytes.size(), {});
    ImportedRequest decodeImportedRequest(const uint8_t* bytes, size_t size, const ImportedRequestLimits& limits);
}
}
#endif
