// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_SOURCE_REQUEST_H_
#define DUST3D_NATIVE_WORKER_SOURCE_REQUEST_H_
#include "WorkerProtocol.h"
namespace dust3d {
namespace worker {
    constexpr size_t sourceRequestPrefixBytes = 24;
    struct SourceRequest {
        std::vector<uint8_t> archive;
        uint64_t maximumSourceBytes = 0;
    };
    struct SourceRequestView {
        const uint8_t* archive;
        size_t archiveBytes;
        uint64_t maximumSourceBytes;
    };
    struct SourceRequestLimits {
        uint64_t maximumWireBytes = 128ULL * 1024 * 1024;
        uint64_t maximumDecodedBytes = 128ULL * 1024 * 1024;
    };
    enum class SourceRequestErrorCode { invalidInput,
        invalidLimits,
        invalidOptions,
        truncated,
        trailingData,
        unsupportedVersion,
        invalidLayout,
        limitExceeded };
    class SourceRequestError : public std::runtime_error {
    public:
        SourceRequestError(SourceRequestErrorCode code, const char* cause);
        SourceRequestErrorCode code() const noexcept { return m_code; }

    private:
        SourceRequestErrorCode m_code;
    };
    /// Copy opaque nonempty archive bytes into a version-one explicit-endian request.
    /// @param value Stable borrowed archive and source budget (zero selects native 64MiB default).
    /// @param limits Positive wire/retained archive quotas at most 256MiB, excluding workspace/RSS.
    /// @return Independent complete bytes; no archive parsing or generation.
    /// @throws SourceRequestError or allocation errors before partial output is published.
    /// Example: auto bytes = encodeSourceRequestView({ archive.data(), archive.size(), 0 }, {});
    std::vector<uint8_t> encodeSourceRequestView(const SourceRequestView& value, const SourceRequestLimits& limits);
    /// Owned-value equivalent of encodeSourceRequestView; borrows input only until return.
    /// @param value Opaque archive and bounded source budget, unchanged by encoding.
    /// @param limits Supported independent wire/archive-retention policy.
    /// @return Complete independent bytes; throws SourceRequestError or allocation errors.
    /// Example: auto bytes = encodeSourceRequest(request, {});
    std::vector<uint8_t> encodeSourceRequest(const SourceRequest& value, const SourceRequestLimits& limits);
    /// Validate complete framing/options/quotas before retaining archive bytes.
    /// @param bytes Stable exact readable wire span, borrowed for this call.
    /// @param size Exact count; trailing, truncated or unsupported fields reject.
    /// @param limits Supported wire/retained archive policy; not a generation memory limit.
    /// @return Independent archive and original budget setting; no native engine execution.
    /// @throws SourceRequestError or allocation errors; no partial result.
    /// Example: auto request = decodeSourceRequest(bytes.data(), bytes.size(), {});
    SourceRequest decodeSourceRequest(const uint8_t* bytes, size_t size, const SourceRequestLimits& limits);
}
}
#endif
